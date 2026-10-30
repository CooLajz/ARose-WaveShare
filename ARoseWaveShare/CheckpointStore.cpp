#include "CheckpointStore.h"
#include <nvs_flash.h>

bool CheckpointStore::begin() {
  // Reuse the existing 64 KiB NVS partition, preserving all clock namespaces.
  // NVS appends replacement records and recycles pages across this partition.
  lastError = nvs_flash_init_partition("clockcfg");
  if (lastError != ESP_OK) return false;
  lastError = nvs_open_from_partition("clockcfg", "arose", NVS_READWRITE, &handle);
  return lastError == ESP_OK;
}

bool CheckpointStore::load(arose::Engine& engine, uint8_t& brightness, bool& savingEnabled, uint8_t& luminous, uint8_t& glow) {
  if (!handle) return false;
  arose::Checkpoint record{};
  size_t size = sizeof(record);
  lastError = nvs_get_blob(handle, "checkpoint", &record, &size);
  if (lastError != ESP_OK || size != sizeof(record) || !record.valid() ||
      (record.hasPosition() && !engine.restore(record.state))) return false;
  brightness = record.brightness();
  luminous = record.luminous();
  glow = record.glow();
  savingEnabled = record.savingEnabled();
  previous = record; havePrevious = true;
  return true;
}

int CheckpointStore::saveRecord(const arose::Checkpoint& record) {
  if (!handle || !record.valid()) { lastError = ESP_ERR_INVALID_ARG; return -1; }
  if (havePrevious && std::memcmp(&record, &previous, sizeof(record)) == 0) return 0;
  lastError = nvs_set_blob(handle, "checkpoint", &record, sizeof(record));
  if (lastError == ESP_OK) lastError = nvs_commit(handle);
  if (lastError != ESP_OK) return -1;
  previous = record; havePrevious = true;
  return 1;
}
