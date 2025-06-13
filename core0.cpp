/**
 * @file core0.cpp
 * @brief Core 0 Task Implementations for ESP32 Smart Monitoring System (EE590 Final Project)
 *
 * @section overview Overview
 * This file contains all task implementations assigned to Core 0 of the ESP32 in a 
 * FreeRTOS-based Smart Security and Anomaly Detection System. These tasks handle 
 * mechanical control (servo motor for locking), user interface (LCD display), and 
 * motion-based interactions (PIR motion sensor).
 * 
 * Key system features such as synchronized access to shared I2C devices, servo locking 
 * behavior, and timed backlight control are managed in this file using semaphores, 
 * software timers, and critical sections. 
 * 
 * The design allows responsive, concurrent behavior on a real-time operating system 
 * while reducing blocking and minimizing unnecessary I2C conflicts.
 *
 * @section system_tasks Tasks
 * - **ServoRunTask**: Controls the locking mechanism based on system state and notifications.
 * - **motionTask**: (debug) Continuously samples PIR sensor data and triggers UI updates and timers.
 * - **LCDTask**: Displays real-time sensor states and system mode using a shared I2C LCD.
 * - **updateButtonTask**: (debug) Provides manual override using a push button.
 *
 * @section concurrency Concurrency & Resources
 * - Uses semaphores (`i2c_semaphore`) to manage access to RTC and LCD on the I2C bus.
 * - Uses `esp_timer` to control servo locking delay and LCD backlight timeout.
 * - Employs critical sections with `portMUX_TYPE` for ISR-safe timer flag updates.
 *
 * @section author Author
 * Created by Sanjay Varghese, 2025  
 * Additionally modified by Sai Jayanth Kalisi, 2025  
 * Additionally modified by Ankit Telluri, 2025  
 */

//========= LIBRARIES =========
#include "Arduino.h"
#include "core0.h"
#include <Wire.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Wire.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include "driver/timer.h"
#include "global_defs.h"

//========= GLOBAL VARIABLES =========
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

//========= TASK FUNCTION DECLARATIONS =========
void ServoRunTask(void* arg);
void motionTask(void* pvParameters);
void LCDTask(void* arg);


//========= INTERRUPT SERVICE ROUTINES =========

/**
 * @brief ISR for lock timer expiration
 * @details Notifies the servo task to engage the lock.
 *          Runs in critical section.
 * @note Name: onLockTimer
 */
void IRAM_ATTR onLockTimer(void* arg) {
  portENTER_CRITICAL_ISR(&timerMux);
  isLock = true;
  xTaskNotifyGive(TaskServoRun_Handle);
  portEXIT_CRITICAL_ISR(&timerMux);
}

/**
 * @brief ISR for LCD backlight timeout
 * @details Turns off the backlight flag when the timer expires.
 * @note Name: onBacklightTimer
 */
void IRAM_ATTR onBacklightTimer(void* arg) {
  portENTER_CRITICAL_ISR(&timerMux);
  backlightOn = false;
  portEXIT_CRITICAL_ISR(&timerMux);
}

//========= TASK DEFINITIONS =========

/**
 * @brief (Deprecated) Debounced button monitor for lock toggle 
 * @details Reads a button pin, and if a rising edge is detected,
 *          it toggles the lock state and notifies the servo task.
 * @note Name: updateButtonTask
 */
void updateButtonTask(void* arg) {
  bool currentReading = digitalRead(BUTTON_PIN);

  if (currentReading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (currentReading != buttonState) {
      if (buttonState == LOW && currentReading == HIGH) {
        isLock = !isLock;
        xTaskNotifyGive(TaskServoRun_Handle);
      }
      buttonState = currentReading;
    }
  }
  lastButtonReading = currentReading;
}

/**
 * @brief Servo motor controller for lock mechanism
 * @details Waits on notification to engage/disengage lock and logs timestamp.
 *          Uses I2C to read RTC and control PWM via ESP32Servo.
 * @note Name: ServoRunTask
 */
