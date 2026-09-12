# =============================================================================
# J-Link Command Reference for nRF52840
# =============================================================================
# Usage: JLink.exe -device nRF52840_xxAA -if swd -speed 4000 -CommandFile <file>
#
# This file documents J-Link commands. Use individual .jlink files for specific
# operations or embed commands directly in PowerShell scripts.
# =============================================================================

# -----------------------------------------------------------------------------
# BASIC COMMANDS
# -----------------------------------------------------------------------------
# r         - Reset and halt target
# g         - Go (run/resume)
# h         - Halt target
# q/exit    - Quit J-Link Commander
# qc        - Quit without confirmation

# -----------------------------------------------------------------------------
# MEMORY OPERATIONS
# -----------------------------------------------------------------------------
# mem8  <addr> <count>    - Read bytes
# mem16 <addr> <count>    - Read halfwords  
# mem32 <addr> <count>    - Read words
# w1 <addr> <data>        - Write byte
# w2 <addr> <data>        - Write halfword
# w4 <addr> <data>        - Write word

# -----------------------------------------------------------------------------
# FLASH OPERATIONS  
# -----------------------------------------------------------------------------
# erase                   - Erase entire chip
# loadfile <hex>          - Load hex/bin file to flash
# verifyfile <hex>        - Verify flash contents
# savebin <file> <addr> <size>  - Save memory to binary file

# -----------------------------------------------------------------------------
# RTT OPERATIONS
# -----------------------------------------------------------------------------
# SetRTTAddr <addr>       - Set RTT control block address
# SetRTTSearchRanges <start> <size>  - Search for RTT control block

# -----------------------------------------------------------------------------
# REGISTER READ COMMANDS (ARM Cortex-M4)
# -----------------------------------------------------------------------------
# regs                    - Show all core registers

# Fault Status Registers:
# mem32 0xE000ED08 1      - VTOR (Vector Table Offset)
# mem32 0xE000ED28 1      - CFSR (Configurable Fault Status)
# mem32 0xE000ED2C 1      - HFSR (HardFault Status)  
# mem32 0xE000ED30 1      - DFSR (Debug Fault Status)
# mem32 0xE000ED34 1      - MMFAR (MemManage Fault Address)
# mem32 0xE000ED38 1      - BFAR (BusFault Address)
# mem32 0xE000ED3C 1      - AFSR (Auxiliary Fault Status)

# Stack Pointer:
# mem32 0xE000ED08 1      - Get VTOR, then read SP from [VTOR]

