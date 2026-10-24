#include "MessageClient.h"
#include <esp_log.h>
#include <atomic>

constexpr const char* kTag = "MessageClient";

constexpr uint8_t kMessageTypeCommand = 1;
constexpr uint8_t kMessageTypeResponse = 2;

constexpr uint32_t kResponseTimeoutMs = 5000;
constexpr uint32_t kResponseCheckIntervalMs = 500;

uint16_t generateRequestId() {
    static std::atomic<uint16_t> requestIdCounter = 0;
    return ++requestIdCounter;
}

MessageClient::MessageClient() :
    mRequestCallback(nullptr),
    mStateChangedCallback(nullptr),
    mSocketClient(nullptr),
    mTaskRunning(false),
    mWasConnected(false),
    mWsMutex(nullptr),
    mIncomingMessageQueue(nullptr),
    mMessageProcessingTaskHandle(nullptr),
    mWsLoopTaskHandle(nullptr) {
}

MessageClient::~MessageClient() {
    end();
}

bool MessageClient::begin(const String& clientId, const String& host, uint16_t port, const String& authKey) {
    mWsMutex = xSemaphoreCreateMutex();
    if (!mWsMutex) {
        ESP_LOGE(kTag, "Failed to create WS Mutex");
        return false;
    }

    mSocketClient = new WebSocketsClient();
    mSocketClient->begin(host.c_str(), port, "/ws");
    mSocketClient->onEvent(std::bind(&MessageClient::handleWebSocketEvent, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    mSocketClient->setAuthorization(authKey.c_str());
    mSocketClient->setReconnectInterval(5000);
    mSocketClient->setExtraHeaders((String("X-Client-ID: ") + clientId).c_str());
    
    mTaskRunning = true;
    
    mResponseTimeoutTicker.attach_ms(kResponseCheckIntervalMs, +[](MessageClient* instance) {
        instance->checkForResponseTimeouts();
    }, this);

    mIncomingMessageQueue = xQueueCreate(10, sizeof(MessageInfo*));
    if (mIncomingMessageQueue == nullptr) {
        ESP_LOGE(kTag, "Failed to create incoming message queue.");
        end();
        return false;
    }

    if (xTaskCreate(&MessageClient::processMessageTask, "ProcessMessageTask", 4096, this, 1, &mMessageProcessingTaskHandle) != pdPASS) {
        ESP_LOGE(kTag, "Failed to create incoming message processing task.");
        end();
        return false;
    }

    if (xTaskCreate(&MessageClient::webSocketLoopTask, "WsLoopTask", 4096, this, 1, &mWsLoopTaskHandle) != pdPASS) {
        ESP_LOGE(kTag, "Failed to create WebSocket loop task.");
        end();
        return false;
    }

    return true;
}

void MessageClient::end() {
    mResponseTimeoutTicker.detach();

    if (mTaskRunning) {
        mTaskRunning = false;
        
        if (mIncomingMessageQueue) {
            MessageInfo* exitSignal = nullptr;
            xQueueSend(mIncomingMessageQueue, &exitSignal, pdMS_TO_TICKS(100));
        }
        
        while (mMessageProcessingTaskHandle || mWsLoopTaskHandle) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        if (mIncomingMessageQueue) {
            MessageInfo* msg = nullptr;
            while (xQueueReceive(mIncomingMessageQueue, &msg, 0) == pdTRUE) {
                if (msg) {
                    delete[] msg->payload;
                    delete msg;
                }
            }
            vQueueDelete(mIncomingMessageQueue);
            mIncomingMessageQueue = nullptr;
        }
    }

    if (mSocketClient) {
        mSocketClient->disconnect();
        delete mSocketClient;
        mSocketClient = nullptr;
    }

    if (mWsMutex) {
        vSemaphoreDelete(mWsMutex);
        mWsMutex = nullptr;
    }
}

void MessageClient::webSocketLoopTask(void* pvParameters) {
    MessageClient* self = static_cast<MessageClient*>(pvParameters);
    while (self->mTaskRunning) {
        if (self->mSocketClient) {
            if (xSemaphoreTake(self->mWsMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                self->mSocketClient->loop();
                xSemaphoreGive(self->mWsMutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    self->mWsLoopTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

void MessageClient::processMessageTask(void* pvParameters) {
    MessageClient* self = static_cast<MessageClient*>(pvParameters);
    while (self->mTaskRunning) {
        MessageInfo* messageInfo = nullptr;
        if (xQueueReceive(self->mIncomingMessageQueue, &messageInfo, portMAX_DELAY) == pdTRUE) {
            if (!messageInfo) {
                break;
            }

            switch (messageInfo->messageType)
            {
            case kMessageTypeCommand: {
                if (!self->mRequestCallback) break;
                if (messageInfo->payloadLength < 1) break; 

                uint8_t commandId = static_cast<uint8_t>(messageInfo->payload[0]);
                self->mRequestCallback(commandId, messageInfo->payloadLength - 1, messageInfo->payload + 1, messageInfo->requestId);
                break;
            }
            case kMessageTypeResponse: {
                if (messageInfo->payloadLength < 1) break; 
                MessageResponseCallback responseCallback = nullptr;
                {
                    std::lock_guard<std::mutex> lock(self->mWaitingResponsesMutex);
                    auto it = self->mWaitingResponsesMap.find(messageInfo->requestId);
                    if (it != self->mWaitingResponsesMap.end()) {
                        responseCallback = it->second.responseCallback;
                        self->mWaitingResponsesMap.erase(it);
                    }
                }

                if (responseCallback) {
                    responseCallback(true, messageInfo->payload + 2);
                }
                break;
            }
            default:
                ESP_LOGW(kTag, "Received unknown message type: %d", messageInfo->messageType);
                break;
            }

            delete[] messageInfo->payload;
            delete messageInfo;
        }
    }

    self->mMessageProcessingTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

void MessageClient::handleWebSocketEvent(WStype_t type, uint8_t* payload, size_t len) {
    switch(type) {
        case WStype_DISCONNECTED:
            if (mWasConnected) {
                if (mStateChangedCallback) mStateChangedCallback(ClientState::kDisconnected);
            } else {
                if (mStateChangedCallback) mStateChangedCallback(ClientState::kHandshakeFailed);
            }
            mWasConnected = false;
            break;
            
        case WStype_CONNECTED:
            mWasConnected = true;
            if (mStateChangedCallback) mStateChangedCallback(ClientState::kConnected);
            break;
            
        case WStype_BIN: {
            if (len < kFixedHeaderLength) {
                ESP_LOGW(kTag, "Received message is too short to contain a valid header.");
                return;
            }

            uint16_t requestId = (static_cast<uint8_t>(payload[1]) << 8) | static_cast<uint8_t>(payload[2]);
            
            MessageInfo* messageInfo = new MessageInfo();
            messageInfo->requestId = requestId;
            messageInfo->messageType = static_cast<uint8_t>(payload[0]);
            messageInfo->payloadLength = len - kFixedHeaderLength;
            messageInfo->payload = new uint8_t[len];
            memcpy(messageInfo->payload, payload, len);

            if (xQueueSend(mIncomingMessageQueue, &messageInfo, pdMS_TO_TICKS(100)) != pdTRUE) {
                ESP_LOGW(kTag, "Failed to enqueue incoming message.");
                delete[] messageInfo->payload;
                delete messageInfo;
            }
            break;
        }
        case WStype_TEXT:
        case WStype_PING:
        case WStype_PONG:
            break;
    }
}

void MessageClient::checkForResponseTimeouts() {
    uint32_t currentTime = millis();
    std::vector<RequestInfo> timedOutRequests;

    {
        std::lock_guard<std::mutex> lock(mWaitingResponsesMutex);
        for (const auto& pair : mWaitingResponsesMap) {
            const RequestInfo& requestInfo = pair.second;
            if (currentTime - requestInfo.timestamp > kResponseTimeoutMs) {
                timedOutRequests.push_back(requestInfo);
            }
        }

        for (const auto& requestInfo : timedOutRequests) {
            mWaitingResponsesMap.erase(requestInfo.requestId);
        }
    }

    for (const auto& requestInfo : timedOutRequests) {
        if (requestInfo.responseCallback) {
            requestInfo.responseCallback(false, nullptr);
        }
    }
}

uint16_t MessageClient::sendRequest(uint8_t commandId, uint8_t* message, size_t length, MessageResponseCallback responseCallback, bool preAllocatedMessageHeader) {
    if (!message || length == 0) {
        return -1;
    }

    if (!mSocketClient || !mWasConnected) {
        if (responseCallback) responseCallback(false, nullptr);
        return -1;
    }

    uint16_t requestId = generateRequestId();
    uint8_t* payload = nullptr;
    
    if (preAllocatedMessageHeader) {
        payload = message;
    } else {
        payload = (uint8_t*)heap_caps_calloc(1, kFixedHeaderLength + length + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!payload) return -1;
    }

    payload[0] = kMessageTypeCommand; 
    payload[1] = static_cast<uint8_t>((requestId >> 8) & 0xFF);
    payload[2] = static_cast<uint8_t>(requestId & 0xFF); 
    payload[3] = commandId;

    if (!preAllocatedMessageHeader) {
        memcpy(payload + kFixedHeaderLength + 1, message, length);
    }

    if (responseCallback) {
        RequestInfo requestInfo;
        requestInfo.requestId = requestId;
        requestInfo.timestamp = millis();
        requestInfo.responseCallback = responseCallback;

        std::lock_guard<std::mutex> lock(mWaitingResponsesMutex);
        mWaitingResponsesMap[requestId] = requestInfo;
    }

    if (xSemaphoreTake(mWsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        mSocketClient->sendBIN(payload, kFixedHeaderLength + length + 1);
        xSemaphoreGive(mWsMutex);
    } else {
        ESP_LOGE(kTag, "Failed to take mutex for sendCommand");
    }
    
    if (!preAllocatedMessageHeader) {
        free(payload);
    }

    return requestId;
}

bool MessageClient::sendResponse(uint16_t requestId, uint8_t* response, size_t length, bool preAllocatedMessageHeader) {
    if (!response || length == 0) return false;
    if (!mSocketClient || !mWasConnected) return false;

    uint8_t* payload = nullptr;
    if (!preAllocatedMessageHeader) {
        payload = (uint8_t*)heap_caps_calloc(1, kFixedHeaderLength + length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!payload) return false;
    } else {
        payload = response;
    }

    payload[0] = kMessageTypeResponse;
    payload[1] = static_cast<uint8_t>((requestId >> 8) & 0xFF);
    payload[2] = static_cast<uint8_t>(requestId & 0xFF); 

    if (!preAllocatedMessageHeader) {
        memcpy(payload + kFixedHeaderLength, response, length);
        length += kFixedHeaderLength;
    }

    bool success = false;
    // Thread-safe Send
    if (xSemaphoreTake(mWsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        success = mSocketClient->sendBIN(payload, length);
        xSemaphoreGive(mWsMutex);
    }

    if (!preAllocatedMessageHeader) {
        free(payload);
    }

    return success;
}