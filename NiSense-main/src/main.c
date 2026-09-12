#include <zephyr/kernel.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/logging/log.h>
#if defined(CONFIG_APP_FEATURE_DIAG_MONITOR)
#include <zephyr/debug/thread_analyzer.h>
#endif
#include <zephyr/sys/reboot.h>
#include <lvgl.h>
#include "power.h"
#include "rgb.h"
#include "led_status.h"
#include "power_mgr.h"

#include "ui.h"
#include "ui_buttons.h"
#include "ui_common.h"
#include "device_info_ui.h"
#if defined(CONFIG_UI_SCREEN_HOME_INFO)
#include "home_info_ui.h"
#endif
#include "home_last_results.h"
#include "rtc.h"
#include "glucose.h"
#include "nor_qspi_busy.h"
#include "buzzer.h"
#include "ppg.h"
#include "i2c_probe.h"
#include "config_manager.h"
#include "resource.h"
#include "resource.h"
#include "record_store.h"
#if defined(CONFIG_SETTINGS)
#include <zephyr/settings/settings.h>
#endif
#include "resource_fonts.h"
#include "ui_theme.h"
#include "resource_icons.h"
#include "temp.h"
#include "health_sched.h"
#include "ui_agent.h"
#if defined(CONFIG_CALIBRATION)
#include "calibration.h"
#endif
#include "product_hw_variant.h"
#if defined(CONFIG_GLUCOSE_MATRICES_FROM_XIP)
#include "glucose_model_xip.h"
#endif

#include "resource_logo.h"
#include "diag_monitor.h"
#include "ram_budget.h"
#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/flash/nrf_qspi_nor.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/drivers/charger.h>
#include <zephyr/devicetree.h>
#include <zephyr/version.h>
#include "git_version.h"
#if defined(CONFIG_MCUBOOT_IMG_MANAGER)
#include <zephyr/dfu/mcuboot.h>
#endif
#include "ota_progress_ui.h"
#include <lvgl.h>

#if defined(CONFIG_PPG_ALGO)
#include <zephyr/subsys/ppg_algo/ppg_algo.h>
#endif

/* MAX32664 sensor ecosystem API */
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/max32664.h>

#ifdef CONFIG_APP_FEATURE_WIFI
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/drivers/wifi/wexx.h>
#endif

#ifdef CONFIG_APP_FEATURE_CLOUD_TELEMETRY
#include "cloud_telemetry.h"
#endif

#ifdef CONFIG_APP_FEATURE_BLE
#include "ble.h"
#endif

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/**
 * @brief Custom fatal error handler for enhanced stack overflow diagnostics
 * 
 * Called before the default Zephyr fatal error handler when CONFIG_MPU_STACK_GUARD
 * detects a stack overflow. Provides detailed thread and stack information
 * to aid in debugging.
 */
#if defined(CONFIG_MPU_STACK_GUARD)
void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf)
{
	ARG_UNUSED(esf);

	/* Use printk (not LOG) - this must work even before LOG backend init */
	printk("=== FATAL ERROR reason=0x%x ===\n", reason);

	if (reason == K_ERR_STACK_CHK_FAIL) {
		printk("*** STACK OVERFLOW DETECTED ***\n");
	}

	/* Print current thread info */
	struct k_thread *current_thread = k_current_get();
	printk("Faulting thread: %p\n", (void *)current_thread);

	/* Also try LOG in case backend is up */
	LOG_ERR("=== FATAL ERROR DETECTED === reason=0x%x", reason);

	/* HALT - do NOT return. Returning would let the default handler call
	 * sys_reboot(), hiding the crash state. Halt here so J-Link can
	 * inspect the PC/LR/fault registers. */
	printk("HALTING - connect J-Link to inspect crash state\n");
	for (;;) {
		/* Keep core clocks active for more reliable SWD/JTAG attach after faults. */
		k_busy_wait(1000);
	}
}
#endif

/* Build time macros */
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

