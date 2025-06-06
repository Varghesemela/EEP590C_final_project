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
#include <LCD_I2C.h>

#include "global_defs.h"

//========= TASK FUNCTION DECLARATIONS =========
void ServoRunTask(void* arg);
void motionTask(void* pvParameters);
void LCDTask(void* arg);

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

void taskRFIDReader(void *pvParameters) {
  byte lastUID[4] = {0};

  while (1) {
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      bool isNew = false;

      for (int i = 0; i < 4; i++) {
        if (rfid.uid.uidByte[i] != lastUID[i]) {
          isNew = true;
          break;
        }
      }

      if (isNew) {
        char uidStr[20];
        snprintf(uidStr, sizeof(uidStr), "%02X %02X %02X %02X",
                 rfid.uid.uidByte[0], rfid.uid.uidByte[1],
                 rfid.uid.uidByte[2], rfid.uid.uidByte[3]);

        // Update last UID
        memcpy(lastUID, rfid.uid.uidByte, 4);

        // Send UID string to the queue
        if (xQueueSend(rfidQueue, &uidStr, pdMS_TO_TICKS(100)) != pdPASS) {
          Serial.println("Queue full! Dropping tag.");
        }
      }

      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
    }

    vTaskDelay(pdMS_TO_TICKS(500)); // Check every 500ms
  }
}

void taskPrinter(void *pvParameters) {
  char receivedUID[20];

  while (1) {
    if (xQueueReceive(rfidQueue, &receivedUID, portMAX_DELAY) == pdPASS) {
      Serial.print("New RFID Tag: ");
      Serial.println(receivedUID);
    }
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

void LCDTask(void* arg) {
  while (1) {
    lcd.setCursor(8, 0);
    lcd.print("        ");
    lcd.setCursor(8, 0);
    lcd.print((close_dist || motion) ? "Detected" : "None");

    lcd.setCursor(8, 1);
    lcd.print("        ");
    lcd.setCursor(8, 1);
    lcd.print(isLock ? "Locked" : "Unlocked");

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}