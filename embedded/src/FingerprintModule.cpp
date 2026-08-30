#include "FingerprintModule.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

#include "BoardConfig.h"

constexpr const char* kTag = "FingerprintModule";

constexpr uint32_t kStateCheckIntervalMs = 100;
constexpr uint32_t kTransferBufferSize = 512;

FingerprintModule::FingerprintModule(): 
    mSensorStateChangedCallback(nullptr),
    mSensorStateTaskHandle(nullptr),
    mClientHandle(nullptr),
    mDeviceHandle(nullptr),
    mHostTaskHandle(nullptr),
    mIsDeviceConnected(false),
    mOutTransfer(nullptr), 
    mInTransfer(nullptr), 
    mTransferDoneSem(nullptr),
    mTouchSem(nullptr),
    mImageBuffer(nullptr) {
}

FingerprintModule::~FingerprintModule() {
    end();
}

void FingerprintModule::usbEventTask(void* arg) {
    FingerprintModule* self = static_cast<FingerprintModule*>(arg);
    while (true) {
        usb_host_client_handle_events(self->mClientHandle, portMAX_DELAY);
    }
}

void FingerprintModule::sensorStateCheckingTask(void* arg) {
    FingerprintModule* self = static_cast<FingerprintModule*>(arg);
    while (true) {
        if (xSemaphoreTake(self->mTouchSem, portMAX_DELAY) == pdTRUE) {
            // Debounce delay to avoid false triggers
            vTaskDelay(pdMS_TO_TICKS(40));

            bool isFingerPresent = digitalRead(BoardConfig::kFingerprintSensorWakeupPin) == HIGH;
            if (isFingerPresent) {
                ESP_LOGI(kTag, "Finger detected on the sensor.");
            } else {
                ESP_LOGI(kTag, "Finger removed from the sensor.");
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

    if (mHostTaskHandle) {
        vTaskDelete(mHostTaskHandle);
        mHostTaskHandle = nullptr;
    }

    if (mOutTransfer) {
        usb_host_transfer_free(mOutTransfer);
        mOutTransfer = nullptr;
    }
    if (mInTransfer) {
        usb_host_transfer_free(mInTransfer);
        mInTransfer = nullptr;
    }
    if (mClientHandle) {
        if (mDeviceHandle) {
            usb_host_interface_release(mClientHandle, mDeviceHandle, 0);
            usb_host_device_close(mClientHandle, mDeviceHandle);
            mDeviceHandle = nullptr;
        }
        usb_host_client_deregister(mClientHandle);
        mClientHandle = nullptr;
    }

    if (mTransferDoneSem) {
        vSemaphoreDelete(mTransferDoneSem);
        mTransferDoneSem = nullptr;
    }
    if (mTouchSem) {
        vSemaphoreDelete(mTouchSem);
        mTouchSem = nullptr;
    }

    if (mImageBuffer) {
        free(mImageBuffer);
        mImageBuffer = nullptr;
    }

    if (mSensorStateTaskHandle) {
        vTaskDelete(mSensorStateTaskHandle);
        mSensorStateTaskHandle = nullptr;
    }
}

bool FingerprintModule::begin() {
    mTransferDoneSem = xSemaphoreCreateBinary();
    mTouchSem = xSemaphoreCreateBinary();
    if (!mTransferDoneSem || !mTouchSem) {
        ESP_LOGE(kTag, "Failed to create semaphores");
        return false;
    }

    const usb_host_config_t hostConfig = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1
    };
    esp_err_t err = usb_host_install(&hostConfig);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(kTag, "usb_host_install failed: 0x%x", err);
        return false;
    }

    const usb_host_client_config_t clientConfig = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {
            .client_event_callback = [](const usb_host_client_event_msg_t *event_msg, void *arg) {
                FingerprintModule* self = static_cast<FingerprintModule*>(arg);
                self->handleClientEvent(event_msg, arg);
            },
            .callback_arg = this
        }
    };
    err = usb_host_client_register(&clientConfig, &mClientHandle);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "usb_host_client_register failed: 0x%x", err);
        return false;
    }

    err = usb_host_transfer_alloc(kTransferBufferSize, 0, &mOutTransfer);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to allocate OUT transfer: 0x%x", err);
        return false;
    }
    err = usb_host_transfer_alloc(kTransferBufferSize, 0, &mInTransfer);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to allocate IN transfer: 0x%x", err);
        return false;
    }

    mOutTransfer->callback = [](usb_transfer_t *transfer) {
        FingerprintModule* self = static_cast<FingerprintModule*>(transfer->context);
        xSemaphoreGive(self->mTransferDoneSem);
    };
    mOutTransfer->context = this;
    mInTransfer->callback = [](usb_transfer_t *transfer) {
        FingerprintModule* self = static_cast<FingerprintModule*>(transfer->context);
        xSemaphoreGive(self->mTransferDoneSem);
    };
    mInTransfer->context = this;

    BaseType_t ret = xTaskCreate(usbEventTask, "usb_events", 4096, this, 5, &mHostTaskHandle);
    if (ret != pdPASS) {
        ESP_LOGE(kTag, "Failed to create USB event handling task");
        return false;
    }

    // Create interrupt task for sensor state checking
    pinMode(BoardConfig::kFingerprintSensorWakeupPin, INPUT_PULLDOWN);
    attachInterruptArg(digitalPinToInterrupt(BoardConfig::kFingerprintSensorWakeupPin), onSensorWakeupInterrupt, this, CHANGE);

    ret = xTaskCreate(sensorStateCheckingTask, "sensor_state_checking", 4096, this, 5, &mSensorStateTaskHandle);
    if (ret != pdPASS) {
        ESP_LOGE(kTag, "Failed to create sensor state checking task");
        return false;
    }

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

