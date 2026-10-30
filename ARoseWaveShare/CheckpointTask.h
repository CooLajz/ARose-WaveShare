#pragma once
#ifdef AROSE_REQUIRE_XIP
#include "CheckpointStore.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

// Main owns submission/polling; the worker exclusively owns NVS after begin().
// Queues copy complete snapshots, so the worker never reads the live engine.
class CheckpointTask {
 public:
  struct Result {
    int result;
    uint32_t frame, brightness, elapsedUs, erases, eraseUs, settings;
    esp_err_t error;
  };
  bool begin(CheckpointStore& store);
  bool submit(const arose::Checkpoint& record);
  bool poll(Result& result);
 private:
  static void run(void* context);
  CheckpointStore* store_ = nullptr;
  QueueHandle_t requests_ = nullptr, results_ = nullptr;
  StaticQueue_t requestQueue_{}, resultQueue_{};
  alignas(4) uint8_t requestBytes_[sizeof(arose::Checkpoint)]{};
  alignas(4) uint8_t resultBytes_[sizeof(Result)]{};
  bool busy_ = false;
};
#endif
