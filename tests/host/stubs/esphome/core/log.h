// Stand-in for esphome/core/log.h: log lines go to stdout.
#pragma once
#include <cinttypes>
#include <cstdio>

#define ESP_LOG_(lvl, tag, fmt, ...) std::printf("[" lvl "][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) ESP_LOG_("E", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) ESP_LOG_("W", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) ESP_LOG_("I", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) ESP_LOG_("D", tag, fmt, ##__VA_ARGS__)
