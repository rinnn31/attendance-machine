#include "FaceCamModule.h"

#include <esp_camera.h>
#include <human_face_detect.hpp>
#include <esp_log.h>

#include "BoardConfig.h"

constexpr const char* kTag = "FaceCamModule";

constexpr uint8_t kSensitivityThreshold = 10;
constexpr uint16_t kMinDistanceMm = 300;
constexpr uint16_t kMaxDistanceMm = 600;

constexpr uint8_t kDefaultStableFrameCount = 10;
constexpr uint8_t kLostFaceFrameThreshold = 2;
constexpr float kFaceDetectionScoreThreshold = 0.7f;

FaceCamModule::FaceCamModule() :
    mDistanceSensorTaskHandle(nullptr),
    mCameraTaskHandle(nullptr),
    mOnFaceStableCallback(nullptr),
    mOnFacePresenceChangedCallback(nullptr),
    mDistanceSensorDoneSem(nullptr),
    mToFSensorWire(nullptr),
    mCameraDoneSem(nullptr),
    mDistanceSensor(nullptr),
    mFaceDetector(nullptr),
    mFaceDetectionEnabled(false),
    mCameraEnabled(false),
    mIsTrackingFace(false),
    mFirstFaceX(0),
    mFirstFaceY(0),
    mStableFrameCounter(0),
    mStableFrameCountThreshold(kDefaultStableFrameCount),
    mLostFaceFrameCounter(0),
    mFacePresent(false),
    mOutOfRangeCounter(0),
    mInRangeCounter(0),
    mRunning(false) {
}

FaceCamModule::~FaceCamModule() {
    end();
}

bool FaceCamModule::begin() {
    mDistanceSensorDoneSem = xSemaphoreCreateBinary();
    mCameraDoneSem = xSemaphoreCreateBinary();

    ///Initialize the distance sensor
    mToFSensorWire = new TwoWire(0);
    if (!mToFSensorWire->begin(BoardConfig::kToFSensorSdaPin, BoardConfig::kToFSensorSclPin, 400000)) {
        ESP_LOGE(kTag, "Failed to initialize I2C for distance sensor");
        return false;
    }
    mDistanceSensor = new Adafruit_VL53L0X();
    if (!mDistanceSensor->begin(0x29, false, mToFSensorWire)) {
        return false;
    }

    // Initialize the camera
    camera_config_t config{};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;

    config.pin_d0       = BoardConfig::kPinY2;
    config.pin_d1       = BoardConfig::kPinY3;
    config.pin_d2       = BoardConfig::kPinY4;
    config.pin_d3       = BoardConfig::kPinY5;
    config.pin_d4       = BoardConfig::kPinY6;
    config.pin_d5       = BoardConfig::kPinY7;
    config.pin_d6       = BoardConfig::kPinY8;
    config.pin_d7       = BoardConfig::kPinY9;

    config.pin_xclk     = BoardConfig::kPinXclk;
    config.pin_pclk     = BoardConfig::kPinPclk;
    config.pin_vsync    = BoardConfig::kPinVsync;
    config.pin_href     = BoardConfig::kPinHref;
    config.pin_sccb_sda = BoardConfig::kPinSiod;
    config.pin_sccb_scl = BoardConfig::kPinSioc;
    config.pin_pwdn     = BoardConfig::kPinPwdn;
    config.pin_reset    = BoardConfig::kPinReset;

    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_RGB565;
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.fb_count = 2;
    config.frame_size = FRAMESIZE_QVGA;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Camera init failed with error 0x%x", err);
        return false;
    }
    sensor_t* s = esp_camera_sensor_get();
    if (s && s->id.PID == OV3660_PID) {
        s->set_vflip(s, 1);
        s->set_brightness(s, 1);
        s->set_saturation(s, -2);
    }

    toggleCamera(false);

    mFaceDetector = new HumanFaceDetect();
    
    // Start tasks
    mRunning = true;
    if (xTaskCreate(distanceSensorTask, "DistanceSensorTask", 4096, this, 1, &mDistanceSensorTaskHandle) != pdPASS) {
        ESP_LOGE(kTag, "Failed to create distance sensor task");
        mRunning = false;
        return false;
    }

    // Pin the camera task to core 1 to get better performance
    if (xTaskCreatePinnedToCore(cameraTask, "CameraTask", 8192, this, 1, &mCameraTaskHandle, 1) != pdPASS) {
        ESP_LOGE(kTag, "Failed to create camera task on core 1");
        mRunning = false;
        return false;
    }

    return true;
}

void FaceCamModule::end() {
    constexpr TickType_t kTimeout = pdMS_TO_TICKS(3000);
    mRunning = false;

    if (mDistanceSensorTaskHandle) {
        if (mDistanceSensorDoneSem && xSemaphoreTake(mDistanceSensorDoneSem, kTimeout) != pdTRUE) {
            ESP_LOGW(kTag, "Timeout waiting for distance sensor task to finish, deleting task forcefully.");
        }
        vTaskDelete(mDistanceSensorTaskHandle);
        mDistanceSensorTaskHandle = nullptr;
    }
    if (mCameraTaskHandle) {
        if (mCameraDoneSem && xSemaphoreTake(mCameraDoneSem, kTimeout) != pdTRUE) {
            ESP_LOGW(kTag, "Timeout waiting for camera task to finish, deleting task forcefully.");
        }
        vTaskDelete(mCameraTaskHandle);
        mCameraTaskHandle = nullptr;
    }
    if (mDistanceSensorDoneSem) {
        vSemaphoreDelete(mDistanceSensorDoneSem);
        mDistanceSensorDoneSem = nullptr;
    }
    if (mCameraDoneSem) {
        vSemaphoreDelete(mCameraDoneSem);
        mCameraDoneSem = nullptr;
    }

    if (mDistanceSensor) {
        delete mDistanceSensor;
        mDistanceSensor = nullptr;
    }
    if (mToFSensorWire) {
        delete mToFSensorWire;
        mToFSensorWire = nullptr;
    }
    if (mFaceDetector) {
        delete mFaceDetector;
        mFaceDetector = nullptr;
    }
    
    esp_camera_deinit();
}

