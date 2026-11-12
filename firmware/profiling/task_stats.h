#pragma once
// Bench-only profiling helper for firmware/gbbdongle-profile.yaml (not used by
// any shipped image). Samples FreeRTOS per-task run time and reports, for
// the last sampling window, how much of each core went to every task. The
// share left to the IDLE task of a core is that core's spare CPU.
//
// Needs CONFIG_FREERTOS_USE_TRACE_FACILITY, CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
// and CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID (set by the profile yaml).

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "esphome/core/log.h"

namespace gbb_profile {

static const char *const TAG = "profile";

struct TaskShare {
  const char *name;
  float percent;  // of one core's time in the window
  int core;       // tskNO_AFFINITY (0x7FFFFFFF) for unpinned tasks
  uint32_t stack_free;
};

inline float idle_percent[2] = {NAN, NAN};
inline float busiest_task_percent = NAN;
inline uint32_t task_count = 0;

inline std::map<TaskHandle_t, configRUN_TIME_COUNTER_TYPE> previous_counters;
inline configRUN_TIME_COUNTER_TYPE previous_total = 0;

/// Call periodically (e.g. every 30 s from an interval:). The first call only
/// primes the counters.
inline void sample(bool log_tasks) {
  const UBaseType_t n = uxTaskGetNumberOfTasks();
  std::vector<TaskStatus_t> tasks(n + 4);
  configRUN_TIME_COUNTER_TYPE total = 0;
  const UBaseType_t got = uxTaskGetSystemState(tasks.data(), tasks.size(), &total);
  tasks.resize(got);
  task_count = got;

  const configRUN_TIME_COUNTER_TYPE window = total - previous_total;
  const bool primed = previous_total != 0 && window > 0;
  previous_total = total;

  TaskHandle_t idle[2] = {xTaskGetIdleTaskHandleForCore(0), nullptr};
#if CONFIG_FREERTOS_NUMBER_OF_CORES > 1 || (defined(portNUM_PROCESSORS) && portNUM_PROCESSORS > 1)
  idle[1] = xTaskGetIdleTaskHandleForCore(1);
#endif

  std::vector<TaskShare> shares;
  shares.reserve(got);
  float busiest = 0.0f;
  for (const TaskStatus_t &t : tasks) {
    auto it = previous_counters.find(t.xHandle);
    const configRUN_TIME_COUNTER_TYPE before = it != previous_counters.end() ? it->second : t.ulRunTimeCounter;
    previous_counters[t.xHandle] = t.ulRunTimeCounter;
    if (!primed)
      continue;
    const float pct = 100.0f * (float) (t.ulRunTimeCounter - before) / (float) window;
    for (int core = 0; core < 2; core++) {
      if (idle[core] != nullptr && t.xHandle == idle[core])
        idle_percent[core] = pct;
    }
    const bool is_idle = t.xHandle == idle[0] || t.xHandle == idle[1];
    if (!is_idle)
      busiest = std::max(busiest, pct);
    shares.push_back({t.pcTaskName, pct, (int) t.xCoreID, (uint32_t) t.usStackHighWaterMark * sizeof(StackType_t)});
  }
  if (!primed)
    return;
  busiest_task_percent = busiest;

  // Tasks that exited since the last sample keep a stale map entry; harmless
  // at the handful of tasks an ESPHome image runs.
  ESP_LOGI(TAG, "CPU over the last %.1f s: core0 idle %.1f%%, core1 idle %.1f%%, %u tasks; internal heap free %u B "
           "(min ever %u B, largest block %u B), PSRAM free %u B",
           window / 1e6f, idle_percent[0], idle_percent[1], (unsigned) got,
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned) heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
           (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  if (!log_tasks)
    return;
  std::sort(shares.begin(), shares.end(), [](const TaskShare &a, const TaskShare &b) { return a.percent > b.percent; });
  for (const TaskShare &s : shares) {
    if (s.core == 0 || s.core == 1) {
      ESP_LOGI(TAG, "  %-16s %6.2f%%  core %d  stack free %5u B", s.name, s.percent, s.core, (unsigned) s.stack_free);
    } else {
      ESP_LOGI(TAG, "  %-16s %6.2f%%  any     stack free %5u B", s.name, s.percent, (unsigned) s.stack_free);
    }
  }
}

}  // namespace gbb_profile
