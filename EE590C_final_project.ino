#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>

// Define pins for MFRC522
#define SS_PIN 10
#define RST_PIN 14

MFRC522 rfid(SS_PIN, RST_PIN); // RFID instance

// Queue handle
QueueHandle_t rfidQueue;

// Function prototypes
void taskRFIDReader(void *pvParameters);
void taskPrinter(void *pvParameters);

void setup() {
  Serial.begin(115200); // Use higher baud for ESP32

  // Init RFID module
  SPI.begin();
  rfid.PCD_Init();
  Serial.println(F("FreeRTOS RFID System Starting..."));

  // Create a queue with room for 5 strings of 20 chars each
  rfidQueue = xQueueCreate(5, sizeof(char[20]));
  if (rfidQueue == NULL) {
    Serial.println("Error creating queue!");
    while (1);
  }

  // Create tasks
  xTaskCreate(taskRFIDReader, "RFID Reader", 4096, NULL, 1, NULL);
  xTaskCreate(taskPrinter, "Printer", 2048, NULL, 1, NULL);
}

void loop() {
  // Nothing here—tasks are running FreeRTOS
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