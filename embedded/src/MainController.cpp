#include "MainController.h"

#include <esp_log.h>
#include <esp_camera.h>

#include "ConfigService.h"
#include "BoardConfig.h"
#include "Display.h"

constexpr const char* kTag = "MainController";

constexpr uint32_t kWifiReconnectIntervalMs = 5000;

constexpr uint8_t kCollectFaceDataCommand = 1;
constexpr uint8_t kCollectFingerprintDataCommand = 2;
constexpr uint8_t kStopCollectingDataCommand = 3;

constexpr uint32_t kResultDisplayMs = 2000;
constexpr uint32_t kPostAttendanceCooldownMs = 500;
constexpr uint32_t kDataCollectionTimeoutMs = 30000;

constexpr const char* kNtpServer1 = "pool.ntp.org";
constexpr const char* kNtpServer2 = "time.google.com";
constexpr long kGmtOffsetSec = 7 * 3600;
constexpr int kDaylightOffsetSec = 0;

static inline bool isHoldingResult(uint32_t holdUntil) {
    return (int32_t)(holdUntil - millis()) > 0; 
}

MainController::MainController() :
    mMessageClient(nullptr),
    mFaceCamModule(nullptr),
    mFingerprintModule(nullptr),
    mRunningMode(DeviceState::kNormalState),
    mReady(false),
    mBusy(false),
    mDataCollectionRequestId(0),
    mAttendanceQueue(nullptr),
    mWorkerTaskHandle(nullptr),
    mAttendanceDoneSem(nullptr),
    mStartCollectionTimestamp(0) {
}

MainController::~MainController() {
    if (mWorkerTaskHandle) {
        vTaskDelete(mWorkerTaskHandle);
        mWorkerTaskHandle = nullptr;
    }
    if (mAttendanceQueue) {
        AttendanceEvent ev;
        while (xQueueReceive(mAttendanceQueue, &ev, 0) == pdTRUE) {
            if (ev.imageBuffer) {
                free(ev.imageBuffer);
            }
        }
        vQueueDelete(mAttendanceQueue);
        mAttendanceQueue = nullptr;
    }

    if (mAttendanceDoneSem) {
        vSemaphoreDelete(mAttendanceDoneSem);
        mAttendanceDoneSem = nullptr;
    }

    delete mMessageClient;
    delete mFaceCamModule;
    delete mFingerprintModule;

    mWifiReconnectTicker.detach();
}

