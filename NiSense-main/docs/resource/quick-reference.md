# Resource store Quick Reference

## 99% of the Time

```
Ctrl+Shift+B  →  Select "Build NiSense"
                 →
                 ✔ App compiled
                 ✔ Resource binary generated
                 →
F4             →  Download & Reset
                 →
                 ✔ Firmware flashed to device
                 →
F5             →  Start
                 →
                 ✔ App running
                 ✔ Resource store auto-flashed
                 ✔ Ready to debug!
```

---

## The Three Build Tasks

**If you need to rebuild logos after editing:**

```
Ctrl+Shift+B

+- Build NiSense                    ← DEFAULT (fastest)
+  • Builds app
+  • Resource conversion runs if needed
+  • CMake dependency tracking
+
+- Build + Convert Resource store       ← If you edited logos
+  • Builds app
+  • Force re-convert logos
+  • Always regenerates binary
+
+- Convert Resource store            ← Just conversion
   • Skip app build
   • Only convert logos
   • Good for testing conversion
```

---

## What Happens Automatically

### During Build
```
West Build
    → CMake runs
    → Check if py available
    → Compare logo files
    → If changed: run convert_logos_to_resource.py
    → Output: <BuildDirName>/resource.bin
    → Continue building app
```

### During Debug (F5)
```
Ozone Start
    → App breaks at main()
    → OnStartupComplete callback
    → Check: resource.bin exists?
    → YES: call FlashResource()
    → Flash to QSPI @ 0x0F4000
    → Assets live at 0x120F4000
```

---

## Troubleshooting (In Order)

### "Build fails: Python not found"
```powershell
# Check Python is installed:
python --version

# If not installed, download from:
# https://www.python.org/downloads/

# Or use Windows Store:
# winget install Python.Python.3.11
```

### "Ozone says: resource.bin not found"
```
1. Run "Build NiSense" task (`<BuildDirName>/resource.bin` will be created)
2. Check build output shows "Wrote 346000 bytes to resource.bin"
3. If not shown, re-run build (Ctrl+Shift+B)
```

### "Resource store flashed but can't read (0xFF everywhere)"
```
1. Check Ozone > Project Settings > J-Link
2. Ensure QSPI flash found: "MX25R6435F" should appear
3. May need to manually flash first time via:
   - J-Flash / J-Flash Lite (nRF5x plugin)
   - nrfjprog (if installed)
```

### "Ozone hangs on 'AutoFlash: Resource store...'"
```
Press Ctrl+C in Ozone terminal to abort
Then manually flash:
  1. Ozone console: exec FlashResource()
  2. Or use J-Flash tools directly
```

---

## If You Need to Disable Automation

### Disable CMake XIP Conversion
Edit `CMakeLists.txt`:
```cmake
# Comment out the resource target
# add_custom_target(resource ALL
#     COMMAND ...
# )
```

### Disable Ozone Auto-Flash
Edit `scripts/debug/ozone/Ozone.jdebug`:
```javascript
void OnStartupComplete(void) {
  // Commented out auto-flash
  // if (File.Exists(...)) {
  //   FlashResource();
  // }
}
```

---

## File Locations

| File | What It Does | Edit? |
|------|--------------|-------|
| CMakeLists.txt | Build automation | ⚠ Only if disabling |
| scripts/debug/ozone/Ozone.jdebug | Debug automation | ⚠ Only if disabling |
| scripts/tools/convert_logos_to_resource.py | Convert script | ✗ Don't edit |
| <BuildDirName>/resource.bin | Generated binary | ✗ Auto-generated |
| automation-guide.md | Full documentation | Read if lost |

---

## Memory Status

Before automation, logos added 337 KB to internal flash.
After automation: logos live in external QSPI XIP memory → saves 337 KB internal space for code.

Result: More room for features! ✓