void FingerprintModule::handleClientEvent(const usb_host_client_event_msg_t *event_msg, void *arg) {
    if (event_msg->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
        ESP_LOGI(kTag, "Openning connection to fingerprint sensor with address: %d", event_msg->new_dev.address);
        esp_err_t err = usb_host_device_open(mClientHandle, event_msg->new_dev.address, &mDeviceHandle);
        if (err == ESP_OK) {
            err = usb_host_interface_claim(mClientHandle, mDeviceHandle, 0, 0);
            if (err != ESP_OK) {
                ESP_LOGE(kTag, "Failed to claim interface: 0x%x", err);
                usb_host_device_close(mClientHandle, mDeviceHandle);
                mDeviceHandle = nullptr;
            } else {
                // Get the endpoint addresses from the interface descriptor
                const usb_config_desc_t* config = nullptr;
                usb_host_get_active_config_descriptor(mDeviceHandle, &config);
                
                const usb_intf_desc_t* interfaceDesc = usb_parse_interface_descriptor(config, 0, 0, nullptr);
                for (int i = 0; i < interfaceDesc->bNumEndpoints; ++i) {
                    const usb_ep_desc_t* epDesc = usb_parse_endpoint_descriptor_by_index(interfaceDesc, i, config->wTotalLength, nullptr);
                    if (epDesc) {
                        bool isIn = (epDesc->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK);
                        bool isBulk = ((epDesc->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK) == USB_BM_ATTRIBUTES_XFER_BULK);
                        if (isBulk && isIn)  mBulkInEp  = epDesc->bEndpointAddress;
                        if (isBulk && !isIn) mBulkOutEp = epDesc->bEndpointAddress;
                    }
                }

                mIsDeviceConnected = true;
                ESP_LOGI(kTag, "AS608 fingerprint sensor connected and interface claimed.");
            }
        }

    } else if (event_msg->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
        ESP_LOGW(kTag, "Fingerprint sensor disconnected.");
        mIsDeviceConnected = false;
        if (mDeviceHandle) {
            usb_host_interface_release(mClientHandle, mDeviceHandle, 0);
            usb_host_device_close(mClientHandle, mDeviceHandle);
            mDeviceHandle = nullptr;
        }
    }
}

bool FingerprintModule::sendPacket(const uint8_t* data, size_t length) {
    if (!mIsDeviceConnected || !mOutTransfer || !mDeviceHandle) {
        ESP_LOGE(kTag, "Cannot send packet: device not connected or transfer not initialized.");
        return false;
    }
    if (length > kTransferBufferSize) {
        ESP_LOGE(kTag, "Data length exceeds transfer buffer size.");
        return false;
    }

    memcpy(mOutTransfer->data_buffer, data, length);
    mOutTransfer->num_bytes = length;
    mOutTransfer->device_handle = mDeviceHandle;
    mOutTransfer->bEndpointAddress = mBulkOutEp;

    esp_err_t err = usb_host_transfer_submit(mOutTransfer);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to submit OUT transfer: 0x%x", err);
        return false;
    }

    if (xSemaphoreTake(mTransferDoneSem, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(kTag, "Timeout waiting for OUT transfer to complete.");

        // Attempt to halt and flush the endpoint to recover from potential deadlock
        usb_host_endpoint_halt(mDeviceHandle, mBulkOutEp);
        usb_host_endpoint_flush(mDeviceHandle, mBulkOutEp);
        xSemaphoreTake(mTransferDoneSem, 0); // Drain the semaphore to avoid deadlock in future transfers

        return false;
    }
    
    return mOutTransfer->status == USB_TRANSFER_STATUS_COMPLETED;
}

