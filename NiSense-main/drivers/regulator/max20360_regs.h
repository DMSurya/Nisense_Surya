/*
 * Copyright (c) 2025 MAX20360 Contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * MAX20360 PMIC Register Definitions
 * All register addresses and bit definitions for complete PMIC control
 */

#ifndef ZEPHYR_DRIVERS_REGULATOR_MAX20360_REGS_H_
#define ZEPHYR_DRIVERS_REGULATOR_MAX20360_REGS_H_

/* ===== Chip Identification ===== */
#define MAX20360_REG_CHIPID     0x00   /* Chip ID (read-only) */

/* ===== Status Registers (0x01-0x05) ===== */
#define MAX20360_REG_STATUS0    0x01   /* Thermal & charger status */
#define MAX20360_REG_STATUS1    0x02   /* USB/charger status flags */
#define MAX20360_REG_STATUS2    0x03   /* Charger thermal & LDO/LSW UVLO */
#define MAX20360_REG_STATUS3    0x04   /* System/fault status */
#define MAX20360_REG_STATUS4    0x05   /* Battery & charge/boost flags */

/* Status4 (0x05) — valid when CHGIN present per datasheet */
#define MAX20360_STATUS4_BATGOOD      BIT(7)  /* VBAT > VBAT_UVLO */
#define MAX20360_STATUS4_BATREGDONE   BIT(6)  /* VBAT >= VBAT_REG (charger) */
#define MAX20360_STATUS4_BSTFAULT     BIT(5)  /* Buck-boost fault */

/* Status3 register bit definitions for adaptive charging */
#define MAX20360_STATUS3_CHGSYSLIM    BIT(4)  /* CHGIN current limit active */
#define MAX20360_STATUS3_SYSBATLIM    BIT(5)  /* VSYS regulation active */

/* ===== Interrupt Status Registers (0x06-0x09) ===== */
#define MAX20360_REG_INT0       0x06   /* Interrupt status group 0 */
#define MAX20360_REG_INT1       0x07   /* Interrupt status group 1 */
#define MAX20360_REG_INT2       0x08   /* Interrupt status group 2 */
#define MAX20360_REG_INT3       0x09   /* Interrupt status group 3 */

/* Interrupt Status Register 0 (INT0) bit definitions */
#define MAX20360_INT0_CHGTMOINT     BIT(0)  /* Charger timeout */
#define MAX20360_INT0_USBOKINT      BIT(3)  /* CHGIN valid voltage detected */
#define MAX20360_INT0_THMSTATINT    BIT(7)  /* Temperature zone change */

/* Interrupt Status Register 2 (INT2) bit definitions */
#define MAX20360_INT2_THMBK1INT     BIT(0)  /* Buck1 thermal shutdown */
#define MAX20360_INT2_THMBK2INT     BIT(1)  /* Buck2 thermal shutdown */
#define MAX20360_INT2_THMBK3INT     BIT(2)  /* Buck3 thermal shutdown */
#define MAX20360_INT2_SYSBATLIMINT   BIT(5)  /* SysBatLim status change */

/* Interrupt Status Register 3 (INT3) bit definitions */
#define MAX20360_INT3_ADCSTATINT    BIT(0)  /* ADC status */
#define MAX20360_INT3_HPTSTATINT    BIT(1)  /* Haptic status */
#define MAX20360_INT3_I2CTMOINT     BIT(2)  /* I2C timeout */
#define MAX20360_INT3_I2CCRCININT   BIT(3)  /* I2C CRC failure (Fletcher-16) */
#define MAX20360_INT3_BSTFAULTINT   BIT(5)  /* Boost regulator fault */
#define MAX20360_INT3_BATREGDONEINT BIT(6)  /* BatRegDone change */
#define MAX20360_INT3_BATGOODINT    BIT(7)  /* Battery presence/UVLO */

/* ===== Interrupt Mask Registers (0x0A-0x0D) ===== */
#define MAX20360_REG_INTMASK0   0x0A   /* Interrupt mask group 0 */
#define MAX20360_REG_INTMASK1   0x0B   /* Interrupt mask group 1 */
#define MAX20360_REG_INTMASK2   0x0C   /* Interrupt mask group 2 */
#define MAX20360_REG_INTMASK3   0x0D   /* Interrupt mask group 3 */

