/* buzzer.c - PWM Buzzer Control Implementation
 *
 * Controls buzzer on P0.12 using PWM1 instance.
 */

#include "buzzer.h"
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(buzzer, LOG_LEVEL_INF);

#define BUZZER_NODE DT_ALIAS(buzzer0)

#if DT_NODE_EXISTS(BUZZER_NODE)
static const struct pwm_dt_spec buzzer_pwm = PWM_DT_SPEC_GET(BUZZER_NODE);
static bool is_initialized = false;
static bool is_playing_tone = false;

/* Forward declaration for timer handler */
static void buzzer_stop_timer_handler(struct k_timer *timer);
static void buzzer_pattern_work_handler(struct k_work *work);

/* Timer for automatic tone stop (non-blocking) */
K_TIMER_DEFINE(buzzer_stop_timer, buzzer_stop_timer_handler, NULL);

/* Work queue for non-blocking multi-tone patterns */
static struct k_work_delayable buzzer_pattern_work;
static bool pattern_work_initialized = false;

/* Pattern state machine */
enum buzzer_pattern_type {
	BUZZER_PATTERN_NONE = 0,
	BUZZER_PATTERN_SUCCESS,
	BUZZER_PATTERN_ERROR,
	BUZZER_PATTERN_WARNING
};

static enum buzzer_pattern_type current_pattern = BUZZER_PATTERN_NONE;
static int pattern_step = 0;
static uint8_t buzzer_volume_pct = 50;

int buzzer_set_volume_percent(uint8_t percent)
{
	if (percent > 100U) {
		return -EINVAL;
	}

	buzzer_volume_pct = percent;
	LOG_INF("Buzzer volume set to %u%%", percent);
	return 0;
}

uint8_t buzzer_get_volume_percent(void)
{
	return buzzer_volume_pct;
}

int buzzer_init(void)
{
	if (is_initialized) {
		LOG_WRN("Buzzer already initialized");
		return 0;
	}
	
	if (!device_is_ready(buzzer_pwm.dev)) {
		LOG_ERR("PWM device %s not ready", buzzer_pwm.dev->name);
		return -ENODEV;
	}
	
	/* Ensure buzzer is off initially */
	int ret = pwm_set_pulse_dt(&buzzer_pwm, 0);
	if (ret < 0) {
		LOG_ERR("Failed to initialize buzzer PWM: %d", ret);
		return ret;
	}
	
	is_initialized = true;
	is_playing_tone = false;
	
	/* Initialize pattern work queue */
	if (!pattern_work_initialized) {
		k_work_init_delayable(&buzzer_pattern_work, buzzer_pattern_work_handler);
		pattern_work_initialized = true;
	}
	
	LOG_INF("Buzzer initialized on %s channel %d", buzzer_pwm.dev->name, buzzer_pwm.channel);
	
	return 0;
}

/**
 * @brief Timer handler to automatically stop buzzer after duration
 * 
 * Runs in ISR context - only calls buzzer_stop() which is safe (no blocking ops)
 */
static void buzzer_stop_timer_handler(struct k_timer *timer)
{
	ARG_UNUSED(timer);

	/* Expiry callback can run in interrupt context.
	 * Stop PWM directly here instead of calling buzzer_stop(), which also
	 * manipulates timer state and is intended for thread-context calls.
	 */
	int ret = pwm_set_pulse_dt(&buzzer_pwm, 0);
	if (ret < 0) {
		LOG_ERR("Failed to auto-stop PWM: %d", ret);
	}
	is_playing_tone = false;
	LOG_DBG("Auto-stopped buzzer after duration");
}

int buzzer_play_tone(uint32_t frequency_hz, uint8_t duty_cycle_percent, uint32_t duration_ms)
{
	if (!is_initialized) {
		LOG_ERR("Buzzer not initialized");
		return -ENODEV;
	}
	
	if (frequency_hz < 20 || frequency_hz > 20000) {
		LOG_ERR("Invalid frequency: %u Hz (range: 20-20000 Hz)", frequency_hz);
		return -EINVAL;
	}
	
	if (duty_cycle_percent > 100) {
		LOG_ERR("Invalid duty cycle: %u%% (range: 0-100%%)", duty_cycle_percent);
		return -EINVAL;
	}

	uint8_t scaled_duty = (uint8_t)((uint16_t)duty_cycle_percent * buzzer_volume_pct / 100U);
	
	/* Calculate PWM period from frequency */
	uint32_t period_ns = (1000000000U / frequency_hz);
	uint32_t pulse_ns = (period_ns * scaled_duty) / 100;
	
	int ret = pwm_set_dt(&buzzer_pwm, period_ns, pulse_ns);
	if (ret < 0) {
		LOG_ERR("Failed to set PWM: %d", ret);
		return ret;
	}
	
	is_playing_tone = true;
	LOG_INF("Playing tone: %u Hz, %u%% duty cycle (%u%% scaled), %u ms (period=%u ns, pulse=%u ns)",
		frequency_hz, duty_cycle_percent, scaled_duty, duration_ms, period_ns, pulse_ns);

	/* Start timer for automatic stop (non-blocking)
	 * If duration_ms is 0, tone plays indefinitely until manual buzzer_stop() */
	if (duration_ms > 0) {
		k_timer_start(&buzzer_stop_timer, K_MSEC(duration_ms), K_NO_WAIT);
	}
	return 0;
}

