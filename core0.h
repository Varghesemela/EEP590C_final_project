// Filename: core0.h

#ifndef CORE0_H
#define CORE0_H

#include <Arduino.h> 

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include "driver/timer.h"

#include "esp_timer.h"

// I²C LCD (same wiring as Part 1: SDA=21, SCL=22)
#define LCD_ADDR       0x27

// =============== LOW-LEVEL LCD I²C Routines =============== //
// (same as Part 1—split into nibbles, toggle EN, RS etc.)
#define LCD_ADDR       0x27
#define LCD_BACKLIGHT  0x08
#define LCD_ENABLE     0x04
#define LCD_RS         0x01

#define LCD_ROW0      0x00
#define LCD_ROW1      0x40
#define LCD_CMD_CR    0x80
#define LCD_CMD_CLS   0x01

extern portMUX_TYPE timerMux;

// Task function prototypes
void updateButtonTask(void* arg);
void ServoRunTask(void* arg);
void motionTask(void* pvParameters);
void LCDTask(void* arg);
// Function prototypes
void taskRFIDReader(void *pvParameters);
void taskPrinter(void *pvParameters);

void IRAM_ATTR onLockTimer(void* arg);
void IRAM_ATTR onBacklightTimer(void* arg);

#endif