/* ===== Current Limit & Charger Control (0x0F-0x19) ===== */
#define MAX20360_REG_ILIMCNTL   0x0F   /* Current limit control */
#define MAX20360_REG_CHGCNTL0   0x10   /* Charger control main */
#define MAX20360_REG_CHGCNTL1   0x11   /* Charger additional config */
#define MAX20360_REG_CHGTMR     0x12   /* Charger timers */
#define MAX20360_REG_STEPCHGCFG0 0x13  /* Step-charging config 0 */
#define MAX20360_REG_STEPCHGCFG1 0x14  /* Step-charging config 1 */
#define MAX20360_REG_THMCFG0    0x15   /* Thermal config 0 */
#define MAX20360_REG_THMCFG1    0x16   /* Thermal config 1 */
#define MAX20360_REG_THMCFG2    0x17   /* Thermal & harvester config */
#define MAX20360_REG_HRVCFG0    0x18   /* Harvester config 0 */
#define MAX20360_REG_HRVCFG1    0x19   /* Harvester config 1 */

/* ===== ADC/Monitor Configuration (0x1A) ===== */
#define MAX20360_REG_IVMONCFG   0x1A   /* ADC monitor mux control */

/* IVMONCFG register bit definitions */
#define MAX20360_IVMONCFG_CNTL_MASK       GENMASK(3, 0)  /* IVMONCntl[3:0]: Channel selector */
#define MAX20360_IVMONCFG_OFFHIZ          BIT(4)         /* IVMONOffHiZ: Hi-Z when disabled */
#define MAX20360_IVMONCFG_RATIO_MASK       GENMASK(6, 5)  /* IVMONRatioConfig[1:0]: Scaling ratio */
#define MAX20360_IVMONCFG_RATIO_SHIFT     5

/* IVMONCntl channel selector values */
#define MAX20360_IVMON_CNTL_DISABLED      0x00  /* IVMON multiplexer disabled */
#define MAX20360_IVMON_CNTL_ISET          0x01  /* Charger current (buffered VISET) */
#define MAX20360_IVMON_CNTL_BAT           0x02  /* Battery voltage (VBAT) */
#define MAX20360_IVMON_CNTL_SYS           0x03  /* System voltage (VSYS) */
#define MAX20360_IVMON_CNTL_BK1OUT        0x04  /* Buck1 output voltage */
#define MAX20360_IVMON_CNTL_BK2OUT        0x05  /* Buck2 output voltage */
#define MAX20360_IVMON_CNTL_BK3OUT        0x06  /* Buck3 output voltage */
#define MAX20360_IVMON_CNTL_L1OUT         0x07  /* LDO1 output voltage */
#define MAX20360_IVMON_CNTL_L2OUT         0x08  /* LDO2 output voltage */
#define MAX20360_IVMON_CNTL_SFOUT         0x09  /* SFOUT output voltage */
#define MAX20360_IVMON_CNTL_BBOUT         0x0A  /* Buck-Boost output voltage */

/* IVMONRatioConfig scaling ratio values (datasheet: 100% / 50% / 33.3% / 25%) */
#define MAX20360_IVMON_RATIO_1_1           0x00  /* 100%  (divide by 1) */
#define MAX20360_IVMON_RATIO_1_2           0x01  /* 50%   (divide by 2) */
#define MAX20360_IVMON_RATIO_1_3           0x02  /* 33.3% (divide by 3) */
#define MAX20360_IVMON_RATIO_1_4           0x03  /* 25%   (divide by 4) */

