#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <esp_psram.h>
#include "Display_ST7701.h"
#include "FirmwareBuild.h"
#include "RoseEngine.h"
#include "RoseRenderer.h"
#include "CheckpointStore.h"
#include "CheckpointTask.h"
#include "Controls.h"
#include "MotionClock.h"
#include "SettingsScreen.h"

#ifndef AROSE_START_FRAME
#define AROSE_START_FRAME 0
#endif

namespace {
arose::Engine engine;
arose::Renderer renderer;
arose::Controls controls;
arose::MotionClock motion;
CheckpointStore storage;
#ifdef AROSE_REQUIRE_XIP
CheckpointTask checkpointTask;
#endif
bool storageReady = false, menuDirty = false, settingsInFlight = false;
int frozenIndex = 0;
float lastAlpha = 0;
int64_t checkpointAt = 0;
uint32_t checkpointCount = 0;
constexpr int64_t CheckpointIntervalUs = 120000000;
void applyControl(unsigned action);
uint16_t* buffers[3]{};
uint16_t* displayed = nullptr;
int drawIndex = 1;
int64_t motionWallAt = 0, statsAt = 0;
double simulationRemainderUs = 0;
uint64_t renderTotal = 0, presentTotal = 0;
uint32_t frameCount = 0, renderMax = 0, failures = 0, droppedSteps = 0;
int64_t previousPresentedAt = 0;
uint32_t maxGapUs = 0, lateFrames = 0;
constexpr int64_t StepUs = 20000;

void initializeDisplay(void* caller) {
  // Keep LCD interrupts on core 0; the renderer runs on core 1.
  LCD_Init();
  xTaskNotifyGive(static_cast<TaskHandle_t>(caller));
  vTaskDelete(nullptr);
}

void status(Print& out) {
  const int64_t duration = esp_timer_get_time() - statsAt;
  const double fps = duration > 0 ? double(frameCount) * 1000000 / duration : 0;
  out.printf("PROFILE prepare=%lu halo=%lu core=%lu convert=%lu cpu0=%lu cpu1=%lu us\n", (unsigned long)renderer.prepareUs, (unsigned long)renderer.haloUs, (unsigned long)renderer.coreUs, (unsigned long)renderer.convertUs, (unsigned long)renderer.workerUs[1], (unsigned long)renderer.workerUs[0]);
  out.printf("AROSE board=%s version=%s fps=%.2f render_ms=%.2f max_ms=%.2f present_ms=%.2f frames=%lu sim=%lu errors=%lu dropped=%lu heap=%u psram=%u brightness=%u\n",
    Touch_GetBoardName(), AROSE_VERSION, fps,
    frameCount ? double(renderTotal) / frameCount / 1000 : 0,
    double(renderMax) / 1000,
    frameCount ? double(presentTotal) / frameCount / 1000 : 0,
    (unsigned long)frameCount, (unsigned long)engine.frame(),
    (unsigned long)failures, (unsigned long)droppedSteps,
    unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
    unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)), unsigned(LCD_GetBacklight()));
  out.printf("UI paused=%u menu=%u brightness=%u saves=%lu storage=%u speed=%.3f ramp=%u save_position=%u draft_save=%u reset_pending=%u saving=%u save_error=%u luminous=%u glow=%u\n", unsigned(controls.paused), unsigned(controls.menu), unsigned(controls.brightness), (unsigned long)checkpointCount, unsigned(storageReady), motion.speed(), unsigned(motion.transitioning()), unsigned(controls.savingEnabled), unsigned(controls.draftSaving), unsigned(controls.resetPending), unsigned(controls.committing), unsigned(controls.saveError),unsigned(controls.luminous),unsigned(controls.glow));
  out.printf("PACING pclk=%lu gap_ms=%.2f late=%lu\n", (unsigned long)LCD_GetPixelClock(), double(maxGapUs)/1000, (unsigned long)lateFrames);
}

void resetStats() {
  statsAt = esp_timer_get_time();
  frameCount = 0; renderTotal = 0; presentTotal = 0; renderMax = 0;
  maxGapUs = 0; lateFrames = 0;
}

