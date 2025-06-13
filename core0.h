/**
 * @file core0.h
 * @brief Header file for Core 0 tasks in the ESP32 Smart Security System (EE590 Final Project)
 *
 * @details
 * This header defines constants, macros, and function prototypes used for managing
 * FreeRTOS tasks running on Core 0 of the ESP32. It supports control over peripherals
 * such as the LCD display and servo motor, as well as synchronization primitives,
 * interrupts, and software timers. 
 *
 * Tasks declared here handle button interaction, servo motor control, and LCD output.
 * Low-level LCD I²C macros are provided to simplify bitwise control of the LCD module.
 *
 * @section hardware Hardware Setup
 * - **LCD I2C Address**: `0x27` (connected via SDA = GPIO8, SCL = GPIO9)
 * - **LCD Control Bits**: BACKLIGHT, ENABLE, RS, etc., defined for raw I²C access
 *
 * @section author Author
 * Created by Sanjay Varghese, 2025  
 * Additionally modified by Sai Jayanth Kalisi, 2025  
 * Additionally modified by Ankit Telluri, 2025  
 */

#ifndef CORE0_H
#define CORE0_H

#include <Arduino.h> 
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include "driver/timer.h"-
#include "esp_timer.h"

extern portMUX_TYPE timerMux; ///< Mux for critical section control during ISR access (used in timers)

//======================= TASK PROTOTYPES =======================//
void updateButtonTask(void* arg);
void ServoRunTask(void* arg);
void motionTask(void* pvParameters);
void LCDTask(void* arg);

//======================= TIMER ISR PROTOTYPES =======================//
void IRAM_ATTR onLockTimer(void* arg);
void IRAM_ATTR onBacklightTimer(void* arg);

#endif