/* ===== Buck1 Regulator (0x1B-0x26) ===== */
#define MAX20360_REG_BUCK1ENA   0x1B   /* Buck-1 enable/sequence/mode */
#define MAX20360_REG_BUCK1CFG0  0x1C   /* Buck-1 config 0 */
#define MAX20360_REG_BUCK1CFG1  0x1D   /* Buck-1 config 1 */
#define MAX20360_REG_BUCK1ISET  0x1E   /* Buck-1 current setting */
#define MAX20360_REG_BUCK1VSET  0x1F   /* Buck-1 voltage setting */
#define MAX20360_REG_BUCK1CTR   0x20   /* Buck-1 MPC control */
#define MAX20360_REG_BUCK1DVSCFG0 0x21 /* Buck-1 DVS config 0 */
#define MAX20360_REG_BUCK1DVSCFG1 0x22 /* Buck-1 DVS config 1 */
#define MAX20360_REG_BUCK1DVSCFG2 0x23 /* Buck-1 DVS config 2 */
#define MAX20360_REG_BUCK1DVSCFG3 0x24 /* Buck-1 DVS config 3 */
#define MAX20360_REG_BUCK1DVSCFG4 0x25 /* Buck-1 DVS config 4 */
#define MAX20360_REG_BUCK1DVSSPI  0x26 /* Buck-1 DVS SPI */

/* ===== Buck2 Regulator (0x27-0x32) ===== */
#define MAX20360_REG_BUCK2ENA   0x27   /* Buck-2 enable/sequence/mode */
#define MAX20360_REG_BUCK2CFG0  0x28   /* Buck-2 config 0 */
#define MAX20360_REG_BUCK2CFG1  0x29   /* Buck-2 config 1 */
#define MAX20360_REG_BUCK2ISET  0x2A   /* Buck-2 current setting */
#define MAX20360_REG_BUCK2VSET  0x2B   /* Buck-2 voltage setting */
#define MAX20360_REG_BUCK2CTR   0x2C   /* Buck-2 MPC control */
#define MAX20360_REG_BUCK2DVSCFG0 0x2D /* Buck-2 DVS config 0 */
#define MAX20360_REG_BUCK2DVSCFG1 0x2E /* Buck-2 DVS config 1 */
#define MAX20360_REG_BUCK2DVSCFG2 0x2F /* Buck-2 DVS config 2 */
#define MAX20360_REG_BUCK2DVSCFG3 0x30 /* Buck-2 DVS config 3 */
#define MAX20360_REG_BUCK2DVSCFG4 0x31 /* Buck-2 DVS config 4 */
#define MAX20360_REG_BUCK2DVSSPI  0x32 /* Buck-2 DVS SPI */

/* ===== Buck3 Regulator (0x34-0x3F) ===== */
#define MAX20360_REG_BUCK3ENA   0x34   /* Buck-3 enable/sequence/mode */
#define MAX20360_REG_BUCK3CFG0  0x35   /* Buck-3 config 0 */
#define MAX20360_REG_BUCK3CFG1  0x36   /* Buck-3 config 1 */
#define MAX20360_REG_BUCK3ISET  0x37   /* Buck-3 current setting */
#define MAX20360_REG_BUCK3VSET  0x38   /* Buck-3 voltage setting */

/* Buck3CFG1 register bit definitions */
#define MAX20360_BUCK3CFG1_DISLDO  BIT(0)  /* Buck3DisLDO: 0=LDO assist enabled, 1=disabled */
#define MAX20360_REG_BUCK3CTR   0x39   /* Buck-3 MPC control */
#define MAX20360_REG_BUCK3DVSCFG0 0x3A /* Buck-3 DVS config 0 */
#define MAX20360_REG_BUCK3DVSCFG1 0x3B /* Buck-3 DVS config 1 */
#define MAX20360_REG_BUCK3DVSCFG2 0x3C /* Buck-3 DVS config 2 */
#define MAX20360_REG_BUCK3DVSCFG3 0x3D /* Buck-3 DVS config 3 */
#define MAX20360_REG_BUCK3DVSCFG4 0x3E /* Buck-3 DVS config 4 */
#define MAX20360_REG_BUCK3DVSSPI  0x3F /* Buck-3 DVS SPI */

