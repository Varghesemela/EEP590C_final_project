/**
 * @file EEP590C_final_project.ino
 * @brief FreeRTOS-based Light Monitoring and Anomaly Detection System using ESP32
 
 * @section overview Overview
 * This sketch sets up all system resources and tasks needed for a smart security and monitoring system 
 * implemented on ESP32. It includes I2C, RFID, servo control, LCD display, RTC setup, and FreeRTOS-based 
 * task management. Timers and semaphores are used to coordinate access to shared resources.
 *
 * @section author Author
 * Created by Sai Jayanth Kalisi, 2025  
 * Additionally modified by Sanjay Varghese, 2025  
 * Additionally modified by Ankit Telluri, 2025   
 *
 * @mainpage EE590 Final Project
 *
 * @details
 * This Project implements a Smart Security Door and Monitoring System designed to provide
 * the user with automated access control and intrusion monitoring. 
 *
 * The system utilizes the RC522 RFID module to authenticate users with RFID tags. 
 * Upon successful authentication, a servo motor is triggered to unlock the door mechanism via
 * a latch that raises to unlock the door and falls to lock it in place. A Real-Time Clock (RTC)
 * module logs each access event, allowing for time-stamped tracking of entries. This is done via
 * a `Serial Print`.
 *
 * To enhance security, a Passive Infrared (PIR) sensor monitors motion near the door. If motion 
 * is detected without a corresponding unlock event, it flags potential unauthorized access. An 
 * Ultrasonic sensor further strengthens the intrusion detection by monitoring the proximity of
 * the potential intruder.
 * 
 * The backlight of the LCD flickers on depending on the values measured by the PIR and Ultrasonic 
 * Sensor. This event is logged using the time provided by the RTC. A timer is also started. If the
 * values of the PIR or the Ultrasonic sensor correspond to a person close to the door by the time
 * the timer is up, the timer resets and the LED Flickers on again. If not, the LCD backlight switches off. 
 * An ESP32 microcontroller serves as the system's central controller, managing sensor data, actuator control,
 * and task scheduling using FreeRTOS queues. Instead of the second ESP32, we used the dual-core functionality
 * of the ESP32 to provide parallel control for the sensors and the actuators. 
 *
 * This project demonstrates real-time control, reliable sensor interfacing, and dual-core embedded programming
 * principles.
 *
 * @section author Author
 * Sai Jayanth Kalisi, 2025  
 * Sanjay Varghese, 2025  
 * Ankit Telluri, 2025   
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

//========= SETUP =========
/**
 * @brief Arduino setup function
 * 
 * @details Initializes peripherals (pins, serial, servo, LCD, RTC), creates RTOS queues and tasks,
 * sets up timers and interrupts, and initializes I2C and SPI interfaces.
 * 
 * @note Name: setup
 */
void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(LED, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);


  //========= I2C INIT =========
  Wire.begin(SDA_PIN, SCL_PIN);

  //========= LCD INIT =========
  lcd.begin(8, 9);
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Person: ");
  lcd.setCursor(0, 1);
  lcd.print("State: ");

  //========= PWM LED INIT =========
  ledcAttach(LED, 100, 12);

  //========= SERVO INIT =========
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  myservo.setPeriodHertz(50);
  myservo.attach(SERVO_PIN, 500, 2400);
  myservo.write(0);
  delay(50);

  //========= RTC INIT =========
  rtc.begin();
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  // Set from compile time
  }

  //========= SEMAPHORE INIT =========
  i2c_semaphore = xSemaphoreCreateMutex();

  //========= TIMERS INIT =========
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

  //========= RFID INIT =========
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

  //========= QUEUE INIT =========
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

  //========= TASK CREATION =========

  // Name: RFID Reader Task
  xTaskCreatePinnedToCore(taskRFIDReader, "RFID Reader", 4096, NULL, 1, &taskRFIDReader_Handle, 1);

  // Name: RFID Printer Task
  xTaskCreatePinnedToCore(taskPrinter, "Printer", 2048, NULL, 1, &taskPrinter_Handle, 1);

  // Name: Servo Controller Task
  xTaskCreatePinnedToCore(ServoRunTask, "servoRun", 2048, NULL, 1, &TaskServoRun_Handle, 0);

  //Button (for debugging servo)

  // Name: LCD Display Task
  xTaskCreatePinnedToCore(LCDTask, "LCDTask", 2048, NULL, 1, &TaskLCD_Handle, 0);

  // Name: Sensor Read Task
  xTaskCreatePinnedToCore(sensorReadTask, "SensorReadTask", 8192, NULL, 1, &taskSensorRead_Handle, 1);

  // Name: Sensor Process Task
  xTaskCreatePinnedToCore(sensorProcessTask, "SensorProcessTask", 8192, NULL, 1, &taskSensorProcess_Handle, 1);

  //========= DEBUG TASKS =========
  // xTaskCreatePinnedToCore(motionTask, "MotionTask", 2048, NULL, 1, &TaskMotion_Handle, 0);
  // xTaskCreatePinnedToCore(updateButtonTask, "updateButton", 1024, NULL, 1, &TaskUpdateButton_Handle, 0);
  // xTaskCreatePinnedToCore(distanceTask, "UltraSonicTask", 2048, nullptr, 1, &TaskUltraSonic_Handle, 1);

  //========= INTERRUPTS =========
  attachInterrupt(ECHO_PIN, echoISR, CHANGE);
}


//========= LOOP =========
/**
 * @brief Main loop (unused)
 * 
 * @details Main loop is intentionally left empty. All execution is performed using FreeRTOS tasks.
 *
 * @note Name: Loop
 */
void loop() {
}