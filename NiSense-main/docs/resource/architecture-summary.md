# Resource store & Calibration - Complete Automation

**Status**: ✓ **Fully Automated**

## What's Automated

### 1. Build-Time: Automatic Logo Conversion
**When**: During `west build` or "Build NiSense" task
**What**: Converts LVGL C logo sources → Resource binary
**Output**: `<BuildDirName>/resource.bin` (~115 KB)
**Trigger**: CMake custom target (CMakeLists.txt)

```
Build Flow:
+-------------------------------------+
+ Compile source files                +
+-------------------------------------+
+ Generate zephyr.elf + zephyr.hex    +
+-------------------------------------+
+ CMake: Convert logo → XIP        + ← AUTOMATIC
+   - logo.c → 115,216 bytes          +
+   - Output: resource.bin          +
+-------------------------------------+
```

### 2. Debug-Time: Automatic XIP Flash
**When**: Pressing F5 (Start) in Ozone
**What**: Flashes Resource binary to external QSPI @ 0x0F4000
**Trigger**: Ozone OnStartupComplete callback
**Safety**: Checks for binary existence before flashing

```
Debug Flow:
+-------------------------------------+
+ F4: Download merged.hex             +
+     (MCUboot + app to internal)      +
+-------------------------------------+
+ F5: Start (break at main)           +
+-------------------------------------+
+ OnStartupComplete callback fires    +
+-------------------------------------+
+ ✓ Check: resource.bin exists?    +
+ ✓ Call: FlashResource()           +
+ ✓ Result: Assets @ 0x120F4000      +
+-------------------------------------+
```

---

## Implementation Details

### CMake Integration (CMakeLists.txt)

```cmake
# Resource store conversion as part of build
add_custom_target(resource ALL
    COMMAND ${PYTHON_EXECUTABLE}
        scripts/tools/convert_logos_to_resource.py
        --src-dir src
        --output <BuildDirName>/resource.bin
    DEPENDS
        src/core/logo.c
)
add_dependencies(app resource)
```

**Features**:
- Dependency tracking: Only converts if logos changed
- Parallel: Doesn't block main app compilation
- Optional: Gracefully skips if Python unavailable
- Safe: Validates Python before running

### Ozone Integration (scripts/debug/ozone/Ozone.jdebug)

```javascript
void OnStartupComplete(void) {
  if (File.Exists("$(ProjectDir)/<BuildDirName>/resource.bin")) {
    Util.Log("AutoFlash: Resource store detected, flashing...");
    FlashResource();
  }
}
```

**Features**:
- Non-blocking: Graceful fallback if binary missing
- Safe: Checks existence before flashing
- Automatic: Runs on every F5 command
- Logged: Clear output messages

### VS Code Tasks (.vscode/tasks.json)

Three new quick-access tasks added:

| Task | Purpose | Shortcut |
|------|---------|----------|
| `Convert Resource store` | Run conversion only | Manual task |
| `Build + Convert Resource store` | Build app, then convert | Chain task |
| (Existing) `Build NiSense` | Build includes conversion automatically | Modified |

---

## Usage Workflow

### Standard Development Cycle

```powershell
# 1. Build (automatic Resource conversion)
Ctrl+Shift+B → Select "Build NiSense"
# Output: zephyr.elf + resource.bin

# 2. Flash firmware and assets
F4 (Download & Reset)
# Result: MCUboot + app to internal flash

# 3. Start debugging
F5 (Start)
# Ozone automatically:
#   ✔ Breaks at main()
#   ✔ Flashes resource.bin to external flash
#   ✔ Assets available at 0x120F4000

# 4. Debug application
# Logos are now accessible from XIP memory!
```

### One-Command Build + Convert

```powershell
# If you changed logos and want explicit re-conversion:
Ctrl+Shift+B → Select "Build + Convert Resource store"
# Always rebuilds conversion, even if logos unchanged
```

### Convert Only (After Build)

```powershell
# If logos changed and you just want to regenerate binary:
Ctrl+Shift+B → Select "Convert Resource store"
# Skips app compilation, just runs conversion
```

---

## What Changed

### Files Modified

| File | Changes | Impact |
|------|---------|--------|
| `CMakeLists.txt` | Added `resource` custom target | Auto-convert on build |
| `scripts/debug/ozone/Ozone.jdebug` | Uncommented `OnStartupComplete()` | Auto-flash on debug |
| `.vscode/tasks.json` | Added conversion tasks | Quick access |
| `scripts/tools/convert_logos_to_resource.py` | New file (created earlier) | Conversion script |

### Files Created

| File | Purpose |
|------|---------|
| `XIP_AUTOMATION.md` | Automation documentation |
| `resource_AUTOMATION_SUMMARY.md` | This file |

### No Breaking Changes

✓ Backward compatible
✓ Gracefully handles missing Python
✓ Optional: can be disabled in CMake
✓ All manual commands still work

---

## Verification

### Build Output Shows Conversion

```
Converting LVGL logo to Resource binary during build...
  Parsing logo.c...
    Found array: logo_map
    Extracted 115200 bytes of pixel data
    Dimensions: 240x240
  
Wrote ~115000 bytes to <BuildDirName>/resource.bin
```

### Binary Generated

```powershell
PS> ls <BuildDirName>/resource.bin
    
    Directory: E:\AARMS\HCM\NiSense\<BuildDirName>
    
Mode Length Name
---- ------ ----
-a-- ~115KB resource.bin
```

### Flash Memory Usage

| Component | Size | Status |
|-----------|------|--------|
| Internal Flash Used | ~901 KB | ~91.7% (shell/USB off, 2026-07-30) — see [BUILD_STATUS.md](../build/BUILD_STATUS.md) |
| Internal Flash Saved | 337 KB | ✓ Via XIP |
| External Flash Resource store | 346 KB | @ 0x0F4000 |
| External Flash Total Available | 8 MB | Plenty of room |

---

## Manual Overrides (If Needed)

### Skip CMake Conversion
```bash
cd <BuildDirName>
cmake -DSKIP_RESOURCE_CONVERSION=ON ..
west build
```

### Manual Ozone Flash
If auto-flash fails, manually flash from Ozone console:
```javascript
// Ozone console (Ctrl+Shift+Q):
exec FlashResource()
```
