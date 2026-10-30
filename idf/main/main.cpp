#include "sdkconfig.h"
#if !CONFIG_SPIRAM_FETCH_INSTRUCTIONS || !CONFIG_SPIRAM_RODATA
#error "This build requires PSRAM XIP for both instructions and read-only data."
#endif
#ifdef CONFIG_LCD_RGB_RESTART_IN_VSYNC
#error "Automatic LCD resynchronization must remain disabled."
#endif
#include "ARoseWaveShare.ino"
