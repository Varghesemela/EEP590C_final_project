#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <ESP32Servo.h>
#include <LCD_I2C.h>
#include <SPI.h>
#include <MFRC522.h>

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

// ========== Constants ==========
extern const float SOUND_SPEED_CM_PER_US;
extern const unsigned long debounceDelay;

// ========== Peripheral Objects ==========
extern LCD_I2C lcd;
extern Servo myservo;
extern MFRC522 rfid; // RFID instance
extern QueueHandle_t rfidQueue;


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