void diagnostics(Stream& port) {
  while (port.available()) {
    const int command = port.read();
    if (command == '?') status(port);
    if (command == 'p') applyControl(controls.togglePause());
    if (command == 'm') applyControl(controls.toggleMenu());
    if (command == 'e') applyControl(controls.toggleSaving());
    if (command == 'r') applyControl(controls.toggleReset());
    if (command == '}') applyControl(controls.setGlow(controls.glow+10));
    if (command == '{') applyControl(controls.setGlow(controls.glow-10));
    if (command == ']') applyControl(controls.setLuminous(controls.luminous+10));
    if (command == '[') applyControl(controls.setLuminous(controls.luminous-10));
    if (command == '+') applyControl(controls.setBrightness(controls.brightness+5));
    if (command == '-') applyControl(controls.setBrightness(controls.brightness-5));
#ifdef AROSE_REQUIRE_XIP
    // Explicit UART stress-test request; normal cadence remains 15 seconds.
    if (command == 's') checkpointAt=esp_timer_get_time()-CheckpointIntervalUs;
#endif
    if (command == 'c' && displayed) {
      // Main-loop capture: the displayed buffer remains immutable throughout.
      port.printf("AROSE_FRAME 480 480 460800\n");
      const uint8_t* bytes = reinterpret_cast<const uint8_t*>(displayed);
      for (size_t offset = 0; offset < 480 * 480 * 2; offset += 1024) {
        const size_t count = std::min(size_t(1024), size_t(480 * 480 * 2) - offset);
        size_t sent = 0;
        const uint32_t start = millis();
        while (sent < count) {
          const size_t written = port.write(bytes + offset + sent, count - sent);
          sent += written;
          if (millis() - start > 2000) {
            motionWallAt = esp_timer_get_time(); previousPresentedAt = 0; resetStats(); return;
          }
          if (!written) delay(1);
        }
        delay(1);
      }
      port.print("\nAROSE_END\n");
      port.flush();
      motionWallAt = esp_timer_get_time();
      previousPresentedAt = 0;
      resetStats();
    }
  }
}

int bufferIndex() {
  for (int i=0; i<3; ++i) if (buffers[i]==displayed) return i;
  return 0;
}

void resetAnimationClock() {
  motionWallAt=esp_timer_get_time();
  previousPresentedAt=0;
  resetStats();
}

void showControlBuffer(int index) {
  // Menu uses two buffers while the third remains the immutable light canvas.
  // Give the previous UI buffer a second transfer boundary before reusing it.
  if (!LCD_addWindow(0,0,479,479,reinterpret_cast<uint8_t*>(buffers[index])) || !LCD_WaitFrames(1,100)) {
    ++failures; Serial0.println("Chyba synchronizace nastaveni.");
    while (true) delay(1000);
  }
  displayed=buffers[index];
  previousPresentedAt=0;
}

void applyControl(unsigned action) {
  if (action & arose::Controls::Open) {
    frozenIndex=bufferIndex(); menuDirty=true; resetAnimationClock();
  }
  if (action & arose::Controls::Save) menuDirty=true;
  if (action & arose::Controls::Options) menuDirty=true;
  if (action & arose::Controls::Pause) motion.setPaused(controls.paused);
  if (action & arose::Controls::Glow) {
    renderer.setGlow(controls.glow);
    if (controls.menu) menuDirty=true;
  }
  if (action & arose::Controls::Luminous) {
    renderer.setLuminous(controls.luminous);
    if (controls.menu) menuDirty=true;
  }
  if (action & arose::Controls::Brightness) {
    Set_Backlight(controls.brightness);
    if (controls.menu) menuDirty=true;
  }
}

void finishSettings(bool success) {
  const bool restart=controls.resetPending;
  controls.finishSave(success);
  settingsInFlight=false;
  menuDirty=true;
  if (!success) return;
  checkpointAt=esp_timer_get_time();
  if (restart) {
    engine.reset(); engine.step(); renderer.resetCanvas();
    controls.paused=false; motion.reset(0); motion.setPaused(false);
    simulationRemainderUs=0; lastAlpha=0;
    drawIndex=(bufferIndex()+1)%3;
  } else {
    showControlBuffer(frozenIndex);
    drawIndex=(frozenIndex+1)%3;
    renderer.invalidateOutputs();
  }
  menuDirty=false; resetAnimationClock();
  Serial0.printf("SETTINGS saved=1 save_position=%u reset=%u frame=%lu\n",
    unsigned(controls.savingEnabled),unsigned(restart),(unsigned long)engine.frame());
}

void reportCheckpoint(int result, uint32_t frame, unsigned brightness,
                      int64_t elapsed, esp_err_t error, uint32_t erases, uint32_t eraseUs, uint32_t settings) {
  if (result>0) ++checkpointCount;
  Serial0.printf("CHECKPOINT result=%s frame=%lu bytes=%u elapsed_ms=%.2f count=%lu brightness=%u error=%s erases=%lu erase_ms=%.2f save_position=%u has_position=%u\n",
    result>0 ? "saved" : result==0 ? "unchanged" : "failed", (unsigned long)frame,
    unsigned(sizeof(arose::Checkpoint)),double(elapsed)/1000,(unsigned long)checkpointCount,
    brightness,result<0 ? esp_err_to_name(error) : "OK", (unsigned long)erases,double(eraseUs)/1000,
    unsigned(bool(settings & arose::Checkpoint::SaveEnabled)),unsigned(bool(settings & arose::Checkpoint::PositionPresent)));
}

