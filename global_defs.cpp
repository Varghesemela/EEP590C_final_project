#include "global_defs.h"

// ========== Pin Definitions ==========
const int LED = 1;
const int SERVO_PIN = 12;
// const int POT_PIN = 17;
const int BUTTON_PIN = 35;
const int SDA_PIN = 8;
const int SCL_PIN = 9;
const int PIR_PIN = 5;
const int SOUND_PIN = 18;
const int ECHO_PIN = 40;
const int TRIG_PIN = 41;

// Define pins for MFRC522
const int SS_PIN = 10;
const int RST_PIN = 14;

//========= TASK HANDLES =========
TaskHandle_t TaskLED_Handle = NULL;
TaskHandle_t TaskServoRun_Handle = NULL;
TaskHandle_t TaskMotion_Handle = NULL;
TaskHandle_t TaskSound_Handle = NULL;
TaskHandle_t TaskLCD_Handle = NULL;
TaskHandle_t TaskUltraSonic_Handle = NULL;
TaskHandle_t taskRFIDReader_Handle = NULL;
TaskHandle_t taskPrinter_Handle = NULL;
TaskHandle_t taskRTC_Handle = NULL;

const char* allowedUIDs[] = {
  "DE AD BE EF",
  "CA FE BA BE",
  "BF 6D CB 1F"
};
const int numAllowedUIDs = sizeof(allowedUIDs) / sizeof(allowedUIDs[0]);

// Queue handle
QueueHandle_t rfidQueue;

// ========== Constants ==========
const float SOUND_SPEED_CM_PER_US = 0.0343f;
const unsigned long debounceDelay = 50;

// ========== Semaphore ==========
SemaphoreHandle_t i2c_semaphore;

// ========== Peripheral Objects ==========
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myservo;
MFRC522 rfid(SS_PIN, RST_PIN); // RFID instance
RTC_DS3231 rtc;

// ========== State Flags ==========
volatile bool motion = false;
volatile bool sound = false;
volatile bool close_dist = false;
volatile bool isLock = true;

// ========== Buffers and Indices ==========
int motionBuffer[5] = {0};
int soundBuffer[5] = {0};
float distanceBuffer[5] = {0.0f};

volatile int bufferIndex = 0;
volatile int bufferIndex_sound = 0;
volatile int bufferIndex_dist = 0;

bool backlightOn = false;
esp_timer_handle_t backlightTimer;
esp_timer_handle_t lockTimer;

// ========== Button State ==========
bool lastButtonReading = HIGH;
bool buttonState = HIGH;
unsigned long lastDebounceTime = 0;