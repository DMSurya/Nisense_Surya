# Out-of-tree drivers

Zephyr modules under `drivers/` (wired via `ZEPHYR_EXTRA_MODULES` in
`CMakeLists.txt`). Docs and APIs:

| Area | Path | Notes |
|------|------|-------|
| MAX20360 PMIC | `drivers/regulator`, `charger`, `fuel_gauge`, … | [docs/drivers/MAX20360.md](../docs/drivers/MAX20360.md) |
| MAX32664 hub | `drivers/sensor/max32664/` | [docs/sensors/MAX32664_SUMMARY.md](../docs/sensors/MAX32664_SUMMARY.md) |
| MAX3010x | `drivers/sensor/max3010x/` | Pulse PPG path |
| Glucose (Svasth) | `drivers/sensor/` (glucose) | [docs/sensors/GLUCOSE_DATA_LOGGING.md](../docs/sensors/GLUCOSE_DATA_LOGGING.md) |
| MAX302xx temp | `drivers/sensor/max302xx/` | Wrist / finger |
| Wi‑Fi WExx | `drivers/wifi/wexx/` | [docs/sensors/WIFI_INTEGRATION.md](../docs/sensors/WIFI_INTEGRATION.md) |
| Counter / RTC | `drivers/counter/` | [counter/README.md](counter/README.md) |

Bindings: `dts/bindings/`. App code: [`src/`](../src/README.md).  
Build / features: [docs/build/BUILD_FLASH_DEPLOY.md](../docs/build/BUILD_FLASH_DEPLOY.md).