void checkpointIfDue() {
  if (!storageReady) { if (controls.committing) finishSettings(false); return; }
#ifdef AROSE_REQUIRE_XIP
  CheckpointTask::Result completed{};
  if (checkpointTask.poll(completed)) {
    reportCheckpoint(completed.result,completed.frame,completed.brightness,completed.elapsedUs,completed.error,completed.erases,completed.eraseUs,completed.settings);
    if (settingsInFlight) { finishSettings(completed.result>=0); return; }
  }
#endif
  const int64_t now=esp_timer_get_time();
  const bool settingsWrite=controls.menu && controls.committing;
  if (settingsInFlight) return;
  if (!settingsWrite && (controls.menu || !controls.savingEnabled || now-checkpointAt<CheckpointIntervalUs)) return;
  const bool enabled=settingsWrite ? controls.draftSaving : controls.savingEnabled;
  const bool clear= !enabled || (settingsWrite && controls.resetPending);
  const auto state=engine.state();
  const auto record=arose::Checkpoint::make(clear ? nullptr : &state,controls.brightness,enabled,controls.luminous,controls.glow);
#ifdef AROSE_REQUIRE_XIP
  // Serialize settings behind any previous checkpoint; close/reset only after ACK.
  if (checkpointTask.submit(record)) { checkpointAt=now; settingsInFlight=settingsWrite; }
#else
  checkpointAt=now;
  const int64_t started=esp_timer_get_time();
  const int result=storage.saveRecord(record);
  const int64_t elapsed=esp_timer_get_time()-started;
  motionWallAt+=elapsed;
  reportCheckpoint(result,record.state.frame,record.brightness(),elapsed,storage.error(),0,0,record.settings);
  if (settingsWrite) finishSettings(result>=0);
#endif
}

void periodicStatus() {
  if (esp_timer_get_time()-statsAt>=5000000) {
    status(Serial0); if (Serial) status(Serial); resetStats();
  }
}
}

void setup() {
  Serial0.setTxBufferSize(4096);
  Serial0.begin(921600);
  Serial.begin(921600);
  delay(300);
  Serial0.printf("ARose WaveShare %s; CPU=%u MHz flash=%u PSRAM=%u\n",
    AROSE_VERSION, unsigned(ESP.getCpuFreqMHz()), unsigned(ESP.getFlashChipSize()), unsigned(ESP.getPsramSize()));
  // With XIP, part of the physical PSRAM contains code/rodata, not heap.
  if (ESP.getFlashChipSize() != 16 * 1024 * 1024 || esp_psram_get_size() < 8 * 1024 * 1024) {
    Serial0.println("Chybna pametova konfigurace. Overte 16MB flash a 8MB OPI PSRAM.");
    while (true) delay(1000);
  }
#ifdef AROSE_REQUIRE_XIP
  Serial0.printf("SDK idf=%s xip_instructions=%u xip_rodata=%u cache_line=%u psram_physical=%u resync=off\n",
    esp_get_idf_version(), unsigned(CONFIG_SPIRAM_FETCH_INSTRUCTIONS),
    unsigned(CONFIG_SPIRAM_RODATA), unsigned(CONFIG_ESP32S3_DATA_CACHE_LINE_SIZE),
    unsigned(esp_psram_get_size()));
#endif
  engine.begin();
  storageReady=storage.begin();
  const bool restored=storageReady && AROSE_START_FRAME==0 && storage.load(engine,controls.brightness,controls.savingEnabled,controls.luminous,controls.glow);
  Serial0.printf("RESTORE loaded=%u frame=%lu brightness=%u storage=%u save_position=%u\n", unsigned(restored), (unsigned long)engine.frame(), unsigned(controls.brightness), unsigned(storageReady),unsigned(controls.savingEnabled));
  if (!storageReady) Serial0.printf("NVS initialization failed: %s\n",esp_err_to_name(storage.error()));
  I2C_Init();
  Set_EXIOS(0x0c);
  TCA9554PWR_Init(0x70);
  Serial0.printf("LCD_MEMORY internal=%u largest_dma=%u\n",
    unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
    unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA)));
  const BaseType_t created = xTaskCreatePinnedToCore(initializeDisplay, "lcd-init", 6144,
    xTaskGetCurrentTaskHandle(), 3, nullptr, 0);
  if (created != pdPASS || !ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10000))) {
    Serial0.println("Inicializace displeje selhala.");
    while (true) delay(1000);
  }
  Set_Backlight(controls.brightness);
  void* fb0 = nullptr; void* fb1 = nullptr; void* fb2 = nullptr;
  ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel_handle, 3, &fb0, &fb1, &fb2));
  buffers[0] = static_cast<uint16_t*>(fb0);
  buffers[1] = static_cast<uint16_t*>(fb1);
  buffers[2] = static_cast<uint16_t*>(fb2);
  for (auto* buffer : buffers) memset(buffer, 0, 480 * 480 * 2);
  // Compile-time stress test; normal firmware resumes its saved state.
