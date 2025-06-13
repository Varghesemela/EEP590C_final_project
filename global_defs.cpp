/**
 * @file global_defs.cpp
 * @brief Global variables, constants, and peripheral instances for the Smart Security Door System.
 *
 * @details
 * This file holds the definitions for globally shared variables, task handles, hardware pin configurations,
 * FreeRTOS synchronization primitives, queues, and sensor state flags. These definitions are used across
 * multiple modules including sensor processing, RFID access control, LCD display handling, and timing logic.
 *
 * Peripheral Components:
 * - Ultrasonic Sensor (TRIG, ECHO)
 * - PIR Motion Sensor
 * - RFID Reader (MFRC522)
 * - I2C-based LCD
 * - Servo Motor
 * - RTC (DS3231)
 *
 * System Synchronization:
 * - FreeRTOS tasks and task handles
 * - Queues for sensor data and RFID UIDs
 * - Semaphores for I2C access
 * - Timers for backlight and lock control
 *
 * @authors Authors
 * Created by Sanjay Varghese, 2025  
 * Additionally modified by Sai Jayanth Kalisi, 2025  
 * Additionally modified by Ankit Telluri, 2025
 */

#include "global_defs.h"

// ========== Pin Definitions ==========
const int LED = 1;         ///< On-board LED pin
const int SERVO_PIN = 12;  ///< Servo motor control pin
// const int POT_PIN = 17; ///< Unused: Potential analog input for potentiometer
const int BUTTON_PIN = 35;  ///< Manual servo lock/unlock button
const int SDA_PIN = 8;      ///< I2C SDA line
const int SCL_PIN = 9;      ///< I2C SCL line
const int PIR_PIN = 5;      ///< PIR motion sensor pin
// const int SOUND_PIN = 18; ///< Unused: Sound sensor pin
const int ECHO_PIN = 40;  ///< Echo pin for ultrasonic distance sensor
const int TRIG_PIN = 41;  ///< Trigger pin for ultrasonic distance sensor

// Define pins for MFRC522
const int SS_PIN = 20;    ///< RFID slave select
const int RST_PIN = 0;    ///< RFID reset pin
const int MOSI_PIN = 47;  ///< SPI MOSI pin
const int MISO_PIN = 48;  ///< SPI MISO pin
const int IRQ_PIN = 45;   ///< RFID interrupt pin (not used, but defined if needed)
const int SCK_PIN = 21;   ///< SPI clock pin

// ========== Task Handles ==========
TaskHandle_t TaskLED_Handle = NULL;            ///< LED blinking task
TaskHandle_t TaskServoRun_Handle = NULL;       ///< Servo motor control task
TaskHandle_t TaskUpdateButton_Handle = NULL;   ///< Button debounce and state update task
TaskHandle_t TaskMotion_Handle = NULL;         ///< PIR sensor logic task
TaskHandle_t TaskSound_Handle = NULL;          ///< Sound detection task
TaskHandle_t TaskLCD_Handle = NULL;            ///< LCD display update task
TaskHandle_t TaskUltraSonic_Handle = NULL;     ///< Ultrasonic distance sensing task (legacy)
TaskHandle_t taskRFIDReader_Handle = NULL;     ///< RFID scanning task
TaskHandle_t taskPrinter_Handle = NULL;        ///< RFID tag validation and access control
TaskHandle_t taskRTC_Handle = NULL;            ///< RTC timestamping task (deprecated)
TaskHandle_t taskSensorRead_Handle = NULL;     ///< Unified sensor reading task
TaskHandle_t taskSensorProcess_Handle = NULL;  ///< Sensor data processing task

// ========== RFID Access Control ==========
const char* allowedUIDs[] = {
  "DE AD BE EF",
  "CA FE BA BE",
  "BF 6D CB 1F",
  "79 49 4D B2"
};                                                                        ///< List of authorized RFID UIDs
const int numAllowedUIDs = sizeof(allowedUIDs) / sizeof(allowedUIDs[0]);  ///< Count of allowed UIDs

// ========== FreeRTOS Queues ==========
QueueHandle_t rfidQueue;           ///< Queue for RFID UID strings
QueueHandle_t sensorQueue = NULL;  ///< Queue for sensorData_t structs

// ========== Constants ==========
const float SOUND_SPEED_CM_PER_US = 0.0343f;  ///< Speed of sound in cm/μs
const unsigned long debounceDelay = 50;       ///< Debounce duration in milliseconds

// ========== Semaphore ==========
SemaphoreHandle_t i2c_semaphore;  ///< Semaphore to manage I2C bus access

// ========== Peripheral Objects ==========
LiquidCrystal_I2C lcd(0x27, 16, 2);  ///< I2C 16x2 LCD display instance
Servo myservo;                       ///< Servo motor instance
MFRC522 rfid(SS_PIN, RST_PIN);       ///< RFID reader instance
RTC_DS3231 rtc;                      ///< Real-time clock instance

// ========== State Flags ==========
volatile bool motion_detected = false;  ///< Motion state flag (true if detected)
volatile bool sound = false;            ///< Sound state flag (not really used)
volatile bool close_dist = false;       ///< Proximity state flag (true if close)
volatile bool isLock = true;            ///< Lock state flag (true if locked)

// ========== Buffers and Indices ==========
int motionBuffer[5] = { 0 };         ///< Circular buffer for motion readings
int soundBuffer[5] = { 0 };          ///< Circular buffer for sound readings
float distanceBuffer[5] = { 0.0f };  ///< Circular buffer for distance measurements

volatile int bufferIndex = 0;        ///< General buffer index
volatile int bufferIndex_sound = 0;  ///< Sound buffer write index
volatile int bufferIndex_dist = 0;   ///< Distance buffer write index
volatile int idx_dist = 0;           ///< Read index for distance processing
volatile int idx_motion = 0;         ///< Read index for motion processing

bool backlightOn = false;           ///< LCD backlight state flag
esp_timer_handle_t backlightTimer;  ///< Timer for backlight timeout
esp_timer_handle_t lockTimer;       ///< Timer for lock reset after granted access

// ========== Button State ==========
bool lastButtonReading = HIGH;       ///< Previous raw reading of button
bool buttonState = HIGH;             ///< Debounced button state
unsigned long lastDebounceTime = 0;  ///< Time of last button state change

// ========== Distance State ==========
volatile uint64_t echo_start_us = 0;  ///< Echo pulse start time (μs)
volatile uint64_t echo_end_us = 0;    ///< Echo pulse end time (μs)