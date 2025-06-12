#ifndef CORE1_H
#define CORE1_H

#include <Arduino.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include "driver/timer.h"

// Task function prototypes

void distanceTask(void* pvParameters);
void rtcTask(void *args);

void taskRFIDReader(void *pvParameters);
void taskPrinter(void *pvParameters);
void sensorReadTask(void *pvParameters);
void sensorProcessTask(void *pvParameters);

#endif