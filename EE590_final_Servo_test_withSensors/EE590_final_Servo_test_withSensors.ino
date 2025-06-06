/**
 * @file EE590 Final project Servo Test.ino
 * @brief FreeRTOS-based Light Monitoring and Anomaly Detection System using ESP32
 *
 * @mainpage EE590 Final Project
 *
 * @section overview Overview
 * This sketch reads light intensity using a photoresistor, smooths the readings using a
 * sliding window average, detects anomalies, displays real-time values on an I2C LCD,
 * and concurrently calculates prime numbers using FreeRTOS tasks pinned to ESP32 cores.
 *
 * @section author Author
 * Created by Sai Jayanth Kalisi, 2025
 *
 */

//========= LIBRARIES =========
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <Wire.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>

//========= PIN DEFINITIONS =========
#define LED 1  ///< Output LED pin for anomaly alert
// #define LEDR 18       ///< Input photoresistor pin meant to read analog inputs
#define SERVO_PIN 12  ///< Output pin for the servo motor
#define POT_PIN 17
#define BUTTON_PIN 35
#define SDA_PIN 20    ///< I2C Data Pin
#define SCL_PIN 21    ///< I2C Clock Pin
#define PIR_PIN 5     ///< PIR PIN
#define SOUND_PIN 18  ///< Sound pin
#define ECHO_PIN 40   ///< echo pin
#define TRIG_PIN 41   ///< trig Pin

// //========= LCD SETUP =========
// /**
//  * @brief 16x2 I2C LCD at address 0x27
//  */
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myservo;

//========= TASK HANDLES =========
TaskHandle_t TaskLED_Handle = NULL;
TaskHandle_t TaskServoRun_Handle = NULL;
TaskHandle_t TaskMotion_Handle = NULL;
TaskHandle_t TaskSound_Handle = NULL;
TaskHandle_t TaskLCD_Handle = NULL;
TaskHandle_t TaskUltraSonic_Handle = NULL;

//========= GLOBAL VARIABLES =========
// static SemaphoreHandle_t semaphore;  ///< Semaphore for data synchronization

