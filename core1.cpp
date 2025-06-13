/**
 * @file core1.cpp
 * @brief FreeRTOS task implementations for Core 1 of the Smart Security Door System.
 *
 * @details
 * This file defines and implements all Core 1 tasks that handle sensor reading,
 * proximity/motion detection, RFID tag reading, and access control logic.
 * It operates as part of a dual-core system on the ESP32 microcontroller.
 *
 * Core responsibilities:
 * - **sensorReadTask**: Collects ultrasonic and PIR sensor data at 50Hz and sends it via queue.
 * - **sensorProcessTask**: Aggregates sensor readings into buffers, determines detection events, and handles the backlight.
 * - **taskRFIDReader**: Reads RFID tag UIDs and passes them into a queue for validation.
 * - **taskPrinter**: Validates UID access, logs attempts, controls lock state, and manages LCD backlight timers.
 * - **distanceTask**: (Debug) Continuously samples Ultrasonic sensor data and triggers UI updates and timers.
 * 
 * @section Features
 * - Real-time sensor polling with timing guarantees using `vTaskDelayUntil`.
 * - Queue-based communication between reader and processor tasks.
 * - Uses ESP32's `esp_timer` library for one-shot timers to control lock duration and backlight timeout.
 * - Incorporates I²C semaphore protection for safe RTC access.
 *
 * @section Authors
 * Created by Sanjay Varghese, 2025  
 * Additionally modified by Sai Jayanth Kalisi, 2025  
 * Additionally modified by Ankit Telluri, 2025
 */

#include <Arduino.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include "core1.h"
#include "global_defs.h"

//========= TASKS =========

/**
 * @brief (Deprecated) Reads ultrasonic distance values in a blocking loop.
 *        Stores results into a circular buffer and manages backlight logic.
 *        Replaced by non-blocking sensorReadTask and sensorProcessTask.
 */
void distanceTask(void* pvParameters) {
  // while (1) {
  //   // 1) Trigger pulse (HIGH for 10 µs):
  //   digitalWrite(TRIG_PIN, LOW);
  //   delayMicroseconds(2);
  //   digitalWrite(TRIG_PIN, HIGH);
  //   delayMicroseconds(10);
  //   digitalWrite(TRIG_PIN, LOW);

  //   // 2) Read echo pulse width (blocking). Max timeout = 38 000 µs (~6.5 m)
  //   unsigned long duration = pulseIn(ECHO_PIN, HIGH, 38000UL);

  //   // If no echo was received within timeout, duration = 0:
  //   float distanceCm = 0.0f;
  //   // Serial.println(duration);
  //   if (duration > 0) {
  //     // 3) Convert to distance in cm: (pulse_time µs) × (speed_cm/µs) ÷ 2
  //     distanceCm = (duration * SOUND_SPEED_CM_PER_US) / 2.0f;
  //   }

  //   // 4) Store in circular buffer:
  //   distanceBuffer[bufferIndex_dist] = distanceCm;
  //   bufferIndex_dist = (bufferIndex_dist + 1) % 5;

  //   // 5) Print the most recent reading:
  //   // Serial.print("Newest distance: ");
  //   // Serial.print(distanceCm, 2);
  //   // Serial.println(distanceCm < 130 ? "CLOSE" : "far");
  //   // Serial.print(" cm   [Next write index = ");
  //   // Serial.print(bufferIndex_dist);
  //   // Serial.println("]");

  //   // 6) Print out the entire buffer contents (indices 0–4):

  //   int sum = 0;

  //   // Serial.print("Buffer contents: [");
  //   for (int i = 0; i < 5; i++) {
  //     // Serial.print(distanceBuffer[i], 2);
  //     // if (i < 4) {
  //     // Serial.print(", ");
  //     // }
  //     sum += distanceBuffer[i];
  //   }

  //   if (sum < 90 && sum != 0) {
  //     close_dist = true;

  //     // Reset inactivity timer
  //     if (!backlightOn) {
  //       backlightOn = true;
  //       // Serial.println(sum);
  //       if (xSemaphoreTake(i2c_semaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
  //         DateTime now = rtc.now();
  //         Serial.printf("Time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
  //         xSemaphoreGive(i2c_semaphore);
  //       } else {
  //         Serial.println("RTC I2C timeout");
  //       }
  //       Serial.println("Backlight ON (proximity)");

  //       esp_timer_stop(backlightTimer);
  //       esp_timer_start_once(backlightTimer, 10000000);  // 10 sec = 10,000,000 us
  //     }

  //     // timerStop(backlightTimer);
  //     // timerWrite(backlightTimer, 0);
  //     // timerStart(backlightTimer);
  //   } else {
  //     close_dist = false;
  //   }

  //   // Serial.println("]");

  //   vTaskDelay(pdMS_TO_TICKS(200));
  // }
}