/* ===== Buck-Boost Regulator (0x40-0x4B) ===== */
#define MAX20360_REG_BBSTENA    0x40   /* Buck-Boost enable/sequence/mode */
#define MAX20360_REG_BBSTCFG0   0x41   /* Buck-Boost config 0 */
#define MAX20360_REG_BBSTVSET   0x42   /* Buck-Boost voltage setting */
#define MAX20360_REG_BBSTISET   0x43   /* Buck-Boost current setting */
#define MAX20360_REG_BBSTCFG1   0x44   /* Buck-Boost config 1 */
#define MAX20360_REG_BBSTCTR0   0x45   /* Buck-Boost MPC control 0 */
#define MAX20360_REG_BBSTCTR1   0x46   /* Buck-Boost MPC control 1 */
#define MAX20360_REG_BBSTDVSCFG0 0x47  /* Buck-Boost DVS config 0 */
#define MAX20360_REG_BBSTDVSCFG1 0x48  /* Buck-Boost DVS config 1 */
#define MAX20360_REG_BBSTDVSCFG2 0x49  /* Buck-Boost DVS config 2 */
#define MAX20360_REG_BBSTDVSCFG3 0x4A  /* Buck-Boost DVS config 3 */
#define MAX20360_REG_BBSTDVSSPI  0x4B  /* Buck-Boost DVS SPI */

/* ===== ADC Registers - Haptic/ADC slave, NOT the PMIC slave =====
 * These registers live in the "Haptic Driver and ADC Registers" map at
 * I2C SlaveID 0xA0/0xA1 (7-bit 0x50).  The PMIC map (SlaveID 0x50/0x51,
 * 7-bit 0x28) has LDO1Ena/LDO1VSet at the same numeric offsets, so these
 * MUST be accessed through the haptic/ADC address, never the PMIC address.
 */
#define MAX20360_HPT_ADC_I2C_ADDR 0x50  /* 7-bit address of haptic/ADC block */

#define MAX20360_REG_ADCEN      0x50   /* ADC conversion launch (haptic/ADC map) */
#define MAX20360_REG_ADCCFG     0x51   /* ADC configuration (haptic/ADC map) */
#define MAX20360_REG_ADCDATAVG  0x53   /* ADC data averaged (read-only) */
#define MAX20360_REG_ADCDATMIN  0x54   /* ADC data minimum (read-only) */
#define MAX20360_REG_ADCDATMAX  0x55   /* ADC data maximum (read-only) */

/* ADCEN register bit definitions */
#define MAX20360_ADCEN_CONVLAUNCH        BIT(0)  /* Write 1 to launch, self-clears */

/* ADCCFG register bit definitions */
#define MAX20360_ADCCFG_SEL_MASK         GENMASK(2, 0)  /* ADCSel[2:0]: ADC channel selection */
#define MAX20360_ADCCFG_AVGSIZ_MASK      GENMASK(5, 3)  /* ADCAvgSiz[2:0]: Averaging size */
#define MAX20360_ADCCFG_AVGSIZ_SHIFT     3

/* ADCSel channel selection values */
#define MAX20360_ADC_SEL_VHDIN           0x00  /* VHDIN */
#define MAX20360_ADC_SEL_VIVMON          0x01  /* VIVMON (use IVMONRatioConfig = 00) */
#define MAX20360_ADC_SEL_RESERVED1        0x02  /* Reserved */
#define MAX20360_ADC_SEL_VCHGIN          0x03  /* VCHGIN */
#define MAX20360_ADC_SEL_VCPOUT          0x04  /* VCPOUT */
#define MAX20360_ADC_SEL_VBSTOUT         0x05  /* VBSTOUT */
#define MAX20360_ADC_SEL_RESERVED2      0x06  /* Reserved */
#define MAX20360_ADC_SEL_RESERVED3       0x07  /* Reserved */

/* ADCAvgSiz averaging size values (2^N measurements) */
#define MAX20360_ADC_AVGSIZ_1            0x00  /* No averaging (1 measurement) */
#define MAX20360_ADC_AVGSIZ_2            0x01  /* Average 2 measurements */
#define MAX20360_ADC_AVGSIZ_4            0x02  /* Average 4 measurements */
#define MAX20360_ADC_AVGSIZ_8            0x03  /* Average 8 measurements */
#define MAX20360_ADC_AVGSIZ_16           0x04  /* Average 16 measurements */
#define MAX20360_ADC_AVGSIZ_32           0x05  /* Average 32 measurements */
#define MAX20360_ADC_AVGSIZ_64           0x06  /* Average 64 measurements */
#define MAX20360_ADC_AVGSIZ_128          0x07  /* Average 128 measurements */

