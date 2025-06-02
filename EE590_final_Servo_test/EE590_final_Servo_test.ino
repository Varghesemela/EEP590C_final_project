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
#define SDA_PIN 20  ///< I2C Data Pin
#define SCL_PIN 21  ///< I2C Clock Pin

// //========= LCD SETUP =========
// /**
//  * @brief 16x2 I2C LCD at address 0x27
//  */
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myservo;

//========= TASK HANDLES =========
TaskHandle_t TaskLED_Handle = NULL;
TaskHandle_t TaskServoRun_Handle = NULL;

//========= GLOBAL VARIABLES =========
// static SemaphoreHandle_t semaphore;  ///< Semaphore for data synchronization

int currentAngle = 0;

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
  pinMode(LED, OUTPUT);
  pinMode(POT_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  myservo.setPeriodHertz(50);
  myservo.attach(SERVO_PIN, 500, 2400);
  myservo.write(0);
  delay(50);

  xTaskCreatePinnedToCore(ServoRunTask, "servoRun", 2048, NULL, 1, &TaskServoRun_Handle, 1);
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

void ServoRunTask(void *arg) {
  while (1) {
    updateButton();

    if(isLock) {
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