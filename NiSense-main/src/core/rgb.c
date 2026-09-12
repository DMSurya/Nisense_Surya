#include "rgb.h"

LOG_MODULE_REGISTER(rgb, LOG_LEVEL_INF);

/* MAX20360 I2C configuration from overlay */
#define MAX20360_I2C_BUS DT_NODELABEL(i2c0)
#define MAX20360_ADDR    0x28
#define LED0_CTR         0x7A  /* Blue  */
#define LED1_CTR         0x7B  /* Red   */
#define LED2_CTR         0x7C  /* Green */
#define LED_OFF          0x00
#define LED_ON           0x20  /* ~0.6 mA current */

struct rgb_state {
    bool blue;
    bool red;
    bool green;
};

static const struct rgb_state rgb_patterns[RGB_PATTERN_COUNT] = {
    {0,0,0},  /* OFF */
    {1,0,0},  /* Blue    */
    {0,1,0},  /* Red     */
    {0,0,1},  /* Green   */
    {1,1,0},  /* Magenta */
    {1,0,1},  /* Cyan    */
    {0,1,1},  /* Yellow  */
    {1,1,1}   /* White   */
};

const char *rgb_color_names[RGB_PATTERN_COUNT] = {
    "OFF","Blue","Red","Green","Magenta","Cyan","Yellow","White"
};

static int led_write(uint8_t reg, uint8_t val)
{
    const struct device *i2c_dev = DEVICE_DT_GET_OR_NULL(MAX20360_I2C_BUS);
    if (i2c_dev == NULL) {
        LOG_ERR("I2C device not found in device tree");
        return -ENODEV;
    }
    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("I2C device not ready");
        return -ENODEV;
    }
    return i2c_reg_write_byte(i2c_dev, MAX20360_ADDR, reg, val);
}

int rgb_init(void)
{
    LOG_INF("Initializing MAX20360 RGB LEDs...");
    const struct device *i2c_dev = DEVICE_DT_GET_OR_NULL(MAX20360_I2C_BUS);
    if (i2c_dev == NULL) {
        LOG_ERR("I2C device not found in device tree");
        return -ENODEV;
    }
    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("I2C bus not ready");
        return -ENODEV;
    }
    /* Default OFF */
    return rgb_set_pattern(0);
}

int rgb_set_pattern(int index)
{
    if (index < 0 || index >= RGB_PATTERN_COUNT) return -EINVAL;
    const struct rgb_state *s = &rgb_patterns[index];
    int r = 0;
    r |= led_write(LED0_CTR, s->blue  ? LED_ON : LED_OFF);
    r |= led_write(LED1_CTR, s->red   ? LED_ON : LED_OFF);
    r |= led_write(LED2_CTR, s->green ? LED_ON : LED_OFF);
    LOG_DBG("LEDs set to %s", rgb_color_names[index]);  /* Reduced to DBG - called frequently */
    return r;
}