/* ADC conversion constants (datasheet Table 6)
 * VIVMON channel: V = (ADC[7:0] x 5.5V) / 255 with IVMONRatioConfig = 00.
 * If a divider ratio is used, multiply the result by the divider (2/3/4).
 */
#define MAX20360_ADC_FULL_SCALE          255   /* 8-bit ADC full scale value */
#define MAX20360_ADC_VIVMON_FS_MV        5500  /* VIVMON full scale: 5.5V (LSB 21.57mV) */

/* Charger current calculation constants (from datasheet) */
#define MAX20360_ISET_GAIN_KISET          2000  /* KISET gain factor (2000A/A) */
#define MAX20360_ISET_NOMINAL_RISET_OHM   1000  /* Nominal RISET resistor value (1kΩ typical) */

/* ===== LDO1 Regulator (0x51-0x54) ===== */
#define MAX20360_REG_LDO1ENA    0x51   /* LDO1 enable/sequence/mode */
#define MAX20360_REG_LDO1CFG    0x52   /* LDO1 config */
#define MAX20360_REG_LDO1VSET   0x53   /* LDO1 voltage setting */
#define MAX20360_REG_LDO1CTR    0x54   /* LDO1 MPC control */

/* ===== LDO2 Regulator (0x55-0x58) ===== */
#define MAX20360_REG_LDO2ENA    0x55   /* LDO2 enable/sequence/mode */
#define MAX20360_REG_LDO2CFG    0x56   /* LDO2 config */
#define MAX20360_REG_LDO2VSET   0x57   /* LDO2 voltage setting */
#define MAX20360_REG_LDO2CTR    0x58   /* LDO2 MPC control */

/* ===== Load Switch 1 (0x59-0x5B) ===== */
#define MAX20360_REG_LSW1ENA    0x59   /* LSW1 enable/sequence/mode */
#define MAX20360_REG_LSW1CFG    0x5A   /* LSW1 config */
#define MAX20360_REG_LSW1CTR    0x5B   /* LSW1 MPC control */

/* ===== Load Switch 2 (0x5C-0x5E) ===== */
#define MAX20360_REG_LSW2ENA    0x5C   /* LSW2 enable/sequence/mode */
#define MAX20360_REG_LSW2CFG    0x5D   /* LSW2 config */
#define MAX20360_REG_LSW2CTR    0x5E   /* LSW2 MPC control */

/* ===== Charge Pump (0x5F-0x61) ===== */
#define MAX20360_REG_CHGPMPENA  0x5F   /* Charge pump enable/sequence/mode */
#define MAX20360_REG_CHGPMPCFG  0x60   /* Charge pump config */
#define MAX20360_REG_CHGPMPCTR  0x61   /* Charge pump MPC control */

/* ===== Boost Regulator (0x62-0x66) ===== */
#define MAX20360_REG_BOOSTENA   0x62   /* Boost enable/sequence/mode */
#define MAX20360_REG_BOOSTCFG   0x63   /* Boost config */
#define MAX20360_REG_BOOSTISET  0x64   /* Boost current setting */
#define MAX20360_REG_BOOSTVSET  0x65   /* Boost voltage setting */
#define MAX20360_REG_BOOSTCTR   0x66   /* Boost MPC control */

/* ===== MPC (Multi-Purpose Control) Configuration (0x67-0x6F) ===== */
#define MAX20360_REG_MPC0CFG    0x67   /* MPC0 config */
#define MAX20360_REG_MPC1CFG    0x68   /* MPC1 config */
#define MAX20360_REG_MPC2CFG    0x69   /* MPC2 config */
#define MAX20360_REG_MPC3CFG    0x6A   /* MPC3 config */
#define MAX20360_REG_MPC4CFG    0x6B   /* MPC4 config */
#define MAX20360_REG_MPC5CFG    0x6C   /* MPC5 config */
#define MAX20360_REG_MPC6CFG    0x6D   /* MPC6 config */
#define MAX20360_REG_MPC7CFG    0x6E   /* MPC7 config */
#define MAX20360_REG_MPCITRSYS  0x6F   /* MPC interrupt/status */

