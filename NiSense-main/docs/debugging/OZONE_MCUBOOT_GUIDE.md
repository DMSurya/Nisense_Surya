# Ozone Debugger Guide for MCUboot Environment

**Last Updated**: 2026-02-08  
**Platform**: nRF52840 (Raytac MDBT50Q-DB-40)  
**SDK**: nRF Connect SDK v3.3.0

## Overview

This guide explains how to use SEGGER Ozone debugger effectively with an MCUboot-based system where:
- **MCUboot** (bootloader) runs at `0x00000000 - 0x0000A000` (40 KB)
- **MCUboot pad (image header)** runs at `0x0000A000 - 0x0000A200` (0x200 bytes)
- **Application** runs at `0x0000A200 - 0x000FA000` (mcuboot_primary app payload, 960 KB total for pad+app)
- **Internal NVS** lives at `0x000FA000 - 0x00100000` (24 KB)

## Quick Start

### 1. Launch Ozone
```
File → Open Project → E:\AARMS\HCM\NiSense\scripts\debug\ozone\Ozone.jdebug
```

### 2. Flash & Debug
| Action | Shortcut | Description |
|--------|----------|-------------|
| Download & Reset | **F4** | Flash program and reset to start |
| Start & Break | **F5** | Run until main() or breakpoint |
| Step Over | **F6** | Execute current line |
| Step Into | **F7** | Step into function |
| Step Out | **F8** | Return from function |
| Toggle Breakpoint | **F9** | Set/clear breakpoint |
| Reset | **F12** | Hardware reset |

### 3. RTT Console
```
Terminal → New Terminal → RTT Channel 0
```

---

## Understanding the Memory Map

### MCUboot + Application Layout
```
Internal Flash (1 MB):
+-------------------------------------------------------------+
+ 0x00000000 + MCUboot Bootloader        + 40 KB             +
+-------------------------------------------------------------+
+ 0x0000A000 + MCUboot Pad (header)      + 512 bytes (0x200) +
+-------------------------------------------------------------+
+ 0x0000A200 + Application Code          + ~958 KB payload   +
+-------------------------------------------------------------+
+ 0x000FA000 + NVS Storage               + 24 KB             +
+-------------------------------------------------------------+

External QSPI Flash (8 MB) — see PARTITION_LAYOUT.md for DTS truth:
+-------------------------------------------------------------+
+ 0x00000000 + Slot1 OTA staging         + 968 KB            +
+-------------------------------------------------------------+
+ 0x000F2000 + MCUboot Scratch           + 16 KB             +
+-------------------------------------------------------------+
+ 0x000F6000 + Resource store (pri/sec)      + 1 MB              +
+-------------------------------------------------------------+
+ 0x001F6000 + Calibration (RO)          + 64 KB             +
+-------------------------------------------------------------+
+ 0x00206000 + External NVS              + 128 KB            +
+-------------------------------------------------------------+
+ 0x00226000 + Glucose model A/B (XIP)   + 512 KB            +
+-------------------------------------------------------------+
+ 0x002A6000 + record_store              + ~5.35 MB          +
+-------------------------------------------------------------+
```

No FAT userdata / USB MSC region — removed.

### Address Interpretation
- **0x00000000 - 0x0000A000**: MCUboot code
- **0x0000A000 - 0x0000A200**: MCUboot image header (magic, size, version)
- **0x0000A200+**: Application code (vectors start here)
- **0x20000000+**: RAM (shared between MCUboot and App)

---

## Loading Symbols for MCUboot vs Application

### Problem: Two ELF Files
Since MCUboot and the application are separate programs, Ozone can only load one ELF at a time. You need to switch based on what you're debugging.

### Solution 1: Debug MCUboot Only
```
File → Open → E:\AARMS\HCM\NiSense\<BuildDirName>\mcuboot\zephyr\zephyr.elf
```
- Use when debugging boot failures
- Symbols valid for addresses 0x00000000 - 0x0000A000

### Solution 2: Debug Application Only (Default)
```
File → Open → E:\AARMS\HCM\NiSense\<BuildDirName>\NiSense\zephyr\zephyr.elf
```
- Use for normal application debugging
- Symbols valid for addresses 0x0000A200+

### Solution 3: Switch ELF During Debug
1. Pause execution (F5 to halt)
2. `File → Load → Load ELF File`
3. Select the appropriate ELF
4. Continue debugging

