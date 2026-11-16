// Stand-in for FreeRTOS tasks. Tests define the functions, e.g. xTaskCreate()
// running the task on a std::thread.
#pragma once
#include "FreeRTOS.h"

typedef void (*TaskFunction_t)(void *);
BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, uint32_t stack, void *param, UBaseType_t prio,
                       TaskHandle_t *handle);
void vTaskDelete(TaskHandle_t handle);