/* ===== Dedicated Interrupt Configuration (0x70-0x75) ===== */
#define MAX20360_REG_BK1DEDINTCFG   0x70  /* Buck1 dedicated interrupt config */
#define MAX20360_REG_BK2DEDINTCFG   0x71  /* Buck2 dedicated interrupt config */
#define MAX20360_REG_BK3DEDINTCFG   0x72  /* Buck3 dedicated interrupt config */
#define MAX20360_REG_HPTDEDINTCFG   0x73  /* Haptic dedicated interrupt config */
#define MAX20360_REG_ADCDEDINTCFG   0x74  /* ADC dedicated interrupt config */
#define MAX20360_REG_USBOKDEDINTCFG 0x75  /* USB OK dedicated interrupt config */

/* ===== LED Current Sinks (0x78-0x7C) ===== */
#define MAX20360_REG_LEDCOMMON  0x78   /* LED common config */
#define MAX20360_REG_LED0REF    0x79   /* LED0 reference */
#define MAX20360_REG_LED0CTR    0x7A   /* LED0 control */
#define MAX20360_REG_LED1CTR    0x7B   /* LED1 control */
#define MAX20360_REG_LED2CTR    0x7C   /* LED2 control */

/* ===== Boot & PFN Status (0x7D-0x7E) ===== */
#define MAX20360_REG_PFN        0x7D   /* PFN pin status/config */
#define MAX20360_REG_BOOTCFG    0x7E   /* Boot configuration */

/* BootCfg (0x7E): PwrRstCfg[3:0] in bits 7:4 (OTP, read-only) */
#define MAX20360_BOOTCFG_PWR_RST_CFG_MASK  GENMASK(7, 4)
#define MAX20360_BOOTCFG_PWR_RST_CFG_SHIFT 4

/* PwrRstCfg modes where PFN2 is KOUT (open-drain mirror of KIN / SW2) */
#define MAX20360_PWR_RST_CFG_KIN          0x6  /* 0110 */
#define MAX20360_PWR_RST_CFG_CSR1         0x7  /* 0111 */
#define MAX20360_PWR_RST_CFG_KIN_OFF_SEAL 0xB  /* 1011 */

/* PFN register bit definitions (0x7D) */
/* PFN1 Configuration (bits 3:0) - Power button input by default */
#define MAX20360_PFN1_MODE_MASK      GENMASK(3, 0)
#define MAX20360_PFN1_MODE_PWRBTN    0x00  /* Power button (default) */
#define MAX20360_PFN1_MODE_BATLOW    0x01  /* Battery low output */
#define MAX20360_PFN1_MODE_CHGSTAT   0x02  /* Charger status output */
#define MAX20360_PFN1_MODE_GPIO_IN   0x03  /* General GPIO input */
#define MAX20360_PFN1_MODE_GPIO_OUT  0x04  /* General GPIO output */

/* PFN2 Configuration (bits 7:4) - Unused by default */
#define MAX20360_PFN2_MODE_MASK      GENMASK(7, 4)
#define MAX20360_PFN2_MODE_SHIFT     4
#define MAX20360_PFN2_MODE_DISABLED  (0x00 << 4)  /* Disabled (default) */
#define MAX20360_PFN2_MODE_BATLOW    (0x01 << 4)  /* Battery low output (active low) */
#define MAX20360_PFN2_MODE_CHGSTAT   (0x02 << 4)  /* Charger status output */
#define MAX20360_PFN2_MODE_USBDET    (0x03 << 4)  /* USB/CHGIN detected output */
#define MAX20360_PFN2_MODE_GPIO_IN   (0x04 << 4)  /* General GPIO input */
#define MAX20360_PFN2_MODE_GPIO_OUT  (0x05 << 4)  /* General GPIO output */

