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

    if (sum < 100) {
      close_dist = true;
    } else {
      close_dist = false;
    }
    // Serial.println("]");

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void rtcTask(void *args) {
  TickType_t lastWakeTime = xTaskGetTickCount();
  const TickType_t interval = pdMS_TO_TICKS(120);

  while (1) {
    if (xSemaphoreTake(i2c_semaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
      DateTime now = rtc.now();
      Serial.printf("Time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
      xSemaphoreGive(i2c_semaphore);
    } else {
      Serial.println("RTC I2C timeout");
    }

    vTaskDelayUntil(&lastWakeTime, interval);
  }
}