void MainController::begin() {
    Display::getInstance().showBootMode(Display::BootType::Normal);

    DeviceConfiguration config = ConfigService::getInstance()->getDeviceConfig();

    mAttendanceQueue = xQueueCreate(1, sizeof(AttendanceEvent));
    if (!mAttendanceQueue) {
        ESP_LOGE(kTag, "Failed to create attendance event queue.");
        return;
    }

    mAttendanceDoneSem = xSemaphoreCreateBinary();
    if (!mAttendanceDoneSem) {
        ESP_LOGE(kTag, "Failed to create attendance done semaphore.");
        return;
    }

    BaseType_t ret = xTaskCreatePinnedToCore(
        MainController::attendanceWorkerTask,
        "AttendanceWorker",
        8192,
        this,
        3,
        &mWorkerTaskHandle,
        0
    );
    if (ret != pdPASS) {
        ESP_LOGE(kTag, "Failed to create attendance worker task.");
        return;
    }

    ret = xTaskCreatePinnedToCore(
        MainController::updateTask,
        "StatusDisplayUpdater",
        4096,
        this,
        1,
        nullptr,
        1
    );
    if (ret != pdPASS) {
        ESP_LOGE(kTag, "Failed to create status display updater task.");
        return;
    }

    if (config.wifiSsid.isEmpty()) {
        ESP_LOGE(kTag, "WiFi SSID is empty, please boot the device in web portal mode to configure WiFi.");
        return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.onEvent(std::bind(&MainController::handleWiFiEvent, this, std::placeholders::_1));
    WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());

    if (config.wsHost.isEmpty() || config.wsPort == 0) {
        ESP_LOGE(kTag, "WebSocket host or port is not configured. Please configure the device using the web portal.");
        return;
    }
    mMessageClient = new MessageClient();
    if (!mMessageClient->begin(BoardConfig::kDeviceSerial, config.wsHost, config.wsPort)) {
        ESP_LOGE(kTag, "Failed to initialize MessageClient.");
        return;
    }
    mMessageClient->onRequestReceived(std::bind(&MainController::handleCommandReceived, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
    mMessageClient->onStateChanged(std::bind(&MainController::handleMessageClientStateChange, this, std::placeholders::_1));

    bool faceCamReady = false, fingerprintReady = false;

    mFaceCamModule = new FaceCamModule();
    if (!(faceCamReady = mFaceCamModule->begin())) {
        ESP_LOGE(kTag, "Failed to initialize FaceCamModule.");
    }
    mFaceCamModule->onFacePresenceChanged(std::bind(&MainController::handleFacePresenceChanged, this, std::placeholders::_1));
    mFaceCamModule->onFaceStable(std::bind(&MainController::handleFaceStable, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

    mFingerprintModule = new FingerprintModule();
    if (!(fingerprintReady = mFingerprintModule->begin())) {
        ESP_LOGE(kTag, "Failed to initialize FingerprintModule.");
    }
    mFingerprintModule->onSensorStateChanged(std::bind(&MainController::handleFingerprintSensorStateChange, this, std::placeholders::_1));

    if (!faceCamReady && !fingerprintReady) {
        ESP_LOGE(kTag, "Both FaceCamModule and FingerprintModule could not be initialized. Please check the hardware connections.");
        return;
    }

    Display::getInstance().showIdle(Display::Status{}, "Đang kết nối...");
}

void MainController::updateTask(void* pvParameters) {
    MainController* self = static_cast<MainController*>(pvParameters);
    while (true) {
        if (self->mRunningMode != DeviceState::kNormalState) {
            uint32_t elapsed = millis() - self->mStartCollectionTimestamp;
            if (elapsed > kDataCollectionTimeoutMs) {
                ESP_LOGW(kTag, "Data collection timed out after %u ms.", elapsed);
                self->mRunningMode = DeviceState::kNormalState;
                self->mDataCollectionRequestId = 0;

                if (self->mFaceCamModule && self->mReady && !self->mBusy) {
                    self->mFaceCamModule->resetFaceTracking();
                    self->mFaceCamModule->toggleFaceDetection(true);
                }

                Display::getInstance().showIdle(self->buildStatus());
            }
        }

        Display::getInstance().updateStatus(self->buildStatus());

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void MainController::handleWiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            ESP_LOGI(kTag, "WiFi connected, IP address: %s", WiFi.localIP().toString().c_str());
            mWifiReconnectTicker.detach();

            configTime(kGmtOffsetSec, kDaylightOffsetSec, kNtpServer1, kNtpServer2);
            ESP_LOGI(kTag, "Time synchronized using NTP.");

            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            ESP_LOGW(kTag, "WiFi disconnected!");
            mWifiReconnectTicker.detach();
            mWifiReconnectTicker.once_ms(kWifiReconnectIntervalMs, []() {
                ESP_LOGI(kTag, "Attempting to reconnect to WiFi...");
                WiFi.reconnect();
            });
            break;
        default:
            break;
    }
}

void MainController::handleMessageClientStateChange(ClientState state) {
    if (state == ClientState::kConnected) {
        ESP_LOGI(kTag, "WebSocket connected to server.");
        mReady = true;
        if (mFaceCamModule && !mBusy) {
            mFaceCamModule->toggleFaceDetection(true);
        }

        Display::getInstance().showIdle(buildStatus());
    } else {
        ESP_LOGW(kTag, "WebSocket disconnected or error occurred. State: %d", static_cast<int>(state));
        mReady = false;
        if (mFaceCamModule) {
            mFaceCamModule->toggleFaceDetection(false);
        }

        Display::getInstance().showIdle(buildStatus(), "Đang kết nối...");
    }
}

void MainController::handleCommandReceived(const uint8_t commandId, const size_t payloadLength, const uint8_t* payload, const uint16_t requestId) {
    ESP_LOGI(kTag, "Received command: %d, payload length: %u", commandId, (unsigned int)payloadLength);
    switch (commandId) {
        case kCollectFaceDataCommand: {
            ESP_LOGI(kTag, "Collect face data command received.");
            if (mRunningMode != DeviceState::kNormalState || mBusy) {
                ESP_LOGW(kTag, "Device is already busy or in collection mode. Rejecting command.");
                sendDataCollectionResponse(requestId, false);
                return;
            }

            mStartCollectionTimestamp = millis();

            mRunningMode = DeviceState::kCollectingFaceDataState;
            mDataCollectionRequestId = requestId;

            if (mFaceCamModule) {
                mFaceCamModule->resetFaceTracking();
                mFaceCamModule->toggleFaceDetection(true);
            }

            Display::getInstance().showCollecting(Display::Method::Face);
            break;
        }

        case kCollectFingerprintDataCommand: {
            ESP_LOGI(kTag, "Collect fingerprint data command received.");
            if (mRunningMode != DeviceState::kNormalState || mBusy) {
                ESP_LOGW(kTag, "Device is already busy or in collection mode. Rejecting command.");
                sendDataCollectionResponse(requestId, false);
                return;
            }

            mStartCollectionTimestamp = millis();

            mRunningMode = DeviceState::kCollectingFingerprintDataState;
            mDataCollectionRequestId = requestId;

            if (mFaceCamModule) {
                mFaceCamModule->toggleFaceDetection(false);
            }

            Display::getInstance().showCollecting(Display::Method::Fingerprint);
            break;
        }

        case kStopCollectingDataCommand: {
            ESP_LOGI(kTag, "Stop collecting data command received.");
            mRunningMode = DeviceState::kNormalState;
            mDataCollectionRequestId = 0;

            if (mFaceCamModule && mReady && !mBusy) {
                mFaceCamModule->resetFaceTracking();
                mFaceCamModule->toggleFaceDetection(true);
            }

            uint8_t responsePayload[MessageClient::kFixedHeaderLength + 1];
            responsePayload[MessageClient::kFixedHeaderLength] = 1;
            mMessageClient->sendResponse(requestId, responsePayload, sizeof(responsePayload), true);

            Display::getInstance().showIdle(buildStatus());
            break;
        }

        default:
            ESP_LOGW(kTag, "Unknown command ID: %d", commandId);
            break;
    }
}

void MainController::handleFacePresenceChanged(bool isFacePresent) {
    if (mBusy) return;

    if (mRunningMode == DeviceState::kNormalState) {
        if (isFacePresent) {
            Display::getInstance().showCollecting(Display::Method::Face, "Đang nhận diện khuôn mặt. Vui lòng đứng yên...");
        } else {
            Display::getInstance().showIdle(buildStatus());
        }
    } else if (mRunningMode == DeviceState::kCollectingFaceDataState) {
        if (isFacePresent) {
            Display::getInstance().showCollecting(Display::Method::Face, "Đang chụp khuôn mặt. Vui lòng đứng yên... ");
        } else {
            Display::getInstance().showCollecting(Display::Method::Face, "Không phát hiện khuôn mặt. Vui lòng đứng trước camera...");
        }
    }

}

void MainController::handleFaceStable(const uint8_t* image, size_t imageSize, size_t width, size_t height) {
    if (!mReady || mBusy) return;
    if (mRunningMode != DeviceState::kNormalState && mRunningMode != DeviceState::kCollectingFaceDataState) {
        return;
    }

    uint8_t* imgCopy = (uint8_t*)heap_caps_malloc(imageSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!imgCopy) {
        ESP_LOGE(kTag, "Failed to allocate memory for face image copy.");
        return;
    }
    memcpy(imgCopy, image, imageSize);

    AttendanceEvent ev;
    ev.type = AttendanceEventType::kFaceDetected;
    ev.imageBuffer = imgCopy;
    ev.imageSize = imageSize;
    ev.width = width;
    ev.height = height;

    if (xQueueSend(mAttendanceQueue, &ev, 0) != pdTRUE) {
        free(imgCopy);
    }
}

void MainController::handleFingerprintSensorStateChange(bool isFingerDetected) {
    if (!isFingerDetected) {
        if (mBusy || isHoldingResult(mResultHoldUntilMs)) return;

        if (mRunningMode == DeviceState::kCollectingFingerprintDataState) {
            Display::getInstance().showCollecting(Display::Method::Fingerprint, "Vui lòng đặt ngón tay lên cảm biến...");
        } else if (mRunningMode == DeviceState::kNormalState) {
            Display::getInstance().showIdle(buildStatus());
        }
        return;
    }
    

    if (!mReady || mBusy) return;
    if (mRunningMode.load()!= DeviceState::kNormalState && mRunningMode.load() != DeviceState::kCollectingFingerprintDataState) {
        return;
    }

    Display::getInstance().showCollecting(Display::Method::Fingerprint, "Đang chụp ảnh vân tay...");

    mBusy = true;

    AttendanceEvent ev;
    ev.type = AttendanceEventType::kFingerprintDetected;
    ev.imageBuffer = nullptr;
    ev.imageSize = 0;
    ev.width = 0;
    ev.height = 0;

    if (xQueueSend(mAttendanceQueue, &ev, 0) != pdTRUE) {
        mBusy = false;
    }
}

Display::Status MainController::buildStatus() {
    Display::Status status;
    status.wifiConnected = WiFi.isConnected();
    status.serverConnected = mMessageClient ? mMessageClient->isConnected() : false;

    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char dateBuffer[11];
    strftime(dateBuffer, sizeof(dateBuffer), "%d/%m/%Y", &timeinfo);
    status.date = String(dateBuffer);

    char timeBuffer[9];
    strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo);
    status.time = String(timeBuffer);

    return status;
}

void MainController::attendanceWorkerTask(void* pvParameters) {
    MainController* self = static_cast<MainController*>(pvParameters);
    AttendanceEvent ev;

    while (true) {
        if (xQueueReceive(self->mAttendanceQueue, &ev, portMAX_DELAY) == pdTRUE) {
            self->mBusy = true;
            bool sentRequest = false;
            
            uint8_t* jpgBuf = nullptr;
            size_t jpgSize = 0;

            if (ev.type == AttendanceEventType::kFaceDetected) {
                if (fmt2jpg(ev.imageBuffer, ev.imageSize, ev.width, ev.height, PIXFORMAT_RGB565, 90, &jpgBuf, &jpgSize)) {
                    if (self->mRunningMode == DeviceState::kNormalState) {
                        sentRequest = self->sendAttendanceData(BiometricType::kFace, jpgBuf, jpgSize);
                    } else if (self->mRunningMode == DeviceState::kCollectingFaceDataState) {
                        self->sendDataCollectionResponse(self->mDataCollectionRequestId, true, jpgBuf, jpgSize);
                        self->mRunningMode = DeviceState::kNormalState;

                        // The person is still standing in front of the camera: don't let the same face
                        // trigger attendance as soon as detection is re-enabled. Wait until they leave.
                        if (self->mFaceCamModule) {
                            self->mFaceCamModule->requireFaceAbsence();
                        }
                    }

                    free(jpgBuf);
                } else {
                    ESP_LOGE(kTag, "Failed to compress face image to JPEG.");
                }

                if (ev.imageBuffer) {
                    free(ev.imageBuffer);
                }

            } else if (ev.type == AttendanceEventType::kFingerprintDetected) {

                // delay a bit to ensure the fingerprint sensor has enough time to capture the image
                vTaskDelay(pdMS_TO_TICKS(100));

                if (self->mFingerprintModule->takeImage()) {
                    const uint8_t* rawImage = self->mFingerprintModule->getNewestRawImage();
                    if (rawImage) {
                        if (fmt2jpg(const_cast<uint8_t*>(rawImage), FingerprintModule::kImageHeight * FingerprintModule::kImageWidth, FingerprintModule::kImageWidth, FingerprintModule::kImageHeight, PIXFORMAT_GRAYSCALE, 90, &jpgBuf, &jpgSize)) {
                            if (self->mRunningMode == DeviceState::kNormalState) {
                                sentRequest = self->sendAttendanceData(BiometricType::kFingerprint, jpgBuf, jpgSize);
                            } else if (self->mRunningMode == DeviceState::kCollectingFingerprintDataState) {
                                self->sendDataCollectionResponse(self->mDataCollectionRequestId, true, jpgBuf, jpgSize);
                                self->mRunningMode = DeviceState::kNormalState;
                            }
            
                            free(jpgBuf);
                        } else {
                            ESP_LOGE(kTag, "Failed to compress fingerprint image to JPEG.");
                        }

                    } else {
                        ESP_LOGE(kTag, "Failed to get the newest fingerprint image.");
                    }
                } else {
                    ESP_LOGE(kTag, "Failed to capture fingerprint image.");
                }
            }
            
            if (sentRequest) {
                ESP_LOGI(kTag, "Waiting for server response...");
                if (xSemaphoreTake(self->mAttendanceDoneSem, pdMS_TO_TICKS(5000)) == pdTRUE) {
                    ESP_LOGI(kTag, "Completed attendance turn successfully.");
                } else {
                    ESP_LOGW(kTag, "Timeout waiting for server ACK.");
                }
            }

            if (self->mFaceCamModule) {
                self->mFaceCamModule->toggleFaceDetection(false);
            }

            while (digitalRead(BoardConfig::kFingerprintSensorWakeupPin) == HIGH) {
                vTaskDelay(pdMS_TO_TICKS(50));
            }

            while (isHoldingResult(self->mResultHoldUntilMs)) {
                vTaskDelay(pdMS_TO_TICKS(50));
            }

            vTaskDelay(pdMS_TO_TICKS(kPostAttendanceCooldownMs));

            if (self->mFaceCamModule) {
                self->mFaceCamModule->resetFaceTracking();
                if (self->mReady && self->mRunningMode != DeviceState::kCollectingFingerprintDataState) {
                    self->mFaceCamModule->toggleFaceDetection(true);
                }
            }

            self->mBusy = false;

            Display::getInstance().showIdle(self->buildStatus());
        }
    }
}

bool MainController::sendAttendanceData(BiometricType type, const uint8_t* imageData, size_t dataSize) {
    if (!mMessageClient || !mReady) {
        ESP_LOGW(kTag, "Cannot send attendance data: MessageClient not ready.");
        return false;
    }

    time_t now;
    time(&now);

    ESP_LOGI(kTag, "Sending attendance data. Type: %s, Image size: %zu bytes, Timestamp: %ld", 
             type == BiometricType::kFace ? "Face" : "Fingerprint", dataSize, now);

    // 4 bytes for preallocated header, 1 byte for BiometricType, 8 bytes for timestamp, and the rest for image data
    size_t payloadSize = MessageClient::kRequestHeaderLength + 1 + 1 + sizeof(now) + dataSize; 
    uint8_t* payload = (uint8_t*)heap_caps_calloc(1, payloadSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!payload) {
        ESP_LOGE(kTag, "Failed to allocate memory for attendance data payload.");
        return false;
    }

    payload[MessageClient::kRequestHeaderLength] = static_cast<uint8_t>(type);
    memcpy(payload + MessageClient::kRequestHeaderLength + 1, &now, sizeof(now));
    memcpy(payload + MessageClient::kRequestHeaderLength + 1 + sizeof(now), imageData, dataSize);

    xSemaphoreTake(mAttendanceDoneSem, 0);

    Display::getInstance().showProcessing(type == BiometricType::kFace ? Display::Method::Face : Display::Method::Fingerprint);

    uint16_t reqId = mMessageClient->sendRequest(1, payload, payloadSize - MessageClient::kRequestHeaderLength - 1, [this, payload](bool success, const uint8_t* response) {
        if (success) {
            ESP_LOGI(kTag, "Got server response for attendance data.");

            uint8_t status = response[0];
            ESP_LOGI(kTag, "Server response status: %d", status);
            mResultHoldUntilMs = millis() + kResultDisplayMs;
            if (status == 1) {
                String fullName(reinterpret_cast<const char*>(response + 1));
                Display::getInstance().showResult(Display::ResultType::Success, "Chấm công thành công", kResultDisplayMs, fullName);
            } else {
                Display::getInstance().showResult(Display::ResultType::Failed, "Chấm công thất bại", kResultDisplayMs);
            }

        } else {
            Display::getInstance().showResult(Display::ResultType::Failed, "Chấm công thất bại");
            ESP_LOGW(kTag, "Failed to send attendance data or response timed out.");
        }
        free(payload);
        if (mAttendanceDoneSem) {
            xSemaphoreGive(mAttendanceDoneSem);
        }
    }, true);

    return (reqId != static_cast<uint16_t>(-1));
}

void MainController::sendDataCollectionResponse(uint16_t requestId, bool success, const uint8_t* data, size_t dataSize) {
    if (!mMessageClient || !mReady) {
        ESP_LOGW(kTag, "Cannot send data collection response: MessageClient not ready.");
        return;
    }

    Display::getInstance().showProcessing(mRunningMode == DeviceState::kCollectingFaceDataState ? Display::Method::Face : Display::Method::Fingerprint, "Đang gửi dữ liệu đến máy chủ...");

    size_t payloadSize = MessageClient::kFixedHeaderLength + 1 + dataSize;
    uint8_t* payload = (uint8_t*)heap_caps_calloc(1, payloadSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!payload) {
        ESP_LOGE(kTag, "Failed to allocate memory for data collection response payload.");
        return;
    }


    payload[MessageClient::kFixedHeaderLength] = success ? 1 : 0;
    if (data && dataSize > 0) {
        memcpy(payload + MessageClient::kFixedHeaderLength + 1, data, dataSize);
    }

    mMessageClient->sendResponse(requestId, payload, payloadSize, true);
    free(payload);
}