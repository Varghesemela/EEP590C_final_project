
static const int TRIG_PIN =  5;   
static const int ECHO_PIN = 18;

// Speed of sound in air ≈ 343 m/s → 0.0343 cm/µs
static const float SOUND_SPEED_CM_PER_US = 0.0343f;

// Circular buffer to hold the last 5 distance readings (in cm):
float distanceBuffer[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
volatile int bufferIndex = 0;  // Always 0…4

void sensorTask(void* pvParameters);

void setup() {
  // Initialize serial port for debugging:
  Serial.begin(115200);
  while (!Serial) { /* wait for Serial to be ready */ }

  // Configure pins:
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Make sure TRIG starts LOW:
  digitalWrite(TRIG_PIN, LOW);
  delay(50);  // let sensor settle

  // Create and run the sensorTask on Core 1 with priority 1:
  xTaskCreatePinnedToCore(
    sensorTask,      // Task function
    "SensorTask",    // Name (for debugging)
    2048,            // Stack size in bytes
    nullptr,         // Parameter (none)
    1,               // Priority (lowest to highest: 0..configMAX_PRIORITIES-1)
    nullptr,         // Task handle (not used here)
    1                // Run on core 1 (you can also pass 0)
  );
}

void loop() {
  // Nothing else in loop—sensorTask() handles everything.
}

//--------------------------------------
// sensorTask: triggers HC‐SR04, stores into a length-5 circular buffer,
// and prints out the entire buffer each time
//--------------------------------------
void sensorTask(void* pvParameters) {
  (void) pvParameters;  // unused

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
    if (duration > 0) {
      // 3) Convert to distance in cm: (pulse_time µs) × (speed_cm/µs) ÷ 2
      distanceCm = (duration * SOUND_SPEED_CM_PER_US) / 2.0f;
    }

    // 4) Store in circular buffer:
    distanceBuffer[bufferIndex] = distanceCm;
    bufferIndex = (bufferIndex + 1) % 5;

    // 5) Print the most recent reading:
    Serial.print("Newest distance: ");
    Serial.print(distanceCm, 2);
    Serial.print(" cm   [Next write index = ");
    Serial.print(bufferIndex);
    Serial.println("]");

    // 6) Print out the entire buffer contents (indices 0–4):
    Serial.print("Buffer contents: [");
    for (int i = 0; i < 5; i++) {
      Serial.print(distanceBuffer[i], 2);
      if (i < 4) {
        Serial.print(", ");
      }
    }
    Serial.println("]");

    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  // Never reaches here, but required by Task signature:
  vTaskDelete(nullptr);
}