/* Hardware Info Example - Read Nordic Device ID */
static void read_device_info(void)
{
	uint8_t dev_id[16];
	int ret;
	size_t length = sizeof(dev_id);

	LOG_INF("=== Firmware Information ===");

	/* Print memory layout for debugging */
	extern char __bss_start[];
	extern char __bss_end[];
	extern char _image_ram_start[];
	extern char __ramfunc_start[];
	
	uint32_t ram_start = (uint32_t)_image_ram_start;
	uint32_t bss_start = (uint32_t)__bss_start;
	uint32_t bss_end = (uint32_t)__bss_end;
	uint32_t ram_func = (uint32_t)__ramfunc_start;
	
	uint32_t bss_size = bss_end - bss_start;
	uint32_t used_ram = bss_end - ram_start;
	
	LOG_DBG("Memory Layout:");
	LOG_DBG("  RAM Start:     0x%08x", ram_start);
	LOG_DBG("  BSS Start:     0x%08x", bss_start);
	LOG_DBG("  BSS End:       0x%08x", bss_end);
	LOG_DBG("  RAM Func:      0x%08x", ram_func);
	LOG_DBG("Memory Usage:");
	LOG_DBG("  BSS Size:      %u bytes", bss_size);
	LOG_DBG("  Used RAM:      %u bytes", used_ram);
	
	LOG_INF("=== End Firmware Information ===");

	LOG_INF("=== Hardware Information ===");

	ret = hwinfo_get_device_id(dev_id, length);
	if (ret < 0) {
		LOG_ERR("Failed to read device ID: %d", ret);
		return;
	}

	LOG_INF("Device ID length: %d bytes", ret);
	LOG_HEXDUMP_INF(dev_id, ret, "Device ID:");

	/* Display as hex string for easy reading */
	char hex_string[33] = {0}; /* 16 bytes * 2 + null terminator */
	for (int i = 0; i < ret && i < 16; i++) {
		snprintf(&hex_string[i*2], 3, "%02x", dev_id[i]);
	}
	LOG_INF("Device ID (hex): %s", hex_string);

	LOG_INF("Product SKU: %s", product_hw_variant_name(product_hw_detect()));
#if DT_HAS_ALIAS(max32664_hub)
	{
		const struct device *hub = DEVICE_DT_GET_OR_NULL(DT_ALIAS(max32664_hub));

		if (hub != NULL && device_is_ready(hub)) {
			LOG_INF("MAX32664 hub variant: %d",
				(int)max32664_get_variant(hub));
		}
	}
#endif

	/* Get reset cause */
	uint32_t reset_cause;
	ret = hwinfo_get_reset_cause(&reset_cause);
	if (ret == 0) {
		LOG_INF("Reset cause: 0x%08x", reset_cause);
	} else {
		LOG_WRN("Failed to read reset cause: %d", ret);
	}

	LOG_INF("=== End Hardware Information ===");
}


