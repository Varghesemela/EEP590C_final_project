/**
 * @file   core0.cpp
 * @brief  core 0 tasks
 * @author Sanjay
 * @date   05/24/2025
 *
 * Implements byte transmission, commands, cursor control, and text printing.
 */

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

portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

//========= TASK FUNCTION DECLARATIONS =========
void ServoRunTask(void* arg);
void motionTask(void* pvParameters);
void LCDTask(void* arg);

void IRAM_ATTR onLockTimer(void* arg) {
  portENTER_CRITICAL_ISR(&timerMux);
  isLock = true;
  xTaskNotifyGive(TaskServoRun_Handle);
  portEXIT_CRITICAL_ISR(&timerMux);
}

void IRAM_ATTR onBacklightTimer(void* arg) {
  portENTER_CRITICAL_ISR(&timerMux);
  backlightOn = false;
  portEXIT_CRITICAL_ISR(&timerMux);
}

//========= TASK DEFINITIONS =========

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

void ServoRunTask(void* arg) {
  while (1) {
    // Wait for a notification to update servo position
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // clears notification

    if (isLock) {
      myservo.write(180);
      Serial.println("Servo locked");
    } else {
      myservo.write(0);
      Serial.println("Servo unlocked");
    }
  }
}

void motionTask(void* pvParameters) {
  // Allow time for Serial + PIR to stabilize:
  vTaskDelay(pdMS_TO_TICKS(200));

  while (1) {
    // HIGH (1) means motion detected; LOW (0) means no motion.
    int state = digitalRead(PIR_PIN);

    // Store it in the circular buffer:
    motionBuffer[bufferIndex] = state;
    bufferIndex = (bufferIndex + 1) % 5;

    // Print the newest reading:
    // Serial.print("Newest state: ");
    // Serial.println(state == HIGH ? "MOTION" : "no motion");
    // Serial.print("   [Next write index = ");
    // Serial.print(bufferIndex);
    // Serial.println("]");

    // Print out the entire buffer (indices 0–4):
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
      if (!backlightOn) {
        backlightOn = true;
        if (xSemaphoreTake(i2c_semaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
          DateTime now = rtc.now();
          Serial.printf("Time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
          xSemaphoreGive(i2c_semaphore);
        } else {
          Serial.println("RTC I2C timeout");
        }
        Serial.println("Backlight ON (motion)");

        esp_timer_stop(backlightTimer);
        esp_timer_start_once(backlightTimer, 10000000);  // 10 sec = 10,000,000 us
      }
    } else {
      motion = false;
    }

    // 5) Delay 200 ms before the next sample:
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void LCDTask(void* arg) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  String prevLine0 = "", prevLine1 = "";

  while (1) {
    String line0 = (close_dist || motion) ? "Detected" : "None";
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

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}
