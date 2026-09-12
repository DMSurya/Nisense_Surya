# Resource store Automation Guide

Complete automation of Resource store management during build and debug workflows.

## Overview

The Resource store conversion and flashing process is now fully automated:

1. **Build-time Automation**: Logos are automatically converted to Resource binary during CMake build
2. **Debug-time Automation**: Resource store are automatically flashed when launching Ozone

## Build-Time Automation (CMake)

### How It Works

When you run the build, CMake automatically:
1. Checks for the Python conversion script
2. Monitors logo source file (logo.c)
3. Runs conversion if logo changes
4. Outputs `<BuildDirName>/resource.bin`

### Build Commands

**Option 1: Standard build (automatic Resource conversion)**
```bash
# VS Code task: "Build NiSense"
# Automatically runs Resource conversion as part of build
```

**Option 2: Build + manual conversion (if needed)**
```bash
# VS Code task: "Build + Convert Resource store"
# Builds app, then re-converts logos
```

**Option 3: Convert only (after build)**
```bash
# VS Code task: "Convert Resource store"
# Just run conversion without rebuilding app
```

### CMake Configuration

The automation is controlled by `CMakeLists.txt`:

```cmake
# Automatically convert LVGL logos to Resource binary during build
if(PYTHON_EXECUTABLE)
    set(_XIP_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/../resource.bin")
    
    add_custom_target(resource ALL
        COMMAND ${PYTHON_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/tools/convert_logos_to_resource.py"
            --src-dir "${CMAKE_CURRENT_SOURCE_DIR}/src"
            --output "${_XIP_OUTPUT}"
            --build-id "${PROJECT_VERSION}"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/core/logo.c"
    )
    
    # Make the main app depend on Resource store being built first
    add_dependencies(app resource)
endif()
```

**Key Features:**
- Only runs if Python is available
- Dependency tracking: rebuild only if logos changed
- Parallel build: doesn't block main app compilation
- Output path: `<BuildDirName>/resource.bin`
- Build ID: automatically tagged with project version

## Debug-Time Automation (Ozone)

### How It Works

When you launch Ozone and hit F5 (Start), the debugger automatically:
1. Checks if `resource.bin` exists
2. If found, calls `FlashResource()` after startup
3. Flashes to external QSPI flash @ 0x0F4000
4. Makes assets available at XIP address 0x120F4000

### Ozone Configuration

The automation is controlled by `scripts/debug/ozone/Ozone.jdebug`:

```javascript
void OnStartupComplete (void) {
  int file_exists;
  
  // Check if Resource store binary exists before attempting to flash
  file_exists = File.Exists("$(ProjectDir)/<BuildDirName>/resource.bin");
  
  if (file_exists) {
    Util.Log("AutoFlash: Resource store detected, flashing...");
    FlashResource();
  } else {
    Util.Log("AutoFlash: resource.bin not found - run build first");
  }
}
```

**Key Features:**
- Safe: checks for binary existence first
- Non-blocking: detects missing binary gracefully
- Automatic: runs on every F5 / "Start" command
- Optional: only if binary exists

### Workflow

1. **Build**: `Ctrl+Shift+B` → Select "Build NiSense"
   - App compiles
   - CMake auto-converts logos → `resource.bin`

2. **Debug**: F4 (Download & Reset)
   - Flashes merged.hex (MCUboot + app)
   - Resets to MCUboot bootloader

3. **Run**: F5 (Start)
   - App launches
   - Ozone detects `resource.bin`
   - Automatically flashes Resource store
   - Assets ready to use

4. **View Output**: 
   - Terminal > New Terminal > RTT Channel 0
   - See XIP flash logs in RTT output

## Manual Control

You can still manually control both processes:

### Manual Build (no Resource conversion)
```bash
# If you want to build app only, disable the custom target
cd <BuildDirName>
cmake -DSKIP_RESOURCE_CONVERSION=ON ..
west build
```

### Manual Ozone Flash
If auto-flash has issues, manually call from Ozone console:
```javascript
// In Ozone console (Ctrl+Shift+Q):
exec FlashResource()          // Flash Resource store
exec VerifyResource()          // Verify asset integrity
exec FlashCalibration()         // Flash calibration (production only)
```

## Troubleshooting

### "resource.bin not found" in Ozone

**Cause**: Build hasn't run or Python conversion failed

**Fix**:
1. Run "Build NiSense" task
2. Check build output for conversion errors
3. Verify Python is installed: `python --version`

### Build fails with "Python not found"

**Cause**: Python is not in PATH

**Fix**:
```powershell
# Windows: Add Python to PATH
$env:PATH += ";C:\Python311"

# Or use full path in CMakeLists.txt:
set(PYTHON_EXECUTABLE "C:/Python311/python.exe")
```