bool FingerprintModule::receivePacket(uint8_t* buffer, size_t bufferSize, size_t* actualLength, uint32_t timeoutMs) {
    if (!mIsDeviceConnected || !mInTransfer || !mDeviceHandle) {
        ESP_LOGE(kTag, "Cannot receive packet: device not connected or transfer not initialized.");
        return false;
    }

    mInTransfer->num_bytes = bufferSize > kTransferBufferSize ? kTransferBufferSize : bufferSize;
    mInTransfer->bEndpointAddress = mBulkInEp;
    mInTransfer->device_handle = mDeviceHandle;

    esp_err_t err = usb_host_transfer_submit(mInTransfer);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to submit IN transfer: 0x%x", err);
        return false;
    }

    if (xSemaphoreTake(mTransferDoneSem, pdMS_TO_TICKS(timeoutMs)) != pdTRUE) {
        ESP_LOGE(kTag, "Timeout waiting for IN transfer to complete.");

        // Attempt to halt and flush the endpoint to recover from potential deadlock
        usb_host_endpoint_halt(mDeviceHandle, mBulkInEp);
        usb_host_endpoint_flush(mDeviceHandle, mBulkInEp);  
        xSemaphoreTake(mTransferDoneSem, 0); // Drain the semaphore to avoid deadlock in future transfers

        return false;
    }

    if (mInTransfer->status != USB_TRANSFER_STATUS_COMPLETED) {
        ESP_LOGE(kTag, "IN transfer failed with status: %d", mInTransfer->status);
        return false;
    }

    memcpy(buffer, mInTransfer->data_buffer, mInTransfer->actual_num_bytes);   
    if (actualLength) {
        *actualLength = mInTransfer->actual_num_bytes;
    }
    return true;
}

bool FingerprintModule::checkPacket(
    const char* ctx,
    const uint8_t* packet, 
    size_t length, 
    size_t expectedLength
) {
    if (!packet) {
        ESP_LOGE(kTag, "%s: Packet buffer is null.", ctx);
        return false;
    }

    // Check if the received length is at least the minimum expected length
    if (length < 12 || length < expectedLength) {
        ESP_LOGE(kTag, "%s: Received length (%zu) is less than expected (%zu).", 
                 ctx, length, expectedLength);
        return false;
    }

    if (packet[0] != 0xEF || packet[1] != 0x01) {
        ESP_LOGE(kTag, "%s: Invalid packet header: 0x%02X 0x%02X", 
                 ctx, packet[0], packet[1]);
        return false;
    }

    if (packet[2] != 0xFF || packet[3] != 0xFF || packet[4] != 0xFF || packet[5] != 0xFF) {
        ESP_LOGE(kTag, "%s: Invalid packet address: 0x%02X 0x%02X 0x%02X 0x%02X", 
                 ctx, packet[2], packet[3], packet[4], packet[5]);
        return false;
    }

    // Check if the PID is one of the expected values (0x07 - Data Packet, 0x02 - Acknowledgment Packet, 0x08 - End of Data Packet)
    uint8_t pid = packet[6];
    if (pid != 0x07 && pid != 0x02 && pid != 0x08) {
        ESP_LOGW(kTag, "%s: Unexpected PID: 0x%02X", ctx, pid);
    }

    uint16_t packetContentLength = (static_cast<uint16_t>(packet[7]) << 8) | packet[8];
    size_t totalPacketLength = 9 + packetContentLength;

    if (length < totalPacketLength) {
        ESP_LOGE(kTag, "%s: Packet is truncated (received %zu, required %zu).", 
                 ctx, length, totalPacketLength);
        return false;
    }

    uint16_t calculatedChecksum = 0;
    for (size_t i = 6; i < totalPacketLength - 2; ++i) {
        calculatedChecksum += packet[i];
    }

    uint16_t receivedChecksum = (static_cast<uint16_t>(packet[totalPacketLength - 2]) << 8) | 
                                 packet[totalPacketLength - 1];

    if (receivedChecksum != calculatedChecksum) {
        ESP_LOGE(kTag, "%s: Checksum mismatch: received=0x%04X, calculated=0x%04X", 
                 ctx, receivedChecksum, calculatedChecksum);
        return false;
    }

    return true;
}


