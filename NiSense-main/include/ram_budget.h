/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Link-time RAM budget helpers — libc malloc tail after BSS.
 */
#ifndef RAM_BUDGET_H_
#define RAM_BUDGET_H_

#ifdef __cplusplus
extern "C" {
#endif

/** Log _end vs SRAM limit and warn when dynamic libc malloc arena is tiny. */
void ram_budget_log_linker_tail(void);

/** Same metrics via printk (for diag_monitor dumps). */
void ram_budget_print_linker_tail(void);

#ifdef __cplusplus
}
#endif

#endif /* RAM_BUDGET_H_ */