int main(void)
{
	/* DIAGNOSTIC: First output - if this prints, main() is running.
	 * Uses printk (not LOG) so it works before LOG backend init. */
	printk("=== APP MAIN START ===\n");

	/* Power first — I2C1 / hub need Buck2 before product_hw_detect. */
	LOG_INF("Initializing power rails via MAX20360...");
	if (power_init() != 0) {
		LOG_ERR("Power initialization failed — halting");
		return -EIO;
	}
	LOG_INF("Power rails enabled — all regulators configured");

	/* Product SKU (Watch/Pulse) — cached for PPG, glucose, BLE name. */
	(void)product_hw_detect();

	read_device_info();

	LOG_INF("========================================");
	LOG_INF("Build: %s %s", BUILD_DATE, BUILD_TIME);
	LOG_INF("========================================");

	LOG_INF("=== Module Versions ===");
	LOG_INF("Firmware:    %s (%s)", FW_VERSION_STRING, GIT_VERSION_STRING);
	LOG_INF("Git:         %s", GIT_HASH_FULL);
	LOG_INF("Zephyr:      %s", KERNEL_VERSION_STRING);
	LOG_INF("LVGL:        %d.%d.%d", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
#if defined(CONFIG_PPG_ALGO)
	LOG_INF("PPG Algo:    %s", ppg_algo_version());
#endif
	LOG_INF("=== End Module Versions ===");

	LOG_INF("Initializing RTC (early init for timestamp sync)...");
	if (rtc_init() != 0) {
		LOG_ERR("RTC init failed (continuing with uptime timestamps)");
	} else {
		LOG_INF("RTC initialized - timestamps now synced with RTC");
	}

	LOG_INF("Probing I2C buses for connected devices...");
	i2c_probe_all_buses();

#if defined(CONFIG_RESOURCE)
	const struct device *ext_flash = DEVICE_DT_GET(DT_NODELABEL(mx25r64));
	if (device_is_ready(ext_flash)) {
		LOG_INF("Enabling XIP mmap for external flash...");
		nrf_qspi_nor_xip_enable(ext_flash, true);
		LOG_INF("XIP mmap enabled @ 0x12000000");
	} else {
		LOG_ERR("External flash not ready — Resource/Model unavailable");
	}

	LOG_INF("Verifying Resource store (UI fonts/logos/icons)...");
	int res_ret = resource_init();
	if (res_ret != 0) {
		LOG_WRN("Resource init failed: %d (UI may degrade)", res_ret);
	}
	resource_print_info();
	(void)resource_validate_all();

	resource_logo_init();
	resource_icons_init();
#endif

#if defined(CONFIG_GLUCOSE_MATRICES_FROM_XIP)
	LOG_INF("Verifying Model store (clinical pack CRC + SKU entry)...");
	int model_ret = model_store_boot_verify();
	if (model_ret != 0) {
		LOG_ERR("Model store verify failed: %d", model_ret);
	}
#endif

#if defined(CONFIG_CALIBRATION) && defined(CONFIG_RESOURCE)
	LOG_INF("Loading factory calibration...");
	int calib_ret = calibration_init();
	if (calib_ret != 0) {
		LOG_WRN("Factory calibration unavailable: %d (partition empty or invalid)",
			calib_ret);
	}
	calibration_log_apply_summary();
#endif

	LOG_INF("Initializing LED status manager...");
	if (led_status_init() != 0) {
		LOG_ERR("LED status init failed");
		return -EIO;
	}

	LOG_INF("Initializing buzzer PWM...");
	if (buzzer_init() != 0) {
		LOG_ERR("Buzzer init failed (continuing without buzzer)");
	}

#if defined(CONFIG_APP_FEATURE_DISPLAY)
	LOG_INF("Initializing hardware buttons (SW1/SW2 GPIO)...");
	if (ui_buttons_gpio_early_init() != 0) {
		LOG_WRN("Hardware button GPIO init failed (continuing)");
	}
#endif

	LOG_INF("Initializing UI via st7789v/ft6336/lvgl...");
	if (ui_init() != 0) {
		LOG_ERR("UI init failed");
		return -EIO;
	}

	ram_budget_log_linker_tail();
#if defined(CONFIG_UI_SCREEN_DEVICE_INFO)
	if (device_info_ui_init() != 0) {
		LOG_WRN("Device Info event hook init failed");
	}
#endif
	/* Force one initial refresh before any potentially slow storage work so the
	 * first screen becomes visible even if FAT mount/format takes a while. */
	lv_timer_handler();

#if defined(CONFIG_UI_SCREEN_DEVICE_INFO)
	device_info_ui_on_fonts_ready();
#if defined(CONFIG_UI_SCREEN_HOME_INFO)
	home_info_ui_on_fonts_ready();
#endif
	lv_timer_handler();
#endif

	/* Settings NVS backend must be up before record_store meta / config_manager
	 * call settings_save_one(); without this those saves return -ENOENT (-2). */
#if defined(CONFIG_SETTINGS)
	{
		int settings_ret = settings_subsys_init();

		if (settings_ret != 0) {
			LOG_ERR("settings_subsys_init failed: %d", settings_ret);
		} else {
			LOG_INF("Settings subsystem ready (NVS)");
		}
	}
#endif

	/* Structured NOR record store (primary measurement persistence). */
	LOG_INF("Initializing NOR record store...");
	if (record_store_init() != 0) {
		LOG_ERR("Record store init failed (measurements will not persist)");
	}

	home_last_results_load();
#if defined(CONFIG_UI_SCREEN_DEVICE_INFO)
	device_info_ui_update_screen();
	lv_timer_handler();
#endif

	/* Initialize config manager (NVS / settings) */
	LOG_INF("Initializing configuration manager...");
	if (config_manager_init() != 0) {
		LOG_WRN("Config manager init failed (using defaults)");
	} else {
		LOG_INF("Config loaded");
#ifdef CONFIG_APP_FEATURE_WIFI
		const struct config_wifi *wifi_cfg = config_get_wifi();
		LOG_INF("Config loaded - Wi-Fi SSID: %s, enabled: %s",
			wifi_cfg->ssid, wifi_cfg->enabled ? "yes" : "no");
#endif
	}

	if (power_mgr_init() != 0) {
		LOG_WRN("Power manager init failed");
	}

#ifdef CONFIG_APP_FEATURE_WIFI
	/* Initialize Wi-Fi and connect using config */
	LOG_INF("Initializing Wi-Fi module...");
	const struct device *wifi_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(wifi0));
	if (wifi_dev && device_is_ready(wifi_dev)) {
		const struct config_wifi *wifi_cfg = config_get_wifi();
		if (wifi_cfg->enabled && wifi_cfg->ssid[0] != '\0') {
			LOG_INF("Connecting to Wi-Fi: %s", wifi_cfg->ssid);
			
			/* Use standard Zephyr Wi-Fi management API */
			struct net_if *iface = net_if_lookup_by_dev(wifi_dev);
			if (!iface) {
				LOG_ERR("Network interface not found");
			} else {
				struct wifi_connect_req_params params = {
					.ssid = (uint8_t *)wifi_cfg->ssid,
					.ssid_length = strlen(wifi_cfg->ssid),
					.psk = (uint8_t *)wifi_cfg->password,
					.psk_length = strlen(wifi_cfg->password),
					.security = (wifi_cfg->password[0] == '\0') ? 
						    WIFI_SECURITY_TYPE_NONE : WIFI_SECURITY_TYPE_PSK,
					.channel = WIFI_CHANNEL_ANY,
					.timeout = SYS_FOREVER_MS
				};
				
				int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, 
						   &params, sizeof(params));
				if (ret == 0) {
					LOG_INF("Wi-Fi connection initiated");
				} else {
					LOG_WRN("Wi-Fi connection request failed: %d", ret);
				}
			}
		}
	} else {
		LOG_WRN("Wi-Fi device not ready");
	}

