/**
 * @file global_defs.h
 * @brief Global declarations for Smart Security Door system.
 *
 * @details
 * This header provides external declarations for all globally used hardware pin definitions, 
 * task handles, semaphores, peripheral objects, sensor state variables, buffers, and RTOS resources.
 * It is shared across all components of the system such as RFID access control, distance sensing, 
 * LCD display, and real-time clock functionalities.
 *
 * It complements `global_defs.cpp` where these variables are defined and initialized.
 *
 * @authors Authors
 * Created by Sanjay Varghese, 2025  
 * Additionally modified by Sai Jayanth Kalisi, 2025  
 * Additionally modified by Ankit Telluri, 2025
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <RTClib.h>
#include "driver/timer.h"

// ========== Pin Definitions ==========
extern const int LED;
extern const int SERVO_PIN;
extern const int POT_PIN;
extern const int BUTTON_PIN;
extern const int SDA_PIN;
extern const int SCL_PIN;
extern const int PIR_PIN;
extern const int ECHO_PIN;
extern const int TRIG_PIN;
extern const int SS_PIN;
extern const int RST_PIN;
extern const int MOSI_PIN;
extern const int MISO_PIN;
extern const int IRQ_PIN;
extern const int SCK_PIN;

//========= TASK HANDLES =========
extern TaskHandle_t TaskLED_Handle;
extern TaskHandle_t TaskServoRun_Handle;
extern TaskHandle_t TaskUpdateButton_Handle;
extern TaskHandle_t TaskMotion_Handle;
extern TaskHandle_t TaskSound_Handle;
extern TaskHandle_t TaskLCD_Handle;
extern TaskHandle_t TaskUltraSonic_Handle;
extern TaskHandle_t taskRFIDReader_Handle;
extern TaskHandle_t taskPrinter_Handle;
extern TaskHandle_t taskRTC_Handle;
extern TaskHandle_t taskSensorRead_Handle;
extern TaskHandle_t taskSensorProcess_Handle;
extern const char* allowedUIDs[];
extern const int numAllowedUIDs;

// ========== Constants ==========
extern const float SOUND_SPEED_CM_PER_US;
extern const unsigned long debounceDelay;

typedef struct {
  float distanceCm;
  int   motionState;   // HIGH or LOW from PIR
} sensorData_t;

// ========== Peripheral Objects ==========
extern LiquidCrystal_I2C lcd;
extern Servo myservo;
extern MFRC522 rfid; // RFID instance
extern QueueHandle_t rfidQueue;
extern QueueHandle_t sensorQueue;

extern RTC_DS3231 rtc;            // RTC object

// ========== Semaphore ==========
extern SemaphoreHandle_t i2c_semaphore;

// ========== State Flags ==========
extern volatile bool motion_detected;
extern volatile bool close_dist;
extern volatile bool isLock;

// ========== Buffers and Indices ==========
extern int motionBuffer[5];
extern float distanceBuffer[5];

extern volatile int bufferIndex;
extern volatile int bufferIndex_sound;
extern volatile int bufferIndex_dist;
extern volatile int idx_dist;
extern volatile int idx_motion;

extern bool backlightOn;
extern esp_timer_handle_t backlightTimer;
extern esp_timer_handle_t lockTimer;

// ========== Button State ==========
extern bool lastButtonReading;
extern bool buttonState;
extern unsigned long lastDebounceTime;

// ========== Distance State ==========
extern volatile uint64_t echo_start_us;
extern volatile uint64_t echo_end_us;
extern void IRAM_ATTR echoISR();

#endif