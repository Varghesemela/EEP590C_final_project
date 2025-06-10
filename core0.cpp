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

void updateButton() {
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

void taskRFIDReader(void* pvParameters) {
  while (1) {
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      char uidStr[20];
      snprintf(uidStr, sizeof(uidStr), "%02X %02X %02X %02X",
               rfid.uid.uidByte[0], rfid.uid.uidByte[1],
               rfid.uid.uidByte[2], rfid.uid.uidByte[3]);

      // Always send UID to queue, regardless of change
      if (xQueueSend(rfidQueue, &uidStr, pdMS_TO_TICKS(100)) != pdPASS) {
        Serial.println("Queue full! Dropping tag.");
      }

      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void taskPrinter(void* pvParameters) {
  char receivedUID[20];
  char lastUID[20] = "";

  while (1) {
    if (xQueueReceive(rfidQueue, &receivedUID, portMAX_DELAY) == pdPASS) {
      // Check if UID is allowed
      bool isAllowed = false;
      for (int i = 0; i < numAllowedUIDs; ++i) {
        if (strcmp(receivedUID, allowedUIDs[i]) == 0) {
          isAllowed = true;
          break;
        }
      }

      if (isAllowed) {
        // Avoid printing duplicates
        int sameIDscanned = strcmp(receivedUID, lastUID);
        if ((sameIDscanned != 0) && isLock) {
          if (xSemaphoreTake(i2c_semaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
            DateTime now = rtc.now();
            Serial.printf("Time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
            xSemaphoreGive(i2c_semaphore);
          } else {
            Serial.println("RTC I2C timeout");
          }
          Serial.print("Access Granted. UID: ");
          Serial.println(receivedUID);
          isLock = false;
          xTaskNotifyGive(TaskServoRun_Handle);
          // Reset timer when RFID grants access
          esp_timer_stop(lockTimer);
          esp_timer_start_once(lockTimer, 10000000);  // 10 sec = 10,000,000 us

          // Reset inactivity timer
          if (!backlightOn) {
            backlightOn = true;
            Serial.println("Backlight ON (RFID update)");
            esp_timer_stop(backlightTimer);
            esp_timer_start_once(backlightTimer, 10000000);  // 10 sec = 10,000,000 us
          }
          strncpy(lastUID, receivedUID, sizeof(lastUID));
        } else {
          if (isLock) {
            isLock = false;
            xTaskNotifyGive(TaskServoRun_Handle);
            // Reset timer when RFID grants access
            esp_timer_stop(lockTimer);
            esp_timer_start_once(lockTimer, 10000000);  // 10 sec = 10,000,000 us

            strncpy(lastUID, receivedUID, sizeof(lastUID));
          } else {
            if (xSemaphoreTake(i2c_semaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
              DateTime now = rtc.now();
              Serial.printf("Time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
              xSemaphoreGive(i2c_semaphore);
            } else {
              Serial.println("RTC I2C timeout");
            }
            Serial.println("Same tag re-scanned, and already unlocked. Ignoring...");
          }
        }
      } else {
        isLock = true;

        //take semaphore and log in time
        if (xSemaphoreTake(i2c_semaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
          DateTime now = rtc.now();
          Serial.printf("Time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
          xSemaphoreGive(i2c_semaphore);
        } else {
          Serial.println("RTC I2C timeout");
        }
        Serial.print("Access Denied. Unknown UID: ");
        Serial.println(receivedUID);
      }
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