#if AROSE_START_FRAME > 0
  for (uint32_t i = 0; i < AROSE_START_FRAME; ++i) {
    engine.step();
    if ((i & 255) == 0) delay(1);
  }
#endif
  if (!renderer.begin(LCD_GetFramePeriodMs())) {
    Serial0.println("Inicializace rendereru selhala.");
    while (true) delay(1000);
  }
  renderer.setGlow(controls.glow);
  renderer.setLuminous(controls.luminous);
  if (!Touch_Init()) Serial0.println("Inicializace dotyku selhala.");
#ifdef AROSE_REQUIRE_XIP
  if (storageReady && !checkpointTask.begin(storage)) {
    storageReady=false;
    Serial0.println("Inicializace ukladani na pozadi selhala.");
  }
#endif
  engine.step();
  checkpointAt=esp_timer_get_time();
  motionWallAt = esp_timer_get_time();
  resetStats();
  Serial0.printf("Spusteno: %s; moderni glow; jas %u %%\n", Touch_GetBoardName(),unsigned(controls.brightness));
}

void loop() {
  diagnostics(Serial0);
  if (Serial) diagnostics(Serial);
  const bool valid=Touch_Read_Data();
  applyControl(controls.sample(valid,touch_data.points!=0,touch_data.x,touch_data.y,millis()));
  if (controls.menu) {
    if (controls.menu && menuDirty) {
      int target=(bufferIndex()+1)%3;
      if (target==frozenIndex) target=(target+1)%3;
      arose::drawSettings(buffers[target],controls.brightness,controls.draftSaving,controls.resetPending,controls.committing,controls.saveError,controls.luminous,controls.glow);
      showControlBuffer(target); menuDirty=false;
    }
    motionWallAt=esp_timer_get_time();
    checkpointIfDue(); periodicStatus(); delay(5); return;
  }
  const int64_t now = esp_timer_get_time();
  const bool slowMotion=motion.transitioning() || motion.speed()<1;
  const double elapsed=motion.advance(now-motionWallAt);
  motionWallAt=now;
  if (motion.stopped() && elapsed==0 && displayed) {
    previousPresentedAt=0; // An intentional pause is not a missed LCD deadline.
    checkpointIfDue(); periodicStatus(); delay(5); return;
  }
  simulationRemainderUs+=elapsed;
  int steps=0;
  while (simulationRemainderUs>=StepUs && steps<10) {
    engine.step(); simulationRemainderUs-=StepUs; ++steps;
  }
  if (simulationRemainderUs>=StepUs) {
    droppedSteps+=uint32_t(simulationRemainderUs/StepUs);
    simulationRemainderUs=std::fmod(simulationRemainderUs,double(StepUs));
  }
  const float alpha=float(simulationRemainderUs/StepUs);
  const int64_t start = esp_timer_get_time();
  renderer.render(buffers[drawIndex], engine, alpha, slowMotion);
  lastAlpha=alpha;
  const uint32_t renderUs = uint32_t(esp_timer_get_time() - start);
  renderTotal += renderUs;
  renderMax = std::max(renderMax, renderUs);
  const int64_t presentStart = esp_timer_get_time();
  if (!LCD_addWindow(0, 0, 479, 479, reinterpret_cast<uint8_t*>(buffers[drawIndex]))) {
    ++failures;
    Serial0.println("Chyba synchronizace LCD. Vykreslovani bylo zastaveno.");
    // Do not reuse a framebuffer whose ownership is now uncertain.
    while (true) { diagnostics(Serial0); if (Serial) diagnostics(Serial); delay(100); }
  }
  presentTotal += esp_timer_get_time() - presentStart;
  displayed = buffers[drawIndex];
  const int64_t presentedAt = esp_timer_get_time();
  if (previousPresentedAt) {
    const uint32_t gap = uint32_t(presentedAt - previousPresentedAt);
    maxGapUs = std::max(maxGapUs, gap);
    if (gap > uint32_t(LCD_GetFramePeriodMs() * 1500)) ++lateFrames;
  }
  previousPresentedAt = presentedAt;
  drawIndex = (drawIndex + 1) % 3;
  ++frameCount;
  checkpointIfDue();
  periodicStatus();
  // LCD completion already blocks each iteration and lets the idle task run.
}
