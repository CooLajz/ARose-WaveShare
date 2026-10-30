#pragma once
#include "Checkpoint.h"
#include <nvs.h>

class CheckpointStore {
 public:
  bool begin();
  bool load(arose::Engine& engine, uint8_t& brightness, bool& savingEnabled, uint8_t& luminous, uint8_t& glow);
  // 1 = committed, 0 = unchanged, -1 = error. No erase-on-error fallback.
  int saveRecord(const arose::Checkpoint& record);
  esp_err_t error() const { return lastError; }
 private:
  nvs_handle_t handle = 0;
  esp_err_t lastError = ESP_OK;
  arose::Checkpoint previous{};
  bool havePrevious = false;
};
