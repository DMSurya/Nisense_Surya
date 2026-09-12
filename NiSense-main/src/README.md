# Application sources (`src/`)

Thematic layout (Zephyr app). Public APIs live in [`../include/`](../include/);
`#include "foo.h"` still works because CMake adds each subdirectory to the
include path.

| Folder | Role |
|--------|------|
| [`main.c`](main.c) | Boot / main loop (stays at `src/` root) |
| [`core/`](core/) | RGB, LED, buzzer, RTC, identity, prefs, logo stubs |
| [`ble/`](ble/) | BLE host, GATT, model/XIP/record sync, BLE UI |
| [`ota/`](ota/) | OTA compat/progress, Resource store/fonts/icons, calibration |
| [`ui/`](ui/) | LVGL shell, theme, screens, buttons |
| [`power/`](power/) | PMIC / battery / power manager |
| [`sensors/`](sensors/) | PPG, glucose, temp, health scheduler, MAX32664 update |
| [`storage/`](storage/) | Record store, config manager, QSPI busy gate |
| [`net/`](net/) | Wi-Fi test, HTTP, MQTT, cloud telemetry |
| [`diag/`](diag/) | Optional CDC shell (`APP_FEATURE_SHELL`→USB) + diagnostics monitor |

See also: [docs/build/BUILD_FLASH_DEPLOY.md](../docs/build/BUILD_FLASH_DEPLOY.md),
[docs/README.md](../docs/README.md), [../README.md](../README.md).
