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
// #include <Wire.h>
#include <ESP32Servo.h>
// #include <LiquidCrystal_I2C.h>

//========= PIN DEFINITIONS =========
#define LED 1         ///< Output LED pin for anomaly alert
// #define LEDR 18       ///< Input photoresistor pin meant to read analog inputs
#define SERVO_PIN 37  ///< Output pin for the servo motor
#define POT_PIN 3
// #define SDA_PIN 20  ///< I2C Data Pin
// #define SCL_PIN 21  ///< I2C Clock Pin

// //========= LCD SETUP =========
// /**
//  * @brief 16x2 I2C LCD at address 0x27
//  */
// LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myservo;

//========= TASK HANDLES =========
TaskHandle_t TaskLED_Handle = NULL;
TaskHandle_t TaskServoRun_Handle = NULL;

//========= GLOBAL VARIABLES =========
static SemaphoreHandle_t semaphore;  ///< Semaphore for data synchronization

int currentAngle = 0;

//========= SETUP =========
/**
 * @brief Arduino setup function
 * @details 1. Initialize pins, serial, LCD, etc
 *          2. Create binary semaphore for synchronizing light level data.
 *          3. Create Tasks
 *          - Create the `Light Detector Task` and assign it to Core 0.
 *          - Create `LCD Task` and assign it to Core 0.
 *          - Create `Anomaly Alarm Task` and assign it to Core 1.
 *          - Create `Prime Calculation Task` and assign it to Core 1.
 *          4. Scheduler attempted before realizing that FreeRTOS automatically establishes a round robin system
 * Initializes peripherals, LCD, semaphore, and starts FreeRTOS tasks
 */
void setup() {
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  pinMode(POT_PIN, INPUT);

  ESP32PWM::allocateTimer(0);
	ESP32PWM::allocateTimer(1);
	ESP32PWM::allocateTimer(2);
	ESP32PWM::allocateTimer(3);
  myservo.setPeriodHertz(50);
  myservo.attach(SERVO_PIN, 500, 2400);
  myservo.write(0);
  delay(50);

  semaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(semaphore);
  xTaskCreatePinnedToCore(ServoRunTask, "servoRun", 2048, NULL, 1, &TaskServoRun_Handle, 1);
  xTaskCreatePinnedToCore(LEDBlinkTask, "AnomalyBlink", 2048, NULL, 1, &TaskLED_Handle, 0);
}


//========= LOOP =========
/**
 * @brief Empty main loop since tasks handle all operations
 */
void loop() {

}

//========= TASKS =========

void ServoRunTask(void *arg) {
  while (1) {
    // xSemaphoreTake(semaphore, portMAX_DELAY);
    currentAngle = analogRead(POT_PIN);
    Serial.print("Current input read: " + String(currentAngle));
    currentAngle = map(currentAngle, 0, 4096, 12, 180);  // scale it to use it with the servo (value between 0 and 180)
    // xSemaphoreGive(semaphore);

    Serial.println(". Corresponds to degrees: " + String(currentAngle));
    myservo.write(currentAngle);    // set the servo position according to the scaled value
    vTaskDelay(pdMS_TO_TICKS(10));  //creating a 0.5 second delay between each new read;                          // wait for the servo to get there
  }
}

void LEDBlinkTask(void *arg) {
  while (1) {
    // xSemaphoreTake(semaphore, portMAX_DELAY);
    if (currentAngle > 145 || currentAngle < 45) {
      for (int i = 0; i < 4; i++) {
        digitalWrite(LED, HIGH);
        vTaskDelay(250);
        digitalWrite(LED, LOW);
        vTaskDelay(250);
      }
    }
    // xSemaphoreGive(semaphore);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