bool FingerprintModule::takeImage() {
    uint8_t command[] = {
        0xEF, 0x01, // Header
        0xFF, 0xFF, 0xFF, 0xFF, // Address
        0x01, // Packet identifier : command packet
        0x00, 0x03, // Packet length
        0x01, // Command code : PS_GetImage
        0x00, 0x05 // Checksum
    };

    if (!sendPacket(command, sizeof(command))) {
        ESP_LOGE(kTag, "Failed to send take image command.");
        return false;
    }

    uint8_t ackBuf[32];
    size_t actualLength = 0;
    if (!receivePacket(ackBuf, sizeof(ackBuf), &actualLength, 2000)) {
        ESP_LOGE(kTag, "Failed to receive response for take image command.");
        return false;
    }

    if (!checkPacket("takeImage", ackBuf, actualLength, 12)) {
        return false;
    }

    if (ackBuf[9] != 0x00) {
        ESP_LOGE(kTag, "Take image command failed with error code: 0x%02X", ackBuf[9]);
        return false;
    }

    return true;
}

const uint8_t* FingerprintModule::getNewestRawImage() {
    const uint8_t command[] = {
        0xEF, 0x01,             // Header
        0xFF, 0xFF, 0xFF, 0xFF, // Address
        0x01,                   // PID: Command
        0x00, 0x03,             // Length
        0x0A,                   // Code: PS_UpImage (0x0A)
        0x00, 0x0E              // Checksum (0x01 + 0x00 + 0x03 + 0x0A = 0x0E)
    };

    if (!sendPacket(command, sizeof(command))) {
        ESP_LOGE(kTag, "getNewestRawImage: Failed to send PS_UpImage command.");
        return nullptr;
    }

    uint8_t ackBuf[32];
    size_t actualLength = 0;
    if (!receivePacket(ackBuf, sizeof(ackBuf), &actualLength, 2000)) {
        ESP_LOGE(kTag, "getNewestRawImage: Timeout waiting for PS_UpImage ACK.");
        return nullptr;
    }

    if (!checkPacket("getNewestRawImage", ackBuf, actualLength, 12)) {
        return nullptr;
    }

    if (ackBuf[9] != 0x00) {
        ESP_LOGE(kTag, "getNewestRawImage: Sensor refused upload with code: 0x%02X", ackBuf[9]);
        return nullptr;
    }

    constexpr size_t totalImageSize = kImageWidth * kImageHeight * kBitsPerPixel / 8; 
    uint8_t* tempBuffer = (uint8_t*)heap_caps_calloc(totalImageSize, 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); // Create buffer in psram 
    if (!tempBuffer) {
        ESP_LOGE(kTag, "getNewestRawImage: Failed to allocate image buffer in PSRAM.");
        return nullptr;
    }

    size_t bytesReceived = 0;
    uint8_t packetBuf[kTransferBufferSize];
    bool isStreamComplete = false;

    while (!isStreamComplete && bytesReceived < totalImageSize) {
        if (!receivePacket(packetBuf, sizeof(packetBuf), &actualLength, 2000)) {
            ESP_LOGE(kTag, "getNewestRawImage: Timeout receiving image chunk.");
            free(tempBuffer);
            return nullptr;
        }

        if (!checkPacket("ImageChunk", packetBuf, actualLength, 11)) {
            free(tempBuffer);
            return nullptr;
        }

        uint8_t pid = packetBuf[6];
        uint16_t packetLength = (static_cast<uint16_t>(packetBuf[7]) << 8) | packetBuf[8];
        size_t payloadLength = packetLength - 2;

        if (bytesReceived + payloadLength > totalImageSize) {
            ESP_LOGE(kTag, "getNewestRawImage: Image stream exceeded buffer capacity.");
            free(tempBuffer);
            return nullptr;
        }

        memcpy(tempBuffer + bytesReceived, &packetBuf[9], payloadLength);
        bytesReceived += payloadLength;

        if (pid == 0x08) {
            isStreamComplete = true;
        }
    }

    if (bytesReceived != totalImageSize) {
        ESP_LOGE(kTag, "getNewestRawImage: Incomplete image data (%zu / %zu bytes).", 
                 bytesReceived, totalImageSize);
        free(tempBuffer);
        return nullptr;
    }

    ESP_LOGI(kTag, "getNewestRawImage: Successfully downloaded raw image (%zu bytes).", totalImageSize);
    
    if (mImageBuffer) {
        free(mImageBuffer);
    }
    mImageBuffer = tempBuffer;
    return mImageBuffer;
}