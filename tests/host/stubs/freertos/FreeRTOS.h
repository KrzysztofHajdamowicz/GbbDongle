// Stand-in for FreeRTOS: the types the units under test use.
#pragma once
#include <cstdint>

typedef unsigned int UBaseType_t;
typedef int BaseType_t;
typedef void *TaskHandle_t;
#define pdPASS 1
