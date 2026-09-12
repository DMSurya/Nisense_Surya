/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PPG Algorithm Subsystem - Shell Commands
 *
 * Provides shell interface for testing and debugging PPG subsystem.
 */

#include "ppg_algo_priv.h"
#include <zephyr/shell/shell.h>
#include <stdlib.h>

LOG_MODULE_DECLARE(ppg_algo, CONFIG_PPG_ALGO_LOG_LEVEL);

static int cmd_ppg_source(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (!ppg_state.initialized) {
		shell_warn(sh, "PPG subsystem not initialized");
		return -ENODEV;
	}

	const char *type_str;
	switch (ppg_state.source_type) {
	case PPG_SOURCE_MAX32664_HUB: type_str = "MAX32664 (HUB mode, WHRM+WSpO2 onboard)"; break;
	case PPG_SOURCE_MAX86141:     type_str = "MAX86141 (hub passthrough/raw)";          break;
	case PPG_SOURCE_MAX3010X:     type_str = "MAX30102 (classic I2C)";                  break;
	default:                      type_str = "none";                                    break;
	}

	shell_print(sh, "PPG source: %s", ppg_algo_get_source_name());
	shell_print(sh, "  Type   : %s", type_str);
	shell_print(sh, "  Hub    : %s",
		    ppg_state.hub_dev ? ppg_state.hub_dev->name : "n/a");
	shell_print(sh, "  Accel  : %s",
		    ppg_state.accel_dev ? ppg_state.accel_dev->name : "n/a");

	uint32_t caps = ppg_algo_get_capabilities();
	shell_print(sh, "  Caps   : HR=%c SpO2=%c Green=%c MotionRej=%c",
		    (caps & PPG_CAP_HR)               ? 'Y' : '-',
		    (caps & PPG_CAP_SPO2)             ? 'Y' : '-',
		    (caps & PPG_CAP_GREEN_LED)        ? 'Y' : '-',
		    (caps & PPG_CAP_MOTION_REJECTION) ? 'Y' : '-');
	return 0;
}

static int cmd_ppg_status(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "PPG Algorithm Subsystem v%s", PPG_ALGO_VERSION_STRING);
	shell_print(sh, "Status: %s", ppg_state.measuring ? "MEASURING" : "IDLE");
	
	if (ppg_state.measuring) {
		uint32_t elapsed = k_uptime_get_32() - ppg_state.start_time_ms;
		shell_print(sh, "  Elapsed: %u ms", elapsed);
		shell_print(sh, "  Samples: %u / %u",
			    ppg_state.samples_processed, ppg_state.config.sample_count);
		shell_print(sh, "  Progress: %u%%",
			    (ppg_state.samples_processed * 100) / ppg_state.config.sample_count);
	}

	shell_print(sh, "Configuration:");
	shell_print(sh, "  Sample rate: %u Hz", ppg_state.config.sample_rate_hz);
	shell_print(sh, "  Sample count: %u", ppg_state.config.sample_count);
	shell_print(sh, "  Type: Vitals");
	shell_print(sh, "  Motion rejection: %s",
		    ppg_state.config.motion_rejection ? "enabled" : "disabled");
	shell_print(sh, "  Quality threshold: %u%%", ppg_state.config.quality_threshold);

	return 0;
}

static int cmd_ppg_start(const struct shell *sh, size_t argc, char **argv)
{
	struct ppg_algo_config config;
	int ret;

	/* Use default config or parse arguments */
	memcpy(&config, &ppg_state.config, sizeof(config));

	if (argc > 1) {
		/* Parse sample count */
		config.sample_count = atoi(argv[1]);
		if (config.sample_count == 0 || config.sample_count > PPG_MAX_SAMPLES) {
			shell_error(sh, "Invalid sample count (1-%u)", PPG_MAX_SAMPLES);
			return -EINVAL;
		}
	}

	ret = ppg_algo_start_measurement(&config);
	if (ret) {
		shell_error(sh, "Failed to start measurement: %d", ret);
		return ret;
	}

	shell_print(sh, "Measurement started: Vitals, %u samples", config.sample_count);

	return 0;
}

static int cmd_ppg_stop(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	int ret = ppg_algo_stop_measurement();
	if (ret) {
		shell_error(sh, "Failed to stop measurement: %d", ret);
		return ret;
	}

	shell_print(sh, "Measurement stopped");
	return 0;
}

static int cmd_ppg_result(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	struct ppg_algo_result result;
	int ret = ppg_algo_get_result(&result);

	if (ret == -ENODATA) {
		shell_print(sh, "No result available yet");
		return 0;
	} else if (ret) {
		shell_error(sh, "Failed to get result: %d", ret);
		return ret;
	}

	shell_print(sh, "PPG Measurement Result:");
	shell_print(sh, "  Type: Vitals");
	shell_print(sh, "  Timestamp: %u ms", result.timestamp_ms);
	shell_print(sh, "  Samples: %u", result.sample_count);
	shell_print(sh, "");
	shell_print(sh, "Heart Rate:");
	shell_print(sh, "  BPM: %u", result.hr_bpm);
	shell_print(sh, "  Confidence: %u%%", result.hr_confidence);
	shell_print(sh, "  Valid: %s", result.hr_valid ? "YES" : "NO");
	shell_print(sh, "");
	shell_print(sh, "SpO2:");
	shell_print(sh, "  Percentage: %u%%", result.spo2_percent);
	shell_print(sh, "  Confidence: %u%%", result.spo2_confidence);
	shell_print(sh, "  Valid: %s", result.spo2_valid ? "YES" : "NO");
	shell_print(sh, "  R-value: %u.%03u", result.r_value_x1000 / 1000, result.r_value_x1000 % 1000);
	shell_print(sh, "");
	shell_print(sh, "Signal Quality:");
	shell_print(sh, "  Quality: %u", result.quality);
	shell_print(sh, "  PI: %u.%u%%", result.perfusion_index / 10, result.perfusion_index % 10);
	shell_print(sh, "  SNR: %u.%u dB", result.snr_db / 10, result.snr_db % 10);
	shell_print(sh, "  Motion: %s (%u mg)",
		    result.motion_detected ? "YES" : "NO", result.motion_magnitude);

	return 0;
}

static int cmd_ppg_reset(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	ppg_algo_reset();
	shell_print(sh, "Algorithm state reset");

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_ppg,
	SHELL_CMD(source, NULL, "Show selected PPG hardware source + caps", cmd_ppg_source),
	SHELL_CMD(status, NULL, "Show PPG subsystem status", cmd_ppg_status),
	SHELL_CMD_ARG(start, NULL, "Start measurement [hr|spo2] [samples]", cmd_ppg_start, 1, 2),
	SHELL_CMD(stop, NULL, "Stop measurement", cmd_ppg_stop),
	SHELL_CMD(result, NULL, "Show latest result", cmd_ppg_result),
	SHELL_CMD(reset, NULL, "Reset algorithm state", cmd_ppg_reset),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(ppg, &sub_ppg, "PPG Algorithm Subsystem commands", NULL);
