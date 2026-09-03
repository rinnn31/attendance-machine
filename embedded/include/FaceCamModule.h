#ifndef FACE_CAM_MODULE_H
#define FACE_CAM_MODULE_H

#include <Adafruit_VL53L0X.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <human_face_detect.hpp>
#include <stdint.h>
#include <atomic>
#include <functional>

using OnFaceStableCallback = std::function<void(const uint8_t* faceImage, size_t imageSize, size_t width, size_t height)>;
using OnFacePresenceChangedCallback = std::function<void(bool isFacePresent)>;

class FaceCamModule {
public:
    FaceCamModule();
    ~FaceCamModule();

    bool begin();
    void end(); 
    void toggleFaceDetection(bool enable) { mFaceDetectionEnabled = enable; }
    void setStableFrameCountThreshold(uint8_t count) { mStableFrameCountThreshold = count; }
    void resetFaceTracking();
    void onFaceStable(OnFaceStableCallback callback) { mOnFaceStableCallback = callback; }
    void onFacePresenceChanged(OnFacePresenceChangedCallback callback) { mOnFacePresenceChangedCallback = callback; }

private:
    static void distanceSensorTask(void* arg);
    static void cameraTask(void* arg);  

    void toggleCamera(bool enable);
    bool trackFaceState(int16_t faceX, int16_t faceY, size_t width, size_t height, size_t frameWidth, size_t frameHeight);
    void postFacePresenceChangedEvent(bool isFacePresent);

    TaskHandle_t mDistanceSensorTaskHandle;
    TaskHandle_t mCameraTaskHandle;
    SemaphoreHandle_t mDistanceSensorDoneSem;
    SemaphoreHandle_t mCameraDoneSem;

    OnFaceStableCallback mOnFaceStableCallback;
    OnFacePresenceChangedCallback mOnFacePresenceChangedCallback;
    Adafruit_VL53L0X* mDistanceSensor;
    TwoWire* mToFSensorWire;
    HumanFaceDetect* mFaceDetector;

    std::atomic<bool> mFaceDetectionEnabled;
    std::atomic<bool> mCameraEnabled;

    // Face tracking parameters
    int16_t mFirstFaceX;
    int16_t mFirstFaceY;
    bool mIsTrackingFace;
    uint8_t mStableFrameCounter;
    uint8_t mStableFrameCountThreshold;
    uint8_t mLostFaceFrameCounter;
    std::atomic<bool> mFacePresent;

    uint8_t mOutOfRangeCounter;
    uint8_t mInRangeCounter;

    std::atomic<bool> mRunning;
};

#endif // FACE_CAM_MODULE_H