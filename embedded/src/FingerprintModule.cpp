#include "FingerprintModule.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_heap_caps.h>

#include "BoardConfig.h"

constexpr const char* kTag = "FingerprintModule";

constexpr uint32_t kSensorBaudRate = 115200 * 2;
constexpr uint32_t kSensorPassword = 0x00000000;
constexpr uint8_t  kUartPortNumber = 1;
constexpr size_t   kMaxDataPayload = 256;

FingerprintModule::FingerprintModule():
    mSensorStateChangedCallback(nullptr),
    mSensorStateTaskHandle(nullptr),
    mSerial(kUartPortNumber),
    mFinger(nullptr),
    mIsSensorReady(false),
    mTouchSem(nullptr),
    mImageBuffer(nullptr) {
}

FingerprintModule::~FingerprintModule() {
    end();
}

void FingerprintModule::sensorStateCheckingTask(void* arg) {
    FingerprintModule* self = static_cast<FingerprintModule*>(arg);
    while (true) {
        if (xSemaphoreTake(self->mTouchSem, portMAX_DELAY) == pdTRUE) {
            vTaskDelay(pdMS_TO_TICKS(40));

            bool isFingerPresent = digitalRead(BoardConfig::kFingerprintSensorWakeupPin) == HIGH;
            if (isFingerPresent) {
                ESP_LOGI(kTag, "Finger detected on the sensor.");
                self->mFinger->LEDcontrol(true);
            } else {
                ESP_LOGI(kTag, "Finger removed from the sensor.");
                self->mFinger->LEDcontrol(false);
            }

            if (self->mSensorStateChangedCallback) {
                self->mSensorStateChangedCallback(isFingerPresent);
            }

            xSemaphoreTake(self->mTouchSem, 0);
        }
    }
}

void FingerprintModule::end() {
    detachInterrupt(digitalPinToInterrupt(BoardConfig::kFingerprintSensorWakeupPin));

    if (mSensorStateTaskHandle) {
        vTaskDelete(mSensorStateTaskHandle);
        mSensorStateTaskHandle = nullptr;
    }

    mIsSensorReady = false;

    if (mFinger) {
        delete mFinger;
        mFinger = nullptr;
        mSerial.end();
    }

    if (mTouchSem) {
        vSemaphoreDelete(mTouchSem);
        mTouchSem = nullptr;
    }

    if (mImageBuffer) {
        free(mImageBuffer);
        mImageBuffer = nullptr;
    }
}

bool FingerprintModule::begin() {
    mTouchSem = xSemaphoreCreateBinary();
    if (!mTouchSem) {
        ESP_LOGE(kTag, "Failed to create semaphore");
        return false;
    }

    mSerial.begin(kSensorBaudRate, SERIAL_8N1,
                  BoardConfig::kFingerprintRxPin,
                  BoardConfig::kFingerprintTxPin);

    mFinger = new Adafruit_Fingerprint(&mSerial, kSensorPassword);
    if (!mFinger) {
        ESP_LOGE(kTag, "Failed to create Adafruit_Fingerprint instance");
        return false;
    }

    mFinger->begin(kSensorBaudRate);
    if (!mFinger->verifyPassword()) {
        ESP_LOGE(kTag, "Fingerprint sensor not found or wrong password. Check wiring/baud rate.");
        return false;
    }
    mIsSensorReady = true;
    ESP_LOGI(kTag, "Fingerprint sensor found over UART.");

    pinMode(BoardConfig::kFingerprintSensorWakeupPin, INPUT_PULLDOWN);
    attachInterruptArg(digitalPinToInterrupt(BoardConfig::kFingerprintSensorWakeupPin), onSensorWakeupInterrupt, this, CHANGE);

    BaseType_t ret = xTaskCreate(sensorStateCheckingTask, "sensor_state_checking", 4096, this, 5, &mSensorStateTaskHandle);
    if (ret != pdPASS) {
        ESP_LOGE(kTag, "Failed to create sensor state checking task");
        return false;
    }


    // Turn off the LED initially
    mFinger->LEDcontrol(false);
    
    return true;
}

