<#
.SYNOPSIS
    Quick reference for debugging commands - nRF52840 Wearable Health Monitor

.DESCRIPTION
    Displays available debugging tools, VS Code tasks, and Ozone commands.
#>

Write-Host @"

╔═══════════════════════════════════════════════════════════════════════════════════╗
║              nRF52840 Wearable Health Monitor - Debug Quick Reference             ║
╚═══════════════════════════════════════════════════════════════════════════════════╝

┌─────────────────────────────────────────────────────────────────────────────────┐
│ VS CODE TASKS (Ctrl+Shift+P → Tasks: Run Task)                                  │
├─────────────────────────────────────────────────────────────────────────────────┤
│ Build NiSense          - Build the project                                   │
│ Build NiSense (Clean)  - Clean build                                         │
│ Flash Program Only        - Flash merged.hex via J-Link                         │
│ RTT Monitor               - Monitor RTT output (continuous)                     │
│ RTT Monitor (Reset + Log) - Reset device, monitor, and log to file              │
│ Build + Flash + RTT       - Full workflow: build, flash, and monitor            │
│ Launch Ozone              - Open Ozone debugger                                 │
└─────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────┐
│ POWERSHELL SCRIPTS (.vscode folder)                                             │
├─────────────────────────────────────────────────────────────────────────────────┤
│ .\rtt_monitor.ps1                   - Continuous RTT monitoring                 │
│ .\rtt_monitor.ps1 -Reset            - Reset device then monitor                 │
│ .\rtt_monitor.ps1 -LogFile log.txt  - Save output to file                       │
│ .\rtt_monitor.ps1 -Duration 60      - Monitor for 60 seconds                    │
│ .\flash_jlink.ps1                   - Flash via J-Link                          │
│ .\flash_nrfjprog.ps1                - Flash via nrfjprog                        │
│ .\launch_ozone.ps1                  - Open Ozone debugger                       │
│ .\kill_debug_processes.ps1          - Kill all J-Link processes                 │
└─────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────┐
│ OZONE DEBUGGER                                                                  │
├─────────────────────────────────────────────────────────────────────────────────┤
│ Hotkeys:                                                                        │
│   F4  - Download & Reset (flash merged.hex, reset to MCUboot)                   │
│   F5  - Start & Break (run until main)                                          │
│   F6  - Step Over                                                               │
│   F7  - Step Into                                                               │
│   F8  - Step Out                                                                │
│   F9  - Toggle Breakpoint                                                       │
│   F12 - Reset                                                                   │
│                                                                                 │
│ RTT Terminal:                                                                   │
│   Terminal → New Terminal → RTT Channel 0                                       │
│                                                                                 │
│ Console Commands (View → Console, then type):                                   │
│   exec ShowMemoryMap()    - Display memory layout                               │
│   exec CheckBootStatus()  - Show MCUboot/App boot status                        │
│   exec BootDirectToApp()  - Skip MCUboot, boot directly to app                  │
└─────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────┐
│ MEMORY MAP                                                                      │
├─────────────────────────────────────────────────────────────────────────────────┤
│ Internal Flash (1 MB):                                                          │
│   0x00000000 - 0x0000A000  MCUboot bootloader (40 KB)                           │
│   0x0000A000 - 0x0000A200  MCUboot image header (512 B)                         │
│   0x0000A200 - 0x000FA000  Application (~960 KB)                                │
│   0x000FA000 - 0x00100000  NVS storage (24 KB)                                  │
│                                                                                 │
│ External QSPI Flash (8 MB):                                                     │
│   0x00000000 - 0x000F0000  Slot1 OTA staging (960 KB)                           │
│   0x000F0000 - 0x000F1000  MCUboot scratch (4 KB)                               │
│   0x00221000 - 0x007A1000  FAT32 user data (5.5 MB)                             │
│                                                                                 │
│ SRAM (256 KB):                                                                  │
│   0x20000000 - 0x20040000  RAM                                                  │
└─────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────┐
│ COMMON ISSUES                                                                   │
├─────────────────────────────────────────────────────────────────────────────────┤
│ "Cannot connect to J-Link"                                                      │
│   → Run: .\kill_debug_processes.ps1 to release the debugger                     │
│   → Check USB cable and J-Link connection                                       │
│                                                                                 │
│ "No RTT output"                                                                 │
│   → Ensure device is running (not halted)                                       │
│   → Reset device: press reset button or use -Reset flag                         │
│   → Check RTT buffer address in SRAM (0x20000000)                               │
│                                                                                 │
│ "BusFault in MCUboot"                                                           │
│   → This is normal during single-step debugging                                 │
│   → Use F5 to run past the boot sequence                                        │
│   → Set breakpoint at main() to stop in application code                        │
│                                                                                 │
│ "App doesn't start on power cycle"                                              │
│   → Verify CONFIG_BOOT_BOOTSTRAP=y in sysbuild/mcuboot/prj.conf                 │
│   → Rebuild and flash: west build --pristine                                    │
└─────────────────────────────────────────────────────────────────────────────────┘

"@ -ForegroundColor Cyan

