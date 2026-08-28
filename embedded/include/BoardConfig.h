#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/*
* This file is used to define board's pin mapping and other hardware-specific configurations.
*/

#include <Arduino.h>

namespace BoardConfig {

#ifndef DEVICE_SERIAL
#error "DEVICE_SERIAL is not defined. Please define it in your build environment or in this file."
#endif

inline constexpr const char* kDeviceSerial = DEVICE_SERIAL;
inline constexpr const char* kDefaultSoftApPassword = "12345678";

}
#endif // BOARD_CONFIG_H