void IRAM_ATTR FingerprintModule::onSensorWakeupInterrupt(void* arg) {
    FingerprintModule* self = static_cast<FingerprintModule*>(arg);
    BaseType_t higherPriorityTaskWoken = pdFALSE;

    if (self->mTouchSem != nullptr) {
        xSemaphoreGiveFromISR(self->mTouchSem, &higherPriorityTaskWoken);
    }

    if (higherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

bool FingerprintModule::readExact(uint8_t* buffer, size_t length, uint32_t timeoutMs) {
    size_t received = 0;
    uint32_t start = millis();
    while (received < length) {
        int available = mSerial.available();
        if (available > 0) {
            size_t toRead = min(static_cast<size_t>(available), length - received);
            received += mSerial.readBytes(buffer + received, toRead);
        } else {
            if (millis() - start > timeoutMs) {
                return false;
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
    return true;
}

bool FingerprintModule::readDataPacket(uint8_t* payload, size_t payloadCapacity, size_t* payloadLength, uint8_t* pid, uint32_t timeoutMs) {
    uint8_t header[9];
    if (!readExact(header, sizeof(header), timeoutMs)) {
        ESP_LOGE(kTag, "Timeout reading data packet header.");
        return false;
    }

    if (header[0] != 0xEF || header[1] != 0x01) {
        ESP_LOGE(kTag, "Invalid packet header: 0x%02X 0x%02X", header[0], header[1]);
        return false;
    }
    if (header[2] != 0xFF || header[3] != 0xFF || header[4] != 0xFF || header[5] != 0xFF) {
        ESP_LOGE(kTag, "Invalid packet address.");
        return false;
    }

    uint8_t packetId = header[6];
    uint16_t packetLength = (static_cast<uint16_t>(header[7]) << 8) | header[8];
    if (packetLength < 2 || (packetLength - 2u) > payloadCapacity) {
        ESP_LOGE(kTag, "Invalid data packet length: %u", packetLength);
        return false;
    }

    size_t dataLen = packetLength - 2;
    if (!readExact(payload, dataLen, timeoutMs)) {
        ESP_LOGE(kTag, "Timeout reading data packet payload.");
        return false;
    }

    uint8_t checksumBytes[2];
    if (!readExact(checksumBytes, sizeof(checksumBytes), timeoutMs)) {
        ESP_LOGE(kTag, "Timeout reading data packet checksum.");
        return false;
    }

    uint16_t calculated = packetId + header[7] + header[8];
    for (size_t i = 0; i < dataLen; ++i) {
        calculated += payload[i];
    }
    uint16_t received = (static_cast<uint16_t>(checksumBytes[0]) << 8) | checksumBytes[1];
    if (received != calculated) {
        ESP_LOGE(kTag, "Checksum mismatch: received=0x%04X, calculated=0x%04X", received, calculated);
        return false;
    }

    *pid = packetId;
    *payloadLength = dataLen;
    return true;
}

bool FingerprintModule::takeImage() {
    if (!mIsSensorReady || !mFinger) {
        ESP_LOGE(kTag, "Cannot take image: sensor not ready.");
        return false;
    }

    uint8_t result = mFinger->getImage();
    switch (result) {
        case FINGERPRINT_OK:
            return true;
        case FINGERPRINT_NOFINGER:
            ESP_LOGW(kTag, "takeImage: no finger on sensor.");
            return false;
        case FINGERPRINT_PACKETRECIEVEERR:
            ESP_LOGE(kTag, "takeImage: communication error.");
            return false;
        case FINGERPRINT_IMAGEFAIL:
            ESP_LOGE(kTag, "takeImage: imaging error.");
            return false;
        default:
            ESP_LOGE(kTag, "takeImage: failed with code: 0x%02X", result);
            return false;
    }
}

const uint8_t* FingerprintModule::getNewestRawImage() {
    if (!mIsSensorReady || !mFinger) {
        ESP_LOGE(kTag, "getNewestRawImage: sensor not ready.");
        return nullptr;
    }

    while (mSerial.available()) {
        mSerial.read();
    }

    // PS_UpImage (0x0A): thư viện Adafruit không có hàm riêng nên dùng gói lệnh thô.
    uint8_t commandData[] = { 0x0A };
    Adafruit_Fingerprint_Packet commandPacket(FINGERPRINT_COMMANDPACKET, sizeof(commandData), commandData);
    mFinger->writeStructuredPacket(commandPacket);

    Adafruit_Fingerprint_Packet reply(FINGERPRINT_ACKPACKET, 0, nullptr);
    if (mFinger->getStructuredPacket(&reply, 2000) != FINGERPRINT_OK) {
        ESP_LOGE(kTag, "getNewestRawImage: Timeout/invalid PS_UpImage ACK.");
        return nullptr;
    }
    if (reply.type != FINGERPRINT_ACKPACKET || reply.data[0] != FINGERPRINT_OK) {
        ESP_LOGE(kTag, "getNewestRawImage: Sensor refused upload with code: 0x%02X", reply.data[0]);
        return nullptr;
    }

    constexpr size_t totalImageSize = kImageWidth * kImageHeight;
    constexpr size_t expectedPacketLength = totalImageSize / 2;  // 4 bit / pixel
    uint8_t* tempBuffer = (uint8_t*)heap_caps_calloc(totalImageSize, 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!tempBuffer) {
        tempBuffer = (uint8_t*)heap_caps_calloc(totalImageSize, 1, MALLOC_CAP_8BIT);
    }
    if (!tempBuffer) {
        ESP_LOGE(kTag, "getNewestRawImage: Failed to allocate image buffer.");
        return nullptr;
    }

    size_t bytesReceived = 0;
    uint8_t payload[kMaxDataPayload];
    bool isStreamComplete = false;

    while (!isStreamComplete && bytesReceived < expectedPacketLength) {
        size_t payloadLength = 0;
        uint8_t pid = 0;
        if (!readDataPacket(payload, sizeof(payload), &payloadLength, &pid, 2000)) {
            ESP_LOGE(kTag, "getNewestRawImage: Failed receiving image chunk.");
            free(tempBuffer);
            return nullptr;
        }

        if (bytesReceived + payloadLength > expectedPacketLength) {
            ESP_LOGE(kTag, "getNewestRawImage: Image stream exceeded buffer capacity.");
            free(tempBuffer);
            return nullptr;
        }

        memcpy(tempBuffer + bytesReceived, payload, payloadLength);
        bytesReceived += payloadLength;

        if (pid == 0x08) {
            isStreamComplete = true;
        }
    }

    if (bytesReceived != expectedPacketLength) {
        ESP_LOGE(kTag, "getNewestRawImage: Incomplete image data (%zu / %zu bytes).",
                 bytesReceived, expectedPacketLength);
        free(tempBuffer);
        return nullptr;
    }

    ESP_LOGI(kTag, "getNewestRawImage: Successfully downloaded raw image (%zu bytes).", totalImageSize);

    if (mImageBuffer) {
        free(mImageBuffer);
    }

    // Mở rộng 4 bit -> 8 bit / pixel (làm ngược từ cuối để không ghi đè dữ liệu chưa đọc).
    for (size_t i = expectedPacketLength; i > 0; --i) {
        size_t idx = i - 1;
        uint8_t b = tempBuffer[idx];
        tempBuffer[idx * 2]     = ((b >> 4) & 0x0F) * 17;
        tempBuffer[idx * 2 + 1] = (b & 0x0F) * 17;
    }
    mImageBuffer = tempBuffer;

    return mImageBuffer;
}