
// Pin assignment (change to whatever GPIO you wired OUT to):
static const int PIR_PIN = 5;  // e.g. GPIO15

// Circular buffer to hold the last 5 PIR readings (0 = no motion, 1 = motion):
int motionBuffer[5] = { 0, 0, 0, 0, 0 };
volatile int bufferIndex = 0;  // always 0…4

// Forward declaration of the FreeRTOS task:
void motionTask(void* pvParameters);

void setup() {
  // Initialize serial at 115200 for debugging:
  Serial.begin(115200);
  while (!Serial) { /* wait for Serial */ }

  // Configure PIR pin as input:
  pinMode(PIR_PIN, INPUT);

  // Create and run the motionTask on Core 1 with priority 1:
  xTaskCreatePinnedToCore(
    motionTask,     // Task function
    "MotionTask",   // Name (for debugging)
    2048,           // Stack size (bytes)
    nullptr,        // Parameter (none)
    1,              // Priority (0…configMAX_PRIORITIES−1)
    nullptr,        // Task handle (not used here)
    1               // Run on core 1 (ESP32 has core 0 and core 1)
  );
}

void loop() {
  // Nothing here—motionTask does the work.
  vTaskDelay(pdMS_TO_TICKS(1000));
}

//--------------------------------------
// motionTask: reads PIR pin, stores into buffer, prints buffer
//--------------------------------------
void motionTask(void* pvParameters) {
  (void) pvParameters;  // unused

  // Allow time for Serial + PIR to stabilize:
  vTaskDelay(pdMS_TO_TICKS(2000));

  while (1) {
    // 1) Read the digital output of HC-SR501:
    //    HIGH (1) means motion detected; LOW (0) means no motion.
    int state = digitalRead(PIR_PIN);

    // 2) Store it in the circular buffer:
    motionBuffer[bufferIndex] = state;
    bufferIndex = (bufferIndex + 1) % 5;

    // 3) Print the newest reading:
    Serial.print("Newest state: ");
    Serial.print(state == HIGH ? "MOTION" : "no motion");
    Serial.print("   [Next write index = ");
    Serial.print(bufferIndex);
    Serial.println("]");

    // 4) Print out the entire buffer (indices 0–4):
    Serial.print("Buffer contents: [");
    for (int i = 0; i < 5; i++) {
      Serial.print(motionBuffer[i]);
      if (i < 4) {
        Serial.print(", ");
      }
    }
    Serial.println("]");

    // 5) Delay 200 ms before the next sample:
    vTaskDelay(pdMS_TO_TICKS(2000));
  }

}

