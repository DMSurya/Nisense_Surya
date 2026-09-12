/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * MAX20360 internal SAR ADC — logical channel IDs.
 *
 * Use these as `channel_id` in adc_channel_setup() / adc_sequence.channels
 * (BIT(id)) against the maxim,max20360-adc device.  Channels 0-9 are routed
 * through the IVMON multiplexer (selected on the PMIC slave 0x28); VHDIN is
 * converted directly by the ADC.
 *
 * All channels are referenced to the 5.5 V VIVMON/VHDIN full scale
 * (ref_internal = 5500 mV, 8-bit resolution, LSB 21.57 mV).  If a channel
 * is configured with ADC_GAIN_1_2 / 1_3 / 1_4, the driver engages the
 * matching IVMON divider ratio; adc_raw_to_millivolts() then recovers the
 * rail voltage from the gain automatically.
 */
#ifndef ZEPHYR_DRIVERS_ADC_MAX20360_ADC_H_
#define ZEPHYR_DRIVERS_ADC_MAX20360_ADC_H_

#define MAX20360_ADC_CHAN_BAT     0  /* Battery voltage (BAT pin) */
#define MAX20360_ADC_CHAN_SYS     1  /* System voltage (SYS pin) */
#define MAX20360_ADC_CHAN_BK1OUT  2  /* Buck1 output */
#define MAX20360_ADC_CHAN_BK2OUT  3  /* Buck2 output */
#define MAX20360_ADC_CHAN_BK3OUT  4  /* Buck3 output */
#define MAX20360_ADC_CHAN_L1OUT   5  /* LDO1 output */
#define MAX20360_ADC_CHAN_L2OUT   6  /* LDO2 output */
#define MAX20360_ADC_CHAN_SFOUT   7  /* SFOUT LDO output */
#define MAX20360_ADC_CHAN_BBOUT   8  /* Buck-boost output */
#define MAX20360_ADC_CHAN_ISET    9  /* Charger current sense voltage (VISET) */
#define MAX20360_ADC_CHAN_VHDIN   10 /* Haptic driver supply (direct, no IVMON) */

#define MAX20360_ADC_CHAN_COUNT   11

#endif /* ZEPHYR_DRIVERS_ADC_MAX20360_ADC_H_ */