---

## Debugging MCUboot Boot Process

### Key MCUboot Functions
| Function | Address | Purpose |
|----------|---------|---------|
| `main` | ~0x03D4 | MCUboot entry point |
| `boot_go` | ~0x0E38 | Start boot process |
| `context_boot_go` | ~0x0AC0 | Main boot logic |
| `boot_read_sectors` | ~0x6380 | Read partition sectors |
| `cleanup_arm_nvic` | ~0x11D0 | Prepare for app jump |
| `arch_system_halt` | ~0x8764 | Fatal error handler |

### Breakpoint Strategy for Boot Issues
```c
// Set breakpoints at key locations:
SetBP 0x03D4   // MCUboot main()
SetBP 0x0E38   // boot_go() entry
SetBP 0x0AC0   // context_boot_go() entry
SetBP 0x0BDE   // context_boot_go() error exit
SetBP 0x11D0   // cleanup_arm_nvic() - about to jump to app
SetBP 0x8764   // Fatal error handler
```

### Watching boot_go Return Value
```
1. Set breakpoint after boot_go() call in main()
2. Run to breakpoint
3. Check R0 register:
   - R0 = 0: Success, will jump to app
   - R0 = -1 (0xFFFFFFFF): Failure, boot aborted
```

---

## Debugging Application After MCUboot

### Method 1: Reset and Run
```
1. F4 (Download & Reset)
2. F5 (Run)
3. Wait ~2-3 seconds for MCUboot to validate and jump
4. F5 (Break) - should now be in application
```

### Method 2: Set Breakpoint in App
```
1. Load Application ELF
2. Set breakpoint at main() or desired function
3. F4 (Download & Reset)
4. MCUboot will run, then break at your app breakpoint
```

### Method 3: Skip MCUboot (Direct App Debug)
If MCUboot is working correctly and you want to skip it:
```
1. In Ozone: Debug → Start from Address → 0x0000A200
2. This bypasses MCUboot but may cause issues if app
   expects certain MCUboot-initialized state
```

---

## RTT (Real-Time Transfer) Setup

### RTT Buffer Location
RTT control block is auto-detected in SRAM (0x20000000 - 0x20040000).

### Opening RTT Terminal
```
Terminal → New Terminal → RTT Channel 0 (Logs)
```

Interactive shell is **USB CDC** (`nisense rec …`) when `APP_FEATURE_SHELL=y`,
not RTT. SystemView may use
RTT Channel 1 when enabled — see [SYSTEMVIEW.md](SYSTEMVIEW.md).

### RTT Troubleshooting
If RTT doesn't show output:
1. Verify target is running (not halted)
2. Check RTT buffer detection: `View → Terminal I/O → Show RTT Control Block`
3. Manually set RTT address if needed (rare)

---

## Fault Analysis

### Exception Numbers (IPSR Register)
| IPSR | Exception | Cause |
|------|-----------|-------|
| 0 | None | Normal execution |
| 2 | NMI | Non-maskable interrupt |
| 3 | HardFault | General fault |
| 4 | MemManage | MPU violation or invalid memory access |
| 5 | BusFault | Bus error (invalid address) |
| 6 | UsageFault | Undefined instruction, alignment |
| 11+ | SVCall, PendSV, SysTick, IRQs | System/peripheral interrupts |

### Fault Status Registers
```
View → Registers → Special Function Registers → Fault
```

| Register | Address | Purpose |
|----------|---------|---------|
| CFSR | 0xE000ED28 | Combined Fault Status |
| MMFAR | 0xE000ED34 | MemManage Fault Address |
| BFAR | 0xE000ED38 | BusFault Address |
| HFSR | 0xE000ED2C | HardFault Status |

### Reading Fault Registers in Ozone
```
// In Console or Watch window:
*(uint32_t*)0xE000ED28   // CFSR
*(uint32_t*)0xE000ED34   // MMFAR
```

### Decoding Exception Stack Frame
When a fault occurs, ARM pushes registers to the stack. Read from SP:
```
SP+0x00: R0
SP+0x04: R1
SP+0x08: R2
SP+0x0C: R3
SP+0x10: R12
SP+0x14: LR (return address before fault)
SP+0x18: PC (faulting instruction)
SP+0x1C: xPSR
```