/* Battery low threshold register (for PFN battery-low mode) */
#define MAX20360_REG_BATLOWCFG  0x7D   /* Same register, different bits based on mode */
/* Battery low threshold: VBAT voltage where PFN pulls low */
/* Threshold = 2.8V + (value * 50mV), range 2.8V to 3.55V */
#define MAX20360_BATLOW_THRESH_2V8   0x00  /* 2.80V */
#define MAX20360_BATLOW_THRESH_3V0   0x04  /* 3.00V */
#define MAX20360_BATLOW_THRESH_3V2   0x08  /* 3.20V */
#define MAX20360_BATLOW_THRESH_3V4   0x0C  /* 3.40V - recommended for Li-ion */
#define MAX20360_BATLOW_THRESH_3V5   0x0F  /* 3.55V */

/* ===== Power Commands (0x7F-0x81) ===== */
#define MAX20360_REG_PWRCFG     0x7F   /* Power config */
#define MAX20360_REG_PWRCMD     0x80   /* Power command */
#define MAX20360_REG_BUCKCFG    0x81   /* Buck configuration */

/* PwrCmd (0x80) values — register auto-clears after accept */
#define MAX20360_PWR_OFF_CMD    0xB2   /* OFF mode */
#define MAX20360_PWR_SEAL_CMD   0xE5   /* SEAL / shipping (lowest Iq) */

/* ===== Lock/Security Registers (0x83-0x84) ===== */
#define MAX20360_REG_LOCKMASK   0x83   /* Lock mask (select functions to lock) */
#define MAX20360_REG_LOCKUNLOCK 0x84   /* Password to lock/unlock (0x55=unlock, 0xAA=lock) */

/* Lock/Unlock password values */
#define MAX20360_UNLOCK_PASSWORD 0x55  /* Password to unlock PMIC functions */
#define MAX20360_LOCK_PASSWORD   0xAA  /* Password to lock PMIC functions */

/* ===== SFOUT LDO (0x86-0x87) ===== */
#define MAX20360_REG_SFOUTCTR   0x86   /* SFOUT LDO voltage & enable */
#define MAX20360_REG_SFOUTMPC   0x87   /* SFOUT MPC control */

/* ===== OTP Registers (0x88-0x89) ===== */
#define MAX20360_REG_I2C_OTP_ADD 0x88  /* OTP address readback */
#define MAX20360_REG_I2C_OTP_DAT 0x89  /* OTP data readback (read-only) */

/* ===== Variant Step Size Definitions ===== */
/* Buck step sizes: 10mV, 25mV, or 50mV per variant */
#define MAX20360_BUCK_STEP_10MV  10000   /* 10mV step size */
#define MAX20360_BUCK_STEP_25MV  25000   /* 25mV step size */
#define MAX20360_BUCK_STEP_50MV  50000   /* 50mV step size */

/* Buck1 voltage ranges by step size */
#define MAX20360_BK1_MIN_UV      550000  /* 0.55V minimum */
#define MAX20360_BK1_MAX_UV_10MV 1200000 /* 1.20V max for 10mV step (extended for MAX20360F) */
#define MAX20360_BK1_MAX_UV_25MV 1950000 /* 1.95V max for 25mV step */
#define MAX20360_BK1_MAX_UV_50MV 3700000 /* 3.7V max for 50mV step */

/* Buck2 voltage ranges by step size */
#define MAX20360_BK2_MIN_UV      550000  /* 0.55V minimum */
#define MAX20360_BK2_MAX_UV_10MV 1180000 /* 1.18V max for 10mV step */
#define MAX20360_BK2_MAX_UV_25MV 2125000 /* 2.125V max for 25mV step */
#define MAX20360_BK2_MAX_UV_50MV 3700000 /* 3.7V max for 50mV step */

/* Buck3 voltage range (always 50mV step) */
#define MAX20360_BK3_MIN_UV      550000  /* 0.55V minimum */
#define MAX20360_BK3_MAX_UV      3700000 /* 3.7V maximum */

#endif /* ZEPHYR_DRIVERS_REGULATOR_MAX20360_REGS_H_ */
