/* led_status.c – RGB LED Status Manager Implementation
 *
 * Background thread handles LED animations (breathing, blinking).
 * Priority queue ensures highest priority status is always shown.
 */

#include "led_status.h"
#include "rgb.h"
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(led_status, LOG_LEVEL_INF);

/* Animation thread config
 * Stack size exposed via CONFIG_LED_STATUS_THREAD_STACK_SIZE so it can be
 * tuned from prj.conf against the diag-monitor high-watermark.
 */
#define LED_THREAD_STACK_SIZE CONFIG_LED_STATUS_THREAD_STACK_SIZE
#define LED_THREAD_PRIORITY   12    /* Low priority background task */
#define LED_STACK_WARN_BYTES  256

/* Animation timing (milliseconds) */
#define BLINK_FAST_MS       150
#define BLINK_SLOW_MS       500
#define BREATH_STEP_MS      50
#define BREATH_STEPS        20   /* Steps for fade in/out */
#define FLASH_DEFAULT_MS    200

/* RGB pattern indices (from rgb.h) */
#define RGB_OFF      0
#define RGB_BLUE     1
#define RGB_RED      2
#define RGB_GREEN    3
#define RGB_MAGENTA  4
#define RGB_CYAN     5
#define RGB_YELLOW   6
#define RGB_WHITE    7

/* Module state */
static uint32_t active_states;       /* Bitmask of active status flags */
static bool force_off;               /* Force LED off override */
static bool flash_active;            /* One-shot flash in progress */
static int flash_pattern;            /* Pattern for one-shot flash */
static uint32_t flash_end_time;      /* When flash should end */
static K_MUTEX_DEFINE(led_mutex);

/* Animation thread */
static K_THREAD_STACK_DEFINE(led_stack, LED_THREAD_STACK_SIZE);
static struct k_thread led_thread_data;
static k_tid_t led_thread_id;
static bool thread_running;

/* Status to pattern mapping */
struct led_status_config {
	int pattern;          /* RGB pattern when "on" */
	int pattern_off;      /* RGB pattern when "off" (for blink) */
	bool blink;           /* Does this status blink? */
	bool fast;            /* Fast or slow blink? */
	bool breathe;         /* Breathing animation? */
};

static const struct led_status_config status_config[] = {
	[LED_STATUS_ERROR]            = { RGB_RED,    RGB_OFF, true,  true,  false },
	[LED_STATUS_BATTERY_CRITICAL] = { RGB_RED,    RGB_OFF, true,  true,  false },
	[LED_STATUS_BATTERY_LOW]      = { RGB_RED,    RGB_OFF, true,  false, false },
	[LED_STATUS_CHARGING]         = { RGB_YELLOW, RGB_OFF, false, false, true  },
	[LED_STATUS_CHARGE_COMPLETE]  = { RGB_GREEN,  RGB_OFF, false, false, false },
	[LED_STATUS_MEASURING]        = { RGB_BLUE,   RGB_OFF, true,  false, false },
	[LED_STATUS_BLE_CONNECTED]    = { RGB_CYAN,   RGB_OFF, false, false, false },
	[LED_STATUS_IDLE]             = { RGB_OFF,    RGB_OFF, false, false, false },
};

static const char *status_names[] = {
	"ERROR", "BATT_CRIT", "BATT_LOW", "CHARGING",
	"CHARGED", "MEASURING", "BLE_CONN", "IDLE"
};

/**
 * @brief Get highest priority active status
 */
static led_status_t get_active_status(void)
{
	for (int i = 0; i < LED_STATUS_COUNT; i++) {
		if (active_states & BIT(i)) {
			return (led_status_t)i;
		}
	}
	return LED_STATUS_IDLE;
}

/**
 * @brief LED animation thread
 */
static void led_thread_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	
	int animation_step = 0;
	bool led_on = true;
	bool breathing_up = true;
#if defined(CONFIG_THREAD_STACK_INFO) && defined(CONFIG_INIT_STACKS)
	uint32_t stack_check_counter = 0;