// Circular buffer to hold the last 5 PIR readings (0 = no motion, 1 = motion):
int motionBuffer[5] = { 0, 0, 0, 0, 0 };
int soundBuffer[5] = { 0, 0, 0, 0, 0 };
float distanceBuffer[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
volatile int bufferIndex_dist = 0;   // for distance
volatile int bufferIndex_sound = 0;  // for sound
volatile int bufferIndex = 0;        // for PIR

int currentAngle = 0;
volatile bool motion = false;
volatile bool sound = false;
volatile bool close_dist = false;

static const float SOUND_SPEED_CM_PER_US = 0.0343f;  // Speed of sound in air ≈ 343 m/s → 0.0343 cm/µs

//Handling locking mechanism with buttons
volatile bool isLock = false;
bool lastButtonReading = HIGH;
bool buttonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

//========= SETUP =========
/**
 * @brief Arduino setup function
 * @details 1. Initialize pins, serial, LCD, etc
 */
void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(LED, OUTPUT);
  pinMode(POT_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(SOUND_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Person: ");
  lcd.setCursor(0, 1);
  lcd.print("State: ");

  ledcAttach(LED, 100, 12);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  myservo.setPeriodHertz(50);
  myservo.attach(SERVO_PIN, 500, 2400);
  myservo.write(0);
  delay(50);

  // xTaskCreatePinnedToCore(ServoRunTask, "servoRun", 2048, NULL, 1, &TaskServoRun_Handle, 0);
  // xTaskCreatePinnedToCore(motionTask, "MotionTask", 2048, nullptr, 1, &TaskMotion_Handle, 1);
  xTaskCreatePinnedToCore(soundTask, "SoundTask", 2048, nullptr, 1, &TaskSound_Handle, 1);
  // xTaskCreatePinnedToCore(LCDTask, "LCDTask", 2048, nullptr, 1, &TaskLCD_Handle, 0);
  // xTaskCreatePinnedToCore(distanceTask, "UltraSonicTask", 2048, nullptr, 1, &TaskUltraSonic_Handle, 1);
}


//========= LOOP =========
/**
 * @brief Empty main loop since tasks handle all operations
 */
void loop() {
}

//========= TASKS =========
void updateButton() {
  bool currentReading = digitalRead(BUTTON_PIN);

  if (currentReading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (currentReading != buttonState) {
      if (buttonState == LOW && currentReading == HIGH) {
        isLock = !isLock;
      }
      buttonState = currentReading;
    }
  }
  lastButtonReading = currentReading;
}

void ServoRunTask(void* arg) {
  while (1) {
    updateButton();

    if (isLock) {
      myservo.write(180);
      // lcd.clear();
      // lcd.print("State: Lock");
    } else {
      myservo.write(0);
      // lcd.clear();
      // lcd.print("State: Unlock");
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void motionTask(void* pvParameters) {
  // Allow time for Serial + PIR to stabilize:
  vTaskDelay(pdMS_TO_TICKS(200));

  while (1) {
    // 1) Read the digital output of HC-SR501:
    //    HIGH (1) means motion detected; LOW (0) means no motion.
    int state = digitalRead(PIR_PIN);

    // 2) Store it in the circular buffer:
    motionBuffer[bufferIndex] = state;
    bufferIndex = (bufferIndex + 1) % 5;

    // 3) Print the newest reading:
    // Serial.print("Newest state: ");
    // Serial.println(state == HIGH ? "MOTION" : "no motion");
    // Serial.print("   [Next write index = ");
    // Serial.print(bufferIndex);
    // Serial.println("]");

    // 4) Print out the entire buffer (indices 0–4):
    // Serial.print("Buffer contents: [");
    int sum = 0;
    for (int i = 0; i < 5; i++) {
      // Serial.print(motionBuffer[i]);
      // if (i < 4) {
      // Serial.print(", ");
      // }
      sum += motionBuffer[i];
    }
    // Serial.println("]");

    if (sum > 2) {
      motion = true;
    } else {
      motion = false;
    }

    // 5) Delay 200 ms before the next sample:
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void soundTask(void* pvParameters) {
  while (1) {
    // 1) Read the digital output of the W104:
    //    HIGH (1) means sound exceeded threshold; LOW (0) means below threshold.
    int sensorState = analogRead(SOUND_PIN);

    // 2) Store it in the circular buffer:
    soundBuffer[bufferIndex_sound] = sensorState;

    ledcWrite(LED, sensorState);
    bufferIndex_sound = (bufferIndex_sound + 1) % 5;

    // 3) Print the newest reading:
    // Serial.print("Newest state: ");
    // Serial.println(sensorState > 400 ? "LOUD" : "quiet");
    // Serial.print("   [Next write index = ");
    // Serial.print(bufferIndex_sound);
    // Serial.println("]");

    // 4) Print out the entire buffer contents (indices 0–4):
    int sum = 0;
    // Serial.print("Buffer contents: [");
    for (int i = 0; i < 5; i++) {
      Serial.println(soundBuffer[i]);
      // if (i < 4) {
      // Serial.print(", ");
      // }
      sum += soundBuffer[i];
    }
    // Serial.println("]");

    if (sum > 2000) {
      sound = true;
    } else {
      sound = false;
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void LCDTask(void* arg) {
  while (1) {
    lcd.setCursor(8, 0);
    lcd.print("        ");
    lcd.setCursor(8, 0);
    if (close_dist | motion) {
      lcd.print("Detected");
    } else {
      lcd.print("None");
    }
    lcd.setCursor(8, 1);
    lcd.print("        ");
    lcd.setCursor(8, 1);
    if (isLock) {
      lcd.print("Locked");
    } else {
      lcd.print("Unlocked");
    }

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void distanceTask(void* pvParameters) {
  while (1) {
    // 1) Trigger pulse (HIGH for 10 µs):
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // 2) Read echo pulse width (blocking). Max timeout = 38 000 µs (~6.5 m)
    unsigned long duration = pulseIn(ECHO_PIN, HIGH, 38000UL);

    // If no echo was received within timeout, duration = 0:
    float distanceCm = 0.0f;
    // Serial.println(duration);
    if (duration > 0) {
      // 3) Convert to distance in cm: (pulse_time µs) × (speed_cm/µs) ÷ 2
      distanceCm = (duration * SOUND_SPEED_CM_PER_US) / 2.0f;
    }

    // 4) Store in circular buffer:
    distanceBuffer[bufferIndex_dist] = distanceCm;
    bufferIndex_dist = (bufferIndex_dist + 1) % 5;

    // 5) Print the most recent reading:
    // Serial.print("Newest distance: ");
    // Serial.print(distanceCm, 2);
    // Serial.println(distanceCm < 130 ? "CLOSE" : "far");
    // Serial.print(" cm   [Next write index = ");
    // Serial.print(bufferIndex_dist);
    // Serial.println("]");

    // 6) Print out the entire buffer contents (indices 0–4):

    int sum = 0;

    // Serial.print("Buffer contents: [");
    for (int i = 0; i < 5; i++) {
      // Serial.print(distanceBuffer[i], 2);
      // if (i < 4) {
      // Serial.print(", ");
      // }
      sum += distanceBuffer[i];
    }

    if (sum < 100) {
      close_dist = true;
    } else {
      close_dist = false;
    }
    // Serial.println("]");

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}