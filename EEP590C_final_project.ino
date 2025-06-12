/**
 * @file EE590 Final project Servo Test.ino
 * @brief FreeRTOS-based Light Monitoring and Anomaly Detection System using ESP32
 *
 * @mainpage EE590 Final Project
 *
 * @section overview Overview
 * This sketch reads light intensity using a photoresistor, smooths the readings using a
 * sliding window average, detects anomalies, displays real-time values on an I2C LCD,
 * and concurrently calculates prime numbers using FreeRTOS tasks pinned to ESP32 cores.
 *
 * @section author Author
 * Created by Sai Jayanth Kalisi, 2025
 *
 */

//========= LIBRARIES =========
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <Wire.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include "global_defs.h"
#include "core0.h"
#include "core1.h"

//========= GLOBAL VARIABLES =========
// static SemaphoreHandle_t semaphore;  ///< Semaphore for data synchronization

int currentAngle = 0;
//========= SETUP =========
/**
 * @brief Arduino setup function
 * @details 1. Initialize pins, serial, LCD, etc
 */
void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(LED, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // init I2C port
  Wire.begin(SDA_PIN, SCL_PIN);

  // init LCD
  lcd.begin(8, 9);
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Person: ");
  lcd.setCursor(0, 1);
  lcd.print("State: ");

  //attach led as an ledc output
  ledcAttach(LED, 100, 12);

  // init servo
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);

  myservo.setPeriodHertz(50);
  myservo.attach(SERVO_PIN, 500, 2400);
  myservo.write(0);
  delay(50);

  //init rtc
  rtc.begin();
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  // Set from compile time
  }

  //semaphores
  i2c_semaphore = xSemaphoreCreateMutex();

  esp_timer_create_args_t timer_args = {
    .callback = &onLockTimer,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "lockTimer"
  };
  esp_timer_create(&timer_args, &lockTimer);

  esp_timer_create_args_t backlight_timer_args = {
    .callback = &onBacklightTimer,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "backlightTimer"
  };
  esp_timer_create(&backlight_timer_args, &backlightTimer);

  timer_config_t cfg = {
    .alarm_en = TIMER_ALARM_DIS,          // no alarm
    .counter_en = TIMER_PAUSE,            // start paused
    .intr_type = TIMER_INTR_LEVEL,        // no IRQ
    .counter_dir = TIMER_COUNT_UP,        // count up
    .auto_reload = TIMER_AUTORELOAD_DIS,  // no reload
    .clk_src = TIMER_SRC_CLK_APB,         // use APB (80 MHz)
    .divider = 80                         // → 1 MHz tick → 1 µs resolution
  };
  timer_init(TIMER_GROUP_0, TIMER_0, &cfg);
  timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
  timer_start(TIMER_GROUP_0, TIMER_0);
  // Init RFID module
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  rfid.PCD_Init(SS_PIN, RST_PIN);
  if (xSemaphoreTake(i2c_semaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
    DateTime now = rtc.now();
    Serial.printf("Time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
    xSemaphoreGive(i2c_semaphore);
  } else {
    Serial.println("RTC I2C timeout");
  }

  Serial.println(F("FreeRTOS RFID System Starting..."));

  // Create a queue with room for 5 strings of 20 chars each
  rfidQueue = xQueueCreate(5, sizeof(char[20]));
  if (rfidQueue == NULL) {
    Serial.println("Error creating queue!");
    while (1)
      ;
  }
  sensorQueue = xQueueCreate(10, sizeof(sensorData_t));
  if (!sensorQueue) {
    Serial.println("ERROR: failed to create sensorQueue");
  }

  //RFID
  xTaskCreatePinnedToCore(taskRFIDReader, "RFID Reader", 4096, NULL, 1, &taskRFIDReader_Handle, 1);
  xTaskCreatePinnedToCore(taskPrinter, "Printer", 2048, NULL, 1, &taskPrinter_Handle, 1);

  //servo
  xTaskCreatePinnedToCore(ServoRunTask, "servoRun", 2048, NULL, 1, &TaskServoRun_Handle, 0);

  //Button (for debugging servo)
  // xTaskCreatePinnedToCore(updateButtonTask, "updateButton", 1024, NULL, 1, &TaskUpdateButton_Handle, 0);

  //LCD
  xTaskCreatePinnedToCore(LCDTask, "LCDTask", 2048, NULL, 1, &TaskLCD_Handle, 0);
  // xTaskCreatePinnedToCore(motionTask, "MotionTask", 2048, NULL, 1, &TaskMotion_Handle, 0);

  // xTaskCreatePinnedToCore(distanceTask, "UltraSonicTask", 2048, nullptr, 1, &TaskUltraSonic_Handle, 1);
  xTaskCreatePinnedToCore(sensorReadTask, "SensorReadTask", 8192, NULL, 1, &taskSensorRead_Handle, 1);
  xTaskCreatePinnedToCore(sensorProcessTask, "SensorProcessTask", 8192, NULL, 1, &taskSensorProcess_Handle, 1);
  attachInterrupt(ECHO_PIN, echoISR, CHANGE);
}


//========= LOOP =========
/**
 * @brief Empty main loop since tasks handle all operations
 */
void loop() {
}