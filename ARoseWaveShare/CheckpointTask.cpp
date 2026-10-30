#include "CheckpointTask.h"
#ifdef AROSE_REQUIRE_XIP
#include <esp_timer.h>
#include <esp_flash.h>
#include <atomic>

namespace {
std::atomic<uint32_t> flashErases{0}, flashEraseUs{0};
}
extern "C" esp_err_t __real_esp_flash_erase_region(esp_flash_t*, uint32_t, uint32_t);
extern "C" esp_err_t __wrap_esp_flash_erase_region(esp_flash_t* chip, uint32_t start, uint32_t length) {
  const int64_t began = esp_timer_get_time();
  const esp_err_t result = __real_esp_flash_erase_region(chip, start, length);
  flashEraseUs.fetch_add(uint32_t(esp_timer_get_time()-began), std::memory_order_relaxed);
  flashErases.fetch_add(1, std::memory_order_relaxed);
  return result;
}

bool CheckpointTask::begin(CheckpointStore& store) {
  store_ = &store;
  requests_ = xQueueCreateStatic(1, sizeof(arose::Checkpoint), requestBytes_, &requestQueue_);
  results_ = xQueueCreateStatic(1, sizeof(Result), resultBytes_, &resultQueue_);
  // Renderer and touch have higher priorities. Allocate after the LCD buffers.
  return requests_ && results_ &&
    xTaskCreatePinnedToCore(run, "checkpoint", 4096, this, 0, nullptr, 0) == pdPASS;
}

bool CheckpointTask::submit(const arose::Checkpoint& record) {
  if (busy_ || !record.valid()) return false;
  if (xQueueSend(requests_, &record, 0) != pdTRUE) return false;
  busy_ = true;
  return true;
}

bool CheckpointTask::poll(Result& result) {
  if (!busy_ || xQueueReceive(results_, &result, 0) != pdTRUE) return false;
  busy_ = false;
  return true;
}

void CheckpointTask::run(void* context) {
  auto& self = *static_cast<CheckpointTask*>(context);
  for (;;) {
    arose::Checkpoint record{};
    xQueueReceive(self.requests_, &record, portMAX_DELAY);
    const uint32_t erases = flashErases.load(std::memory_order_relaxed);
    const uint32_t eraseUs = flashEraseUs.load(std::memory_order_relaxed);
    const int64_t started = esp_timer_get_time();
    const int saved = self.store_->saveRecord(record);
    Result result{saved, record.state.frame, record.brightness(),
      uint32_t(esp_timer_get_time() - started),
      flashErases.load(std::memory_order_relaxed)-erases,
      flashEraseUs.load(std::memory_order_relaxed)-eraseUs, record.settings, self.store_->error()};
    xQueueSend(self.results_, &result, portMAX_DELAY);
  }
}
#endif