int buzzer_stop(void)
{
	if (!is_initialized) {
		LOG_ERR("Buzzer not initialized");
		return -ENODEV;
	}
	
	/* Stop any active timer (thread-context only).
	 * In ISR/expiry context, avoid timer stop operations and just silence PWM.
	 */
	if (!k_is_in_isr()) {
		k_timer_stop(&buzzer_stop_timer);
	}

	int ret = pwm_set_pulse_dt(&buzzer_pwm, 0);
	if (ret < 0) {
		LOG_ERR("Failed to stop PWM: %d", ret);
		return ret;
	}
	
	is_playing_tone = false;
	LOG_DBG("Buzzer stopped");
	
	return 0;
}

bool buzzer_is_playing(void)
{
	return is_initialized && is_playing_tone;
}

/**
 * @brief Cancel any running pattern and stop buzzer immediately
 */
static void buzzer_cancel_pattern(void)
{
	k_work_cancel_delayable(&buzzer_pattern_work);
	current_pattern = BUZZER_PATTERN_NONE;
	pattern_step = 0;
	buzzer_stop();
}

/**
 * @brief Work handler for multi-tone patterns (non-blocking state machine)
 * 
 * Executes pattern steps with scheduled delays between tones.
 * Each step plays one tone, then reschedules for the next step.
 * Preemptible: calling a new pattern cancels the current one.
 */
static void buzzer_pattern_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	
	switch (current_pattern) {
	case BUZZER_PATTERN_SUCCESS:
		if (pattern_step == 0) {
			buzzer_play_tone(BUZZER_TONE_C5, 30, 40);   /* C5 40ms */
			pattern_step = 1;
			k_work_reschedule(&buzzer_pattern_work, K_MSEC(140));  /* 40ms tone + 100ms pause */
		} else {
			buzzer_play_tone(BUZZER_TONE_E4, 30, 40);   /* E4 40ms */
			current_pattern = BUZZER_PATTERN_NONE;
			pattern_step = 0;
		}
		break;
		
	case BUZZER_PATTERN_ERROR:
		if (pattern_step < 3) {
			buzzer_play_tone(BUZZER_TONE_C4, 30, 100);  /* C4 100ms */
			pattern_step++;
			if (pattern_step < 3) {
				k_work_reschedule(&buzzer_pattern_work, K_MSEC(250));  /* 100ms + 150ms gap */
			} else {
				current_pattern = BUZZER_PATTERN_NONE;
				pattern_step = 0;
			}
		}
		break;
		
	case BUZZER_PATTERN_WARNING:
		if (pattern_step < 2) {
			buzzer_play_tone(BUZZER_TONE_C4, 30, 100);  /* C4 100ms */
			pattern_step++;
			if (pattern_step < 2) {
				k_work_reschedule(&buzzer_pattern_work, K_MSEC(250));  /* 100ms + 150ms gap */
			} else {
				current_pattern = BUZZER_PATTERN_NONE;
				pattern_step = 0;
			}
		}
		break;
		
	default:
		current_pattern = BUZZER_PATTERN_NONE;
		pattern_step = 0;
		break;
	}
}

void buzzer_play_success(void)
{
	if (!is_initialized) return;
	buzzer_cancel_pattern();
	current_pattern = BUZZER_PATTERN_SUCCESS;
	k_work_schedule(&buzzer_pattern_work, K_NO_WAIT);
}

void buzzer_play_error(void)
{
	if (!is_initialized) return;
	buzzer_cancel_pattern();
	current_pattern = BUZZER_PATTERN_ERROR;
	k_work_schedule(&buzzer_pattern_work, K_NO_WAIT);
}

void buzzer_play_warning(void)
{
	if (!is_initialized) return;
	buzzer_cancel_pattern();
	current_pattern = BUZZER_PATTERN_WARNING;
	k_work_schedule(&buzzer_pattern_work, K_NO_WAIT);
}

#else
/* Buzzer not configured in device tree */
int buzzer_init(void)
{
	LOG_ERR("Buzzer not configured in device tree");
	return -ENOTSUP;
}

int buzzer_play_tone(uint32_t frequency_hz, uint8_t duty_cycle_percent, uint32_t duration_ms)
{
	ARG_UNUSED(frequency_hz);
	ARG_UNUSED(duty_cycle_percent);
	ARG_UNUSED(duration_ms);
	return -ENOTSUP;
}

int buzzer_stop(void)
{
	return -ENOTSUP;
}

bool buzzer_is_playing(void)
{
	return false;
}

void buzzer_play_success(void) {}
void buzzer_play_error(void) {}
void buzzer_play_warning(void) {}

#endif /* DT_NODE_EXISTS(BUZZER_NODE) */
