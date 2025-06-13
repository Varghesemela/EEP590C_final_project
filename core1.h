/**
 * @file core1.h
 * @brief Task declarations for Core 1 of the Smart Security Door System on ESP32.
 *
 * @details
 * This header provides declarations for FreeRTOS tasks that execute on Core 1.
 * These tasks include sensor polling, RFID scanning, and access control logic.
 * It complements `core1.cpp` and is intended to be used alongside shared definitions in `global_defs.h`.
 * 
 * Task Overview:
 * - `sensorReadTask` reads PIR and ultrasonic sensor values periodically.
 * - `sensorProcessTask` aggregates data from sensors and determines if backlight or unlock actions should be triggered.
 * - `taskRFIDReader` continuously checks for new RFID cards and sends UIDs.
 * - `taskPrinter` validates RFID UIDs, manages lock state, and provides user feedback.
 * - `distanceTask` and `rtcTask` are deprecated debug routines.
 *
 * @section Authors
 * Created by Sanjay Varghese, 2025  
 * Additionally modified by Sai Jayanth Kalisi, 2025  
 * Additionally modified by Ankit Telluri, 2025
 */

#ifndef CORE1_H
#define CORE1_H

#include <Arduino.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include "driver/timer.h"

// ================== TASK FUNCTION PROTOTYPES ================== //

void distanceTask(void* pvParameters); ///< Deprecated
void taskRFIDReader(void *pvParameters);
void taskPrinter(void *pvParameters);
void sensorReadTask(void *pvParameters);
void sensorProcessTask(void *pvParameters);

extern void IRAM_ATTR echoISR();

/**
 * @brief (Deprecated) Task to handle RTC functionality (current version incorporates this without the help of this task).
 * @param args Unused
 */
void rtcTask(void *args); ///< Deprecated

#endif