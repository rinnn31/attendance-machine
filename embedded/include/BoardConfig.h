#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/*
* This file is used to define board's pin mapping and other hardware-specific configurations.
*/

#include <Arduino.h>
#include <esp_camera.h>

namespace BoardConfig {

#ifndef DEVICE_SERIAL
#error "DEVICE_SERIAL is not defined. Please define it in your build environment or in this file."
#endif

inline constexpr const char* kDeviceSerial = DEVICE_SERIAL;
inline constexpr const char* kDefaultSoftApPassword = "12345678";

inline constexpr uint32_t kMinResetPressDurationMs = 10000;
inline constexpr uint32_t kMinModeSwitchPressDurationMs = 3000;

inline constexpr uint8_t kModeSwitchButtonPin = 3;
inline constexpr uint8_t kFingerprintSensorWakeupPin = 14;  
inline constexpr uint8_t kFingerprintRxPin = 38;
inline constexpr uint8_t kFingerprintTxPin = 1;

inline constexpr uint8_t kToFSensorSclPin = 39;
inline constexpr uint8_t kToFSensorSdaPin = 40;

inline constexpr uint8_t kDisplaySckPin = 41;
inline constexpr uint8_t kDisplayMosiPin = 42;
inline constexpr uint8_t kDisplayRstPin = 21;
inline constexpr uint8_t kDisplayDcPin = 47;

// Camera specific configurations
inline constexpr int8_t  kPinPwdn   = -1;
inline constexpr int8_t  kPinReset  = -1;
inline constexpr uint8_t kPinXclk   = 15;
inline constexpr uint8_t kPinSiod   = 4;
inline constexpr uint8_t kPinSioc   = 5;

inline constexpr uint8_t kPinY9     = 16; // D7
inline constexpr uint8_t kPinY8     = 17;  // D6
inline constexpr uint8_t kPinY7     = 18;  // D5
inline constexpr uint8_t kPinY6     = 12;  // D4
inline constexpr uint8_t kPinY5     = 10;  // D3
inline constexpr uint8_t kPinY4     = 8;  // D2
inline constexpr uint8_t kPinY3     = 9; // D1
inline constexpr uint8_t kPinY2     = 11; // D0

inline constexpr uint8_t kPinVsync  = 6;
inline constexpr uint8_t kPinHref   = 7;
inline constexpr uint8_t kPinPclk   = 13;

// Macro to toggle camera power using the sensor's register, 
// this depends on the specific camera sensor being used (currently using OV3660). 
// Adjust the register and values as necessary for your hardware.
inline void __toggle_camera_power(sensor_t* sensor, bool enable) {
    sensor->set_reg(sensor, 0x3008, 0xFF, enable ? 0x02 : 0x42);
}

} // namespace BoardConfig
#endif // BOARD_CONFIG_H