void ServoRunTask(void* arg) {
  while (1) {
    // Wait for a notification to update servo position
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // clears notification

    if (isLock) {
      myservo.write(180);
      if (xSemaphoreTake(i2c_semaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
        DateTime now = rtc.now();
        Serial.printf("Time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
        xSemaphoreGive(i2c_semaphore);
      } else {
        Serial.println("RTC I2C timeout");
      }
      Serial.println("Servo locked");
    } else {
      myservo.write(0);
      if (xSemaphoreTake(i2c_semaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
        DateTime now = rtc.now();
        Serial.printf("Time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
        xSemaphoreGive(i2c_semaphore);
      } else {
        Serial.println("RTC I2C timeout");
      }
      Serial.println("Servo unlocked");
    }
  }
}

/**
 * @brief (Deprecated) PIR motion sampling and backlight trigger task
 * @details Samples PIR readings into a buffer and triggers LCD backlight timer.
 *          Logs activity timestamp via RTC.
 * @note Name: motionTask
 */
void motionTask(void* pvParameters) {
  // // Allow time for Serial + PIR to stabilize:
  // vTaskDelay(pdMS_TO_TICKS(200));

  // while (1) {
  //   // HIGH (1) means motion detected; LOW (0) means no motion.
  //   int state = digitalRead(PIR_PIN);

  //   // Store it in the circular buffer:
  //   motionBuffer[bufferIndex] = state;
  //   bufferIndex = (bufferIndex + 1) % 5;

  //   // Print the newest reading:
  //   // Serial.print("Newest state: ");
  //   // Serial.println(state == HIGH ? "MOTION" : "no motion");
  //   // Serial.print("   [Next write index = ");
  //   // Serial.print(bufferIndex);
  //   // Serial.println("]");

  //   // Print out the entire buffer (indices 0–4):
  //   // Serial.print("Buffer contents: [");
  //   int sum = 0;
  //   for (int i = 0; i < 5; i++) {
  //     // Serial.print(motionBuffer[i]);
  //     // if (i < 4) {
  //     // Serial.print(", ");
  //     // }
  //     sum += motionBuffer[i];
  //   }
  //   // Serial.println("]");

  //   if (sum > 2) {
  //     motion = true;
  //     if (!backlightOn) {
  //       backlightOn = true;
  //       if (xSemaphoreTake(i2c_semaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
  //         DateTime now = rtc.now();
  //         Serial.printf("Time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
  //         xSemaphoreGive(i2c_semaphore);
  //       } else {
  //         Serial.println("RTC I2C timeout");
  //       }
  //       Serial.println("Backlight ON (motion)");

  //       esp_timer_stop(backlightTimer);
  //       esp_timer_start_once(backlightTimer, 10000000);  // 10 sec = 10,000,000 us
  //     }
  //   } else {
  //     motion = false;
  //   }

  //   // 5) Delay 200 ms before the next sample:
  //   vTaskDelay(pdMS_TO_TICKS(200));
  // }
}

/**
 * @brief LCD display manager for lock and detection state
 * @details Updates LCD lines if state changes. Manages backlight state.
 *          Synchronizes I2C access using semaphore.
 * @note Name: LCDTask
 */
void LCDTask(void* arg) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  String prevLine0 = "", prevLine1 = "";

  while (1) {
    String line0 = (close_dist || motion_detected) ? "Detected" : "None";
    String line1 = isLock ? "Locked" : "Unlocked";

    if (xSemaphoreTake(i2c_semaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
      if (line0 != prevLine0) {
        lcd.setCursor(8, 0);
        lcd.print("        ");
        lcd.setCursor(8, 0);
        lcd.print(line0);
        prevLine0 = line0;
      }

      if (line1 != prevLine1) {
        lcd.setCursor(8, 1);
        lcd.print("        ");
        lcd.setCursor(8, 1);
        lcd.print(line1);
        prevLine1 = line1;
      }
      // Backlight control
      if (backlightOn) {
        lcd.backlight();
      } else {
        lcd.noBacklight();
      }

      xSemaphoreGive(i2c_semaphore);
    }

    vTaskDelay(pdMS_TO_TICKS(31.25));
  }
}
