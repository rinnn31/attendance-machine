#ifndef MESSAGE_CLIENT_H
#define MESSAGE_CLIENT_H

#include <WebSocketsClient.h>
#include <WString.h>
#include <Ticker.h>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

enum class ClientState : uint8_t {
    kConnected,
    kDisconnected,
    kRejected,
    kServerUnavailable,
    kHandshakeFailed
};

using RequestReceivedCallback = std::function<void(const uint8_t commandId, const size_t payloadLength, const uint8_t* payload, const uint16_t requestId)>;
using ClientStateChangedCallback = std::function<void(ClientState state)>;
using MessageResponseCallback = std::function<void(bool success, const uint8_t* response)>;

struct RequestInfo {
    uint16_t requestId;
    uint32_t timestamp;
    MessageResponseCallback responseCallback = nullptr;
};

struct MessageInfo {
    uint16_t requestId;
    uint8_t messageType;
    uint8_t* payload;   
    size_t payloadLength;
};

class MessageClient {
public:
    static constexpr uint8_t kFixedHeaderLength = 3;
    static constexpr uint8_t kRequestHeaderLength = kFixedHeaderLength + 1;

    MessageClient();
    ~MessageClient();
    bool begin(const String& clientId, const String& host, uint16_t port);
    void end();
    bool isConnected() const {
        return mSocketClient && mSocketClient->isConnected();
    }

    uint16_t sendRequest(uint8_t commandId, uint8_t* message, size_t length, MessageResponseCallback responseCallback = nullptr, bool preAllocatedMessageHeader = false);
    bool sendResponse(uint16_t requestId, uint8_t* message, size_t length, bool preAllocatedMessageHeader = false);

    void onRequestReceived(RequestReceivedCallback callback) {
        mRequestCallback = callback;
    }
    void onStateChanged(ClientStateChangedCallback callback) {
        mStateChangedCallback = callback;
    }

private:
    void handleWebSocketEvent(WStype_t type, uint8_t* payload, size_t len);
    void checkForResponseTimeouts();
    static void processMessageTask(void* pvParameters);
    static void webSocketLoopTask(void* pvParameters); // Task mới để duy trì WebSocketsClient::loop()

    RequestReceivedCallback mRequestCallback;
    ClientStateChangedCallback mStateChangedCallback;

    std::atomic<bool> mTaskRunning;
    bool mWasConnected;

    WebSocketsClient* mSocketClient;
    SemaphoreHandle_t mWsMutex; // Mutex bảo vệ send/recv đồng thời

    QueueHandle_t mIncomingMessageQueue;
    TaskHandle_t mMessageProcessingTaskHandle;
    TaskHandle_t mWsLoopTaskHandle;

    // Request-response tracking
    std::unordered_map<uint16_t, RequestInfo> mWaitingResponsesMap;
    std::mutex mWaitingResponsesMutex;
    Ticker mResponseTimeoutTicker;
};

#endif