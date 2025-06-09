#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <RTClib.h>

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


//========= TASK HANDLES =========
extern TaskHandle_t TaskLED_Handle;
extern TaskHandle_t TaskServoRun_Handle;
extern TaskHandle_t TaskMotion_Handle;
extern TaskHandle_t TaskSound_Handle;
extern TaskHandle_t TaskLCD_Handle;
extern TaskHandle_t TaskUltraSonic_Handle;
extern TaskHandle_t taskRFIDReader_Handle;
extern TaskHandle_t taskPrinter_Handle;
extern TaskHandle_t taskRTC_Handle;

extern const char* allowedUIDs[];
extern const int numAllowedUIDs;

// ========== Constants ==========
extern const float SOUND_SPEED_CM_PER_US;
extern const unsigned long debounceDelay;

// ========== Peripheral Objects ==========
extern LiquidCrystal_I2C lcd;
extern Servo myservo;
extern MFRC522 rfid; // RFID instance
extern QueueHandle_t rfidQueue;
extern RTC_DS3231 rtc;            // RTC object

// ========== Semaphore ==========
extern SemaphoreHandle_t i2c_semaphore;

// ========== State Flags ==========
extern volatile bool motion;
extern volatile bool close_dist;
extern volatile bool isLock;

// ========== Buffers and Indices ==========
extern int motionBuffer[5];
extern float distanceBuffer[5];

extern volatile int bufferIndex;
extern volatile int bufferIndex_sound;
extern volatile int bufferIndex_dist;

// ========== Button State ==========
extern bool lastButtonReading;
extern bool buttonState;
extern unsigned long lastDebounceTime;

#endif