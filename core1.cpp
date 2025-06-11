#include <Arduino.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string>

#include "core1.h"
#include "global_defs.h"

//========= TASKS =========

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

    if (sum < 90 && sum != 0) {
      close_dist = true;

      // Reset inactivity timer
      if (!backlightOn) {
        backlightOn = true;
        // Serial.println(sum);
        if (xSemaphoreTake(i2c_semaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
          DateTime now = rtc.now();
          Serial.printf("Time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
          xSemaphoreGive(i2c_semaphore);
        } else {
          Serial.println("RTC I2C timeout");
        }
        Serial.println("Backlight ON (proximity)");

        esp_timer_stop(backlightTimer);
        esp_timer_start_once(backlightTimer, 10000000);  // 10 sec = 10,000,000 us
      }

      // timerStop(backlightTimer);
      // timerWrite(backlightTimer, 0);
      // timerStart(backlightTimer);
    } else {
      close_dist = false;
    }

    // Serial.println("]");

    vTaskDelay(pdMS_TO_TICKS(200));
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