/**
 * @brief Periodically triggers ultrasonic distance measurements and reads PIR sensor state.
 *
 * @details 
 * - Triggered every 20ms (50Hz).
 * - Sends sensor readings (`sensorData_t`) to `sensorQueue`.
 * - Uses task notifications for echo pulse timing instead of `pulseIn()`.
 *
 * @param pvParameters Unused
 */
void sensorReadTask(void* pvParameters) {
  sensorData_t data;
  TickType_t xLastWakeTime;
  xLastWakeTime = xTaskGetTickCount();
  while (1) {
    // — Ultrasonic trigger pulse —
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // — Measure echo with timeout ~3.4 m (20 000 µs) —
    // unsigned long dur = pulseIn(ECHO_PIN, HIGH, 38000UL);
    uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));

    uint32_t dur = 0;
    if (notified) {
      // safe because micros() rolls over only every ~71 mins
      dur = echo_end_us - echo_start_us;
    }
    data.distanceCm = (dur > 0)
                        ? (dur * SOUND_SPEED_CM_PER_US) / 2.0f
                        : 0.0f;
    if (data.distanceCm > 400.0f) data.distanceCm = 0.0f;

    // — PIR reading —
    data.motionState = digitalRead(PIR_PIN);

    // — Send both readings to processing task —
    xQueueSend(sensorQueue, &data, portMAX_DELAY);

    // — pace readings at 20 ms intervals — 50Hz
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(20));
  }
}

/**
 * @brief Processes buffered sensor readings and triggers appropriate system behavior.
 *
 * @details
 * - Computes rolling average of distance and motion buffer.
 * - Determines `close_dist` and `motion_detected` flags.
 * - Turns on LCD backlight on detection and logs event using RTC with semaphore.
 *
 * @param pvParameters Unused
 */
void sensorProcessTask(void* pvParameters) {
  sensorData_t d;
  while (1) {
    if (xQueueReceive(sensorQueue, &d, portMAX_DELAY) == pdTRUE) {
      // 1) update circular buffers
      distanceBuffer[idx_dist] = d.distanceCm;
      idx_dist = (idx_dist + 1) % 5;

      motionBuffer[idx_motion] = (d.motionState == HIGH);
      idx_motion = (idx_motion + 1) % 5;

      // 2) compute sums
      float sumDist = 0;
      int sumMotion = 0;
      // Serial.print("Distances: [");
      for (int i = 0; i < 5; i++) {
        // Serial.print(distanceBuffer[i], 2);
        // if (i < 4) Serial.print(", ");
        sumDist += distanceBuffer[i];
      }
      // Serial.print("]  Motion: [");
      for (int i = 0; i < 5; i++) {
        // Serial.print(motionBuffer[i]);
        // if (i < 4) Serial.print(", ");
        sumMotion += motionBuffer[i];
      }
      // Serial.println("]");

      // 3) proximity logic
      if (sumDist < 90.0f && sumDist > 0.0f) {
        close_dist = true;
      } else {
        close_dist = false;
      }

      // 4) motion logic
      if (sumMotion > 2) {
        motion_detected = true;
      } else {
        motion_detected = false;
      }

      // 5) backlight handling
      if ((close_dist || motion_detected) && !backlightOn) {
        backlightOn = true;
        if (xSemaphoreTake(i2c_semaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
          DateTime now = rtc.now();
          Serial.printf("Time: %02d:%02d:%02d\n",
                        now.hour(), now.minute(), now.second());
          Serial.println(close_dist ? "Distance" : "Motion");
          xSemaphoreGive(i2c_semaphore);
        }
        Serial.println("Backlight ON");
        esp_timer_stop(backlightTimer);
        esp_timer_start_once(backlightTimer, 10 * 1000 * 1000);  // 10 s
      }
    }
  }
}

/**
 * @brief Polls the RFID reader for new tag UIDs and sends them to `rfidQueue`.
 *
 * @details
 * - Checks for new cards using MFRC522 interface.
 * - Extracts and formats UID into a string and sends to queue.
 * - Ensures card halting and crypto session termination.
 *
 * @param pvParameters Unused
 */
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

/**
 * @brief Processes UIDs from `rfidQueue` and manages access control.
 *
 * @details
 * - Compares received UID to allowed list.
 * - If authorized:
 *   - Unlocks system.
 *   - Notifies `ServoRunTask`.
 *   - Starts/reset lock and backlight timers.
 * - If unauthorized:
 *   - Logs timestamp and denies access.
 * - Avoids redundant unlocks from repeated scans.
 *
 * @param pvParameters Unused
 */
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