/**
 * @file app_data.h
 * @brief App-owned QSPI region (hub firmware blobs, etc.)
 *
 * Partition label "app" @ 0x726000, size 0xDA000 (~872 KB) — enough for
 * MAX32664 .msbl (~300 KB) with headroom. See 51_qspi.overlayinc /
 * PARTITION_LAYOUT.md. Records take a rounded 4 MB ahead of this region.
 */

#ifndef APP_DATA_H_
#define APP_DATA_H_

#define APP_DATA_XIP_BASE           0x12000000U
#define APP_DATA_PARTITION_OFFSET   0x726000U
#define APP_DATA_PARTITION_SIZE     0xDA000U   /* ~872 KB */
#define APP_DATA_RESOURCE_MMAP_ADDR \
	(APP_DATA_XIP_BASE + APP_DATA_PARTITION_OFFSET)

#endif /* APP_DATA_H_ */
