# Copyright (c) 2025 AARMS Value Chain
# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_MAX302XX max302xx.c)
