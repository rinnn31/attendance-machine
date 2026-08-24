#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/*
* This file is used to define board's pin mapping and other hardware-specific configurations.
*/

#include <Arduino.h>

// Camera pins
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5

#define Y9_GPIO_NUM       16
#define Y8_GPIO_NUM       17
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM       10
#define Y4_GPIO_NUM       8
#define Y3_GPIO_NUM       9
#define Y2_GPIO_NUM       11

#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM     13

// Display pins
#define SDA_PIN 1
#define SCL_PIN 2

// Sensor module pins
#define LD2410_RX_PIN 47
#define LD2410_TX_PIN 21
#define LD2410_WAKE_PIN 46

#define AS608_TX_PIN 42
#define AS608_RX_PIN 41
#define AS608_WAKE_PIN 14

// Control pins
#define MODE_SWITCH_BUTTON_PIN 3

#endif // BOARD_CONFIG_H