#ifdef CONFIG_APP_FEATURE_CLOUD_TELEMETRY
	if (cloud_telemetry_init() != 0) {
		LOG_WRN("Cloud telemetry init failed");
	}
#endif
#endif

#if defined(CONFIG_APP_FEATURE_PPG)
	LOG_INF("Initializing PPG measurement system...");
	if (ppg_init() != 0) {
		LOG_ERR("PPG init failed (non-fatal)");
	}
#endif

#if defined(CONFIG_GLUCOSE_SENSOR)
	LOG_INF("Initializing glucose sensor...");
	if (glucose_init() != 0) {
		LOG_ERR("Glucose sensor init failed");
	}
#endif

#if defined(CONFIG_APP_TEMP_MONITOR)
	LOG_INF("Initializing temperature monitor...");
	if (temp_init() != 0) {
		LOG_WRN("Temperature monitor init failed (continuing without)");
	}
#endif

#if defined(CONFIG_APP_HEALTH_SCHED)
	LOG_INF("Initializing health measurement scheduler...");
	if (health_sched_init() != 0) {
		LOG_WRN("Health scheduler init failed (continuing without auto cycles)");
	}
#endif

#if defined(CONFIG_APP_UI_AGENT)
	LOG_INF("Initializing agentic UI presenter...");
	if (ui_agent_init() != 0) {
		LOG_WRN("UI agent init failed (continuing without slideshow)");
	}
#endif

#ifdef CONFIG_APP_FEATURE_BLE
	/* BLE init may write config.json — FatFS must already be mounted. */
	if (app_ble_init() != 0) {
		LOG_ERR("BLE init failed (continuing without BLE)");
	}
	ota_progress_smp_hooks_init();
#endif

	/* Dump stack usage after initialization only when the diagnostics
	 * feature owns runtime monitoring. Keeping THREAD_ANALYZER enabled
	 * separately still helps the fatal handler without spamming boot logs. */
#if defined(CONFIG_APP_FEATURE_DIAG_MONITOR) && defined(CONFIG_THREAD_ANALYZER)
	LOG_INF("=== Stack usage after init ===");
	thread_analyzer_print(0);
#endif
#ifdef CONFIG_APP_FEATURE_DIAG_MONITOR
	/* Force one immediate dump so we have a baseline before the first
	 * interval elapses; diag_monitor thread will then pick up the cadence. */
	diag_monitor_dump_now();
#endif

#if defined(CONFIG_MCUBOOT_IMG_MANAGER)
	/* Confirm this image now that boot-time initialization has fully
	 * succeeded. MCUboot runs in swap-with-revert mode and the companion app
	 * uploads OTA images as a *test* swap (confirm=false), so without this
	 * call the bootloader reverts to the previous image on the next reset —
	 * making every successful BLE DFU silently disappear. Confirming here
	 * keeps the safety net intact: if an upgraded image faults before
	 * reaching this point, MCUboot still reverts automatically. */
	if (!boot_is_img_confirmed()) {
		int confirm_err = boot_write_img_confirmed();
		if (confirm_err) {
			LOG_ERR("MCUboot image confirm failed: %d", confirm_err);
		} else {
			LOG_INF("MCUboot image confirmed — OTA update is now permanent");
		}
	}
#endif

	bool display_was_asleep = false;

	while (1) {
#ifdef CONFIG_APP_FEATURE_BLE
		app_ble_tick();
#endif
		power_mgr_tick();

		/* Pause LVGL while the panel is asleep, or briefly while a QSPI
		 * program/erase (record store / model update) owns the NOR die —
		 * XIP fonts/icons share the same die, so concurrent access would
		 * garble reads.
		 */
		bool display_asleep = power_mgr_display_asleep();

		/* On wake, re-prime the XIP mapping (exit DPD) before the first
		 * draw so fonts/icons are not read from a sleeping flash. */
		if (display_was_asleep && !display_asleep) {
			resource_wake();
		}
		display_was_asleep = display_asleep;

		if (!display_asleep && !nor_qspi_busy()) {
			/* Update UI first to batch all changes, then process LVGL
			 * timers/refresh so all UI changes complete before refresh.
			 */
			ui_update();
			lv_timer_handler();
		}
		/* 20 ms: less LVGL stack pressure and sys_work_q congestion. */
		k_msleep(20);
	}

	return 0;
}