void FaceCamModule::toggleCamera(bool enable) {
    sensor_t* sensor = esp_camera_sensor_get();
    if (sensor) {
        BoardConfig::__toggle_camera_power(sensor, enable);
        mCameraEnabled = enable;
    }
}

void FaceCamModule::distanceSensorTask(void* arg) {
    FaceCamModule* self = static_cast<FaceCamModule*>(arg);
    while (self->mRunning) {
        if (self->mFaceDetectionEnabled && self->mDistanceSensor) {
            VL53L0X_RangingMeasurementData_t measure;
            self->mDistanceSensor->rangingTest(&measure, false);
            if (measure.RangeStatus == 0 && measure.RangeMilliMeter <= kMaxDistanceMm && measure.RangeMilliMeter >= kMinDistanceMm) {
                self->mOutOfRangeCounter = 0; // Reset out of range counter
                
                if (!self->mCameraEnabled) {
                    if (self->mInRangeCounter++ >= kSensitivityThreshold) {
                        self->toggleCamera(true);
                        self->mInRangeCounter = 0;
                    }
                }
            } else {
                self->mInRangeCounter = 0; // Reset in range counter

                if (self->mCameraEnabled) {
                    if (self->mOutOfRangeCounter++ >= kSensitivityThreshold) {
                        self->toggleCamera(false);
                        self->mOutOfRangeCounter = 0;
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (self->mDistanceSensorDoneSem) {
        xSemaphoreGive(self->mDistanceSensorDoneSem);
    }
    vTaskSuspend(nullptr);
}

void FaceCamModule::cameraTask(void* arg) {
    FaceCamModule* self = static_cast<FaceCamModule*>(arg);
    while (self->mRunning) {
        if (self->mCameraEnabled && self->mFaceDetectionEnabled) {
            camera_fb_t* fb = esp_camera_fb_get();
            if (fb) {
                dl::image::img_t img;
                img.data = fb->buf;
                img.width = fb->width;
                img.height = fb->height;
                img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565BE;
                
                auto faces = self->mFaceDetector->run(img);
                bool faceDetected = !faces.empty() && faces.front().score >= kFaceDetectionScoreThreshold;

                if (faceDetected) {
                    auto& face = faces.front();
                    self->postFacePresenceChangedEvent(true);
                    if (self->trackFaceState(face.box[0], face.box[1], face.box[2] - face.box[0], face.box[3] - face.box[1], fb->width, fb->height)) {
                        self->resetFaceTracking();
                        if (self->mOnFaceStableCallback) {
                            self->mOnFaceStableCallback(fb->buf, fb->len, fb->width, fb->height);
                        }
                    }

                    self->mLostFaceFrameCounter = 0; // Reset lost face counter on detection

                } else {
                    if (self->mLostFaceFrameCounter++ >= kLostFaceFrameThreshold) {
                        self->postFacePresenceChangedEvent(false);
                        self->resetFaceTracking();
                        self->mLostFaceFrameCounter = 0; // Reset lost face counter after reporting
                    }
                }
                
                esp_camera_fb_return(fb);
            } 

            vTaskDelay(pdMS_TO_TICKS(10));
        } else {
            self->postFacePresenceChangedEvent(false);
            self->resetFaceTracking();

            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    if (self->mCameraDoneSem) {
        xSemaphoreGive(self->mCameraDoneSem);
    }
    vTaskSuspend(nullptr);
}

bool FaceCamModule::trackFaceState(int16_t faceX, int16_t faceY, size_t faceWidth, size_t faceHeight, size_t frameWidth, size_t frameHeight) {
    if (!mIsTrackingFace) {
        mFirstFaceX = faceX;
        mFirstFaceY = faceY;
        mIsTrackingFace = true;
        mStableFrameCounter = 0;
        return false;
    } else {
        int16_t deltaX = std::abs(faceX - mFirstFaceX);
        int16_t deltaY = std::abs(faceY - mFirstFaceY);
        
        int16_t maxDeltaX = std::max<int16_t>(faceWidth * 0.15f, 10);
        int16_t maxDeltaY = std::max<int16_t>(faceHeight * 0.15f, 10);

        bool ok = faceHeight  / (float)frameHeight > 0.3f && deltaX <= maxDeltaX && deltaY <= maxDeltaY;
        if (ok) {
            mStableFrameCounter++;
            if (mStableFrameCounter >= mStableFrameCountThreshold) {
                mStableFrameCounter = 0; // Reset counter after stable detection
                return true;
            }
        } else {
            mFirstFaceX = faceX;
            mFirstFaceY = faceY;
            mStableFrameCounter = 0;
        }
    }
    return false;
}

void FaceCamModule::postFacePresenceChangedEvent(bool isFacePresent) {
    if (isFacePresent != mFacePresent) {
        mFacePresent = isFacePresent;
        if (mOnFacePresenceChangedCallback) {
            mOnFacePresenceChangedCallback(isFacePresent);
        }
    }
}

void FaceCamModule::resetFaceTracking() {
    mIsTrackingFace = false;
    mFirstFaceX = -1;
    mFirstFaceY = -1;
    mStableFrameCounter = 0;
}