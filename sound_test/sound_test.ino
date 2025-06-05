

static const int SOUND_PIN = 18;  // Connect W104 “OUT” to GPIO18

// Circular buffer to hold the last 5 readings (0 or 1):
int soundBuffer[5] = { 0, 0, 0, 0, 0 };
volatile int bufferIndex = 0;  // Always 0…4

void soundTask(void* pvParameters);

void setup() {
  Serial.begin(115200);
  while (!Serial) {  }

  // Configure the sound‐sensor pin as input:
  pinMode(SOUND_PIN, INPUT);

  // Create and run the soundTask on Core 1 with priority 1:
  xTaskCreatePinnedToCore(
    soundTask,     // Task function
    "SoundTask",   // Name (for debugging)
    2048,          // Stack size in bytes
    nullptr,       // Parameter (none)
    1,             // Priority (0 = low … configMAX_PRIORITIES−1 = high)
    nullptr,       // Task handle (not used here)
    1              // Run on core 1 (ESP32 has core 0 and core 1)
  );
}

void loop() {
  // Nothing in loop(); the FreeRTOS task handles everything.
}

//--------------------------------------
// soundTask: reads W104 OUT pin, stores into buffer, prints buffer
//--------------------------------------
void soundTask(void* pvParameters) {
  (void) pvParameters;  // unused parameter

  // Give time for setup/Serial to settle:
  vTaskDelay(pdMS_TO_TICKS(200));

  while (1) {
    // 1) Read the digital output of the W104:
    //    HIGH (1) means sound exceeded threshold; LOW (0) means below threshold.
    int sensorState = digitalRead(SOUND_PIN);

    // 2) Store it in the circular buffer:
    soundBuffer[bufferIndex] = sensorState;
    bufferIndex = (bufferIndex + 1) % 5;

    // 3) Print the newest reading:
    Serial.print("Newest state: ");
    Serial.print(sensorState == HIGH ? "LOUD" : "quiet");
    Serial.print("   [Next write index = ");
    Serial.print(bufferIndex);
    Serial.println("]");

    // 4) Print out the entire buffer contents (indices 0–4):
    Serial.print("Buffer contents: [");
    for (int i = 0; i < 5; i++) {
      Serial.print(soundBuffer[i]);
      if (i < 4) {
        Serial.print(", ");
      }
    }
    Serial.println("]");

    vTaskDelay(pdMS_TO_TICKS(1000));
  }


}