---

## Common MCUboot Debug Scenarios

### Scenario 1: MCUboot Doesn't Start
**Symptoms**: PC stuck at reset vector or early startup
```
1. Check vector table at 0x00000000
2. Verify SP (first word) points to valid RAM
3. Verify Reset vector (second word) points to valid code
```

### Scenario 2: MCUboot Fails to Validate Image
**Symptoms**: boot_go() returns -1
```
1. Set breakpoint at context_boot_go() (0x0AC0)
2. Step through to find which check fails:
   - flash_area_open() - partition access
   - boot_read_sectors() - sector enumeration
   - boot_image_check() - signature verification
```

### Scenario 3: MemManage Fault During Boot
**Symptoms**: IPSR = 4, PC in fatal handler
```
1. Check CONFIG_MPU_STACK_GUARD - may cause false faults
2. Read CFSR to determine fault type
3. Read MMFAR for fault address
4. Check stack sizes (CONFIG_MAIN_STACK_SIZE)
```

### Scenario 4: App Doesn't Start After MCUboot
**Symptoms**: MCUboot completes but app crashes immediately
```
1. Verify app vector table at 0x0000A200
2. Check app's SP and Reset vector are valid
3. Verify FLASH_LOAD_OFFSET matches actual load address
4. Check for RAM overlap between MCUboot and app
```

---

## Ozone Project Configuration

### Current Project File: `scripts/debug/ozone/Ozone.jdebug`
```javascript
// Key settings in OnProjectLoad():
Project.SetDevice("nRF52840_xxAA");
Project.SetTargetIF("SWD");
Project.SetTIFSpeed("4 MHz");
Project.SetOSPlugin("ZephyrPlugin.js");  // Zephyr thread awareness

// ELF file (choose based on what you're debugging):
File.Open("$(ProjectDir)/<BuildDirName>/NiSense/zephyr/zephyr.elf");
// Or for MCUboot:
// File.Open("$(ProjectDir)/<BuildDirName>/mcuboot/zephyr/zephyr.elf");
```

### Useful Ozone Commands (Console)
```
// Memory read
Debug.Read32(0x0000A200)   // Read vector table
Debug.Read32(0xE000ED28)   // Read CFSR

// Set/clear breakpoints
Debug.SetBreakpoint(0x03D4)   // MCUboot main
Debug.ClearAllBreakpoints()

// Reset and run
Target.Reset()
Target.Go()
Target.Halt()
```

---

## Best Practices

### 1. Always Use merged.hex for Flashing
The merged.hex contains both MCUboot and the signed application:
```
<BuildDirName>/merged.hex
```

### 2. Keep Both ELF Files Accessible
- MCUboot: `<BuildDirName>/mcuboot/zephyr/zephyr.elf`
- Application: `<BuildDirName>/NiSense/zephyr/zephyr.elf`

### 3. Use addr2line for Quick Symbol Lookup
```powershell
arm-zephyr-eabi-addr2line.exe -e <elf_file> -f -C <address>
```

### 4. Watch the IPSR Register
Always check IPSR after a crash to identify exception type.

### 5. Save Debug Sessions
`File → Save Session` to preserve breakpoints and window layout.

---

## Troubleshooting Ozone

### "Script interpretation failure" Error
If you see errors about unknown functions like `Project.SetRTTSearchRanges`:
- Edit `scripts/debug/ozone/Ozone.jdebug` and remove/comment the offending line
- RTT is auto-configured in modern Ozone versions

### "No Debug Unit Found"
- Check USB connection to J-Link
- Verify target power (VTref should show ~3.3V)
- Try `Target → Reconnect`

### Symbols Don't Match Code
- Ensure you've loaded the correct ELF file
- Rebuild and reflash if code changed
- Check that FLASH_LOAD_OFFSET matches

---

## Quick Reference Card

| Task | Command/Location |
|------|-----------------|
| Flash + Reset | F4 |
| Run | F5 |
| Step Over | F6 |
| Set Breakpoint | F9 or click line margin |
| View Registers | View → Registers |
| View Call Stack | View → Call Stack |
| View Threads | View → Threads (with ZephyrPlugin) |
| RTT Terminal | Terminal → New Terminal → RTT |
| Memory Window | View → Memory |
| Watch Variable | View → Watched Data → Add |
| Console | View → Console |


