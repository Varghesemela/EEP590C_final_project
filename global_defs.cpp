#include "global_defs.h"

// ========== Pin Definitions ==========
const int LED = 1;
const int SERVO_PIN = 12;
const int POT_PIN = 17;
const int BUTTON_PIN = 35;
const int SDA_PIN = 20;
const int SCL_PIN = 21;
const int PIR_PIN = 5;
const int SOUND_PIN = 18;
const int ECHO_PIN = 40;
const int TRIG_PIN = 41;

// Define pins for MFRC522
const int SS_PIN = 10;
const int RST_PIN = 14;

// Queue handle
QueueHandle_t rfidQueue;
// ========== Constants ==========
const float SOUND_SPEED_CM_PER_US = 0.0343f;
const unsigned long debounceDelay = 50;

// ========== Peripheral Objects ==========
LCD_I2C lcd(0x27, 16, 2);
Servo myservo;
MFRC522 rfid(SS_PIN, RST_PIN); // RFID instance

// ========== State Flags ==========
volatile bool motion = false;
volatile bool sound = false;
volatile bool close_dist = false;
volatile bool isLock = false;

// ========== Buffers and Indices ==========
int motionBuffer[5] = {0};
int soundBuffer[5] = {0};
float distanceBuffer[5] = {0.0f};

volatile int bufferIndex = 0;
volatile int bufferIndex_sound = 0;
volatile int bufferIndex_dist = 0;

// ========== Button State ==========
bool lastButtonReading = HIGH;
bool buttonState = HIGH;
unsigned long lastDebounceTime = 0;