#endif
	
	while (thread_running) {
#if defined(CONFIG_THREAD_STACK_INFO) && defined(CONFIG_INIT_STACKS)
		if ((stack_check_counter++ % 50U) == 0U) {
			size_t unused = 0U;
			if (k_thread_stack_space_get(k_current_get(), &unused) == 0) {
				if (unused < LED_STACK_WARN_BYTES) {
					LOG_WRN("LED thread stack low: %u bytes free", (uint32_t)unused);
				}
			}
		}
#endif

		k_mutex_lock(&led_mutex, K_FOREVER);
		
		led_status_t status = get_active_status();
		
		/* Check for force-off override */
		if (force_off) {
			rgb_set_pattern(RGB_OFF);
			k_mutex_unlock(&led_mutex);
			k_msleep(100);
			continue;
		}
		
		/* Check for one-shot flash */
		if (flash_active) {
			if (k_uptime_get_32() < flash_end_time) {
				rgb_set_pattern(flash_pattern);
				k_mutex_unlock(&led_mutex);
				k_msleep(50);
				continue;
			} else {
				flash_active = false;
			}
		}
		
		const struct led_status_config *cfg = &status_config[status];
		
		if (cfg->breathe) {
			/* Breathing animation - simulate with on/off cycling */
			/* Full breathing would require PWM; we approximate with timing */
			if (breathing_up) {
				animation_step++;
				if (animation_step >= BREATH_STEPS) {
					breathing_up = false;
				}
			} else {
				animation_step--;
				if (animation_step <= 0) {
					breathing_up = true;
				}
			}
			
			/* Approximate breathing: on for longer at peak, shorter at trough */
			if (animation_step > BREATH_STEPS / 2) {
				rgb_set_pattern(cfg->pattern);
			} else {
				rgb_set_pattern(RGB_OFF);
			}
			
			k_mutex_unlock(&led_mutex);
			k_msleep(BREATH_STEP_MS);
			
		} else if (cfg->blink) {
			/* Blinking animation */
			rgb_set_pattern(led_on ? cfg->pattern : cfg->pattern_off);
			led_on = !led_on;
			
			k_mutex_unlock(&led_mutex);
			k_msleep(cfg->fast ? BLINK_FAST_MS : BLINK_SLOW_MS);
			
		} else {
			/* Solid color */
			rgb_set_pattern(cfg->pattern);
			
			k_mutex_unlock(&led_mutex);
			k_msleep(200);  /* Slow poll when no animation needed */
		}
	}
}

int led_status_init(void)
{
	int ret;
	
	/* Initialize RGB hardware */
	ret = rgb_init();
	if (ret != 0) {
		LOG_ERR("Failed to init RGB: %d", ret);
		return ret;
	}
	
	active_states = 0;
	force_off = false;
	flash_active = false;
	thread_running = true;
	
	/* Start animation thread */
	led_thread_id = k_thread_create(&led_thread_data, led_stack,
					K_THREAD_STACK_SIZEOF(led_stack),
					led_thread_func,
					NULL, NULL, NULL,
					LED_THREAD_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(led_thread_id, "led_status");
	
	LOG_INF("LED status manager initialized");
	return 0;
}

void led_status_set(led_status_t status)
{
	if (status >= LED_STATUS_COUNT) {
		return;
	}
	
	k_mutex_lock(&led_mutex, K_FOREVER);
	active_states |= BIT(status);
	k_mutex_unlock(&led_mutex);
	
	LOG_DBG("LED status set: %s", status_names[status]);
}

void led_status_clear(led_status_t status)
{
	if (status >= LED_STATUS_COUNT) {
		return;
	}
	
	k_mutex_lock(&led_mutex, K_FOREVER);
	active_states &= ~BIT(status);
	k_mutex_unlock(&led_mutex);
	
	LOG_DBG("LED status cleared: %s", status_names[status]);
}

led_status_t led_status_get(void)
{
	k_mutex_lock(&led_mutex, K_FOREVER);
	led_status_t status = get_active_status();
	k_mutex_unlock(&led_mutex);
	return status;
}

void led_status_flash(int rgb_pattern, uint32_t duration_ms)
{
	if (rgb_pattern < 0 || rgb_pattern >= RGB_PATTERN_COUNT) {
		return;
	}
	
	k_mutex_lock(&led_mutex, K_FOREVER);
	flash_pattern = rgb_pattern;
	flash_end_time = k_uptime_get_32() + duration_ms;
	flash_active = true;
	k_mutex_unlock(&led_mutex);
	
	LOG_DBG("LED flash: pattern %d for %u ms", rgb_pattern, duration_ms);
}

void led_status_force_off(bool force)
{
	k_mutex_lock(&led_mutex, K_FOREVER);
	force_off = force;
	k_mutex_unlock(&led_mutex);
	
	if (force) {
		rgb_set_pattern(RGB_OFF);
	}
	
	LOG_INF("LED force off: %s", force ? "enabled" : "disabled");
}
