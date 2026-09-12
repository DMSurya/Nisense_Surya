# HCM Mobile — Android BLE Monitor

Flutter companion app for the NiSense wearable: **Bluetooth scan**, live **Dashboard**
(with last-reading seed and hold-to-start measure), **Raw** streams, **Settings**,
**Sync** (NOR pull + local SQLite + CSV/XLSX + cloud push), **Firmware/DFU**, plus
**Server / Cloud** via the app-bar icon (OIDC, OTA artifacts).

| Reference | Location |
|-----------|----------|
| **Platform guide** (build / flash / screens / export) | [`docs/guides/PLATFORM_GUIDE.md`](../../docs/guides/PLATFORM_GUIDE.md) |
| GATT protocol (Dart) | [`lib/protocol/hcm_protocol.dart`](lib/protocol/hcm_protocol.dart) |
| GATT protocol (Python) | [`../pc/hcm_protocol.py`](../pc/hcm_protocol.py) |
| Connect / pair / subscribe flow | [`docs/clients/ANDROID_MONITOR.md`](../../docs/clients/ANDROID_MONITOR.md) |
| BLE security matrix | [`docs/ble/BLE_DATA_SECURITY_DATABASE.md`](../../docs/ble/BLE_DATA_SECURITY_DATABASE.md) |

---

## Supported targets

| Item | Requirement |
|------|-------------|
| **Host OS** | Windows 10/11, macOS, or Linux (build machine) |
| **Device OS** | **Android 10+ (API 29)** with BLE central |
| **Firmware** | SECURE BLE profile (default in `conf/features/ble.conf`) |
| **Flutter** | Stable channel, Dart **≥ 3.12** (`pubspec.yaml`) |
| **JDK** | **17** (required by Android Gradle Plugin 8.11) |

The app does **not** run as a Windows/macOS/Linux desktop target for BLE — use a physical phone or emulator with Bluetooth passthrough for development.

---

## Build environment from scratch

Follow these steps on a **clean machine** with nothing installed yet. Order matters.

### 1. Clone the repository

```powershell
git clone <repo-url> NiSense
cd NiSense
```

Open the multi-root workspace (optional but convenient):

```powershell
code NiSense.code-workspace
```

The Flutter project root is `apps/mobile/`.

### 2. Install Git (if missing)

- Windows: [Git for Windows](https://git-scm.com/download/win)
- Verify: `git --version`

### 3. Install Flutter SDK

1. Download the **stable** Flutter SDK: [https://docs.flutter.dev/get-started/install](https://docs.flutter.dev/get-started/install)
2. Extract to a fixed path, e.g. `C:\flutter` (avoid spaces in the path).
3. Add Flutter to `PATH`:
   - Windows: **Settings → System → About → Advanced system settings → Environment Variables**
   - Add `C:\flutter\bin` to **User** `Path`
4. Open a **new** terminal and verify:

```powershell
flutter --version
dart --version
```

Expected: Flutter 3.44+ stable, Dart 3.12+.

Enable desktop tooling only if you want IDE support — Android builds do not require Windows desktop:

```powershell
flutter config --no-enable-windows-desktop
```

### 4. Install Android Studio

1. Download [Android Studio](https://developer.android.com/studio) (latest stable).
2. Run the installer; include:
   - **Android SDK**
   - **Android SDK Platform**
   - **Android Virtual Device** (optional)
3. In Android Studio: **Settings → Plugins** → install **Flutter** and **Dart** (restart IDE).

### 5. Install Android SDK components

Open **Android Studio → Settings → Languages & Frameworks → Android SDK → SDK Platforms** and install:

| Component | Why |
|-----------|-----|
| **Android API 36** (or latest shown by `flutter doctor`) | `compileSdk` / `targetSdk` (Flutter-managed) |
| **Android 10 (API 29)** | Minimum supported device OS |

Under **SDK Tools**, install:

| Tool | Why |
|------|-----|
| **Android SDK Build-Tools** (latest) | APK packaging |
| **Android SDK Command-line Tools** | `sdkmanager`, CI |
| **Android SDK Platform-Tools** | `adb` |
| **Android Emulator** | Optional virtual device |
| **NDK** | Pulled automatically when Gradle resolves `ndkVersion` |

Note the SDK location (typical Windows path):

```text
C:\Users\<you>\AppData\Local\Android\Sdk
```

Set `ANDROID_HOME` (recommended):

```powershell
# PowerShell — User environment variable
[Environment]::SetEnvironmentVariable("ANDROID_HOME", "$env:LOCALAPPDATA\Android\Sdk", "User")
```

Add platform-tools to `Path`:

```text
%LOCALAPPDATA%\Android\Sdk\platform-tools
```

### 6. Install JDK 17

AGP **8.11.1** in this project requires **Java 17**.

- Android Studio bundles a JBR — usually sufficient when building from the IDE.
- CLI builds: install [Eclipse Temurin 17](https://adoptium.net/) or use Android Studio’s bundled JDK.

Set `JAVA_HOME` if `flutter doctor` reports a Java issue:

```powershell
# Example — adjust to your JDK install path
[Environment]::SetEnvironmentVariable("JAVA_HOME", "C:\Program Files\Eclipse Adoptium\jdk-17.0.x-hotspot", "User")
```

### 7. Accept Android licenses

```powershell
flutter doctor --android-licenses
```

Type `y` for each prompt.

### 8. Configure `local.properties`

Gradle needs Flutter and Android SDK paths. This file is **local only** (gitignored).

```powershell
cd apps\mobile\android
copy local.properties.example local.properties
notepad local.properties
```

Fill in both lines (use forward slashes or escaped backslashes):

```properties
flutter.sdk=C:\\flutter
sdk.dir=C:\\Users\\you\\AppData\\Local\\Android\\Sdk
```

Or let Flutter create it automatically on first build:

```powershell
cd apps\mobile
flutter pub get
flutter build apk --debug
```

### 9. Install project dependencies

From the repo root or `apps/mobile`:

```powershell
cd apps\mobile
flutter pub get
```

This resolves Dart packages (`flutter_blue_plus`, `permission_handler`, `sqflite`, etc.) into `pubspec.lock`.

### 10. Verify the toolchain

```powershell
flutter doctor -v
```

Resolve every **✗** before continuing. A healthy Android setup shows:

- Flutter (Channel stable, …)
- Android toolchain — Android SDK at `…`, Platform android-36 (or similar)
- Android Studio (version …)
- Connected device (after phone setup below)

Common fixes:

| `flutter doctor` issue | Fix |
|------------------------|-----|
| `cmdline-tools component is missing` | SDK Manager → install **Android SDK Command-line Tools** |
| `Android license status unknown` | `flutter doctor --android-licenses` |
| `Unable to locate Android SDK` | Set `ANDROID_HOME`, create `local.properties` |
| `Java version …` | Point `JAVA_HOME` to JDK 17 |

### 11. (Optional) Python — Nordic UUID name sync

The committed file `lib/protocol/generated/sig_uuids.g.dart` is already in the repo. Regenerate only after Nordic database updates:

```powershell
python --version   # 3.8+
python ..\..\scripts\tools\sync_bluetooth_uuids.py
```

Requires network access to fetch Nordic’s `bluetooth-numbers-database`.

### 12. Prepare a physical Android device

Recommended for SECURE MITM pairing (emulator Bluetooth is limited).

1. **Developer options:** Settings → About phone → tap **Build number** seven times.
2. **USB debugging:** Settings → Developer options → enable **USB debugging**.
3. Connect via USB; accept the **RSA fingerprint** prompt on the phone.
4. Verify:

```powershell
adb devices
flutter devices
```

**OEM USB drivers (Windows):** install from the phone maker if `adb devices` shows `unauthorized` or no device. OnePlus/Oppo: [OPlus USB drivers](https://www.oneplus.com/support/softwareupgrade).

**Wireless debugging (Android 11+):** Settings → Developer options → Wireless debugging → pair with `adb pair` / `adb connect` if you prefer Wi‑Fi.

### 13. First build (clean clone)

Debug APK (fastest sanity check):

```powershell
cd apps\mobile
flutter build apk --debug
```

Output: `build/app/outputs/flutter-apk/app-debug.apk`

Install directly to a connected phone:

```powershell
flutter install -d <device_id>
```

Or run with hot reload:

```powershell
flutter run -d <device_id>
```

Release APK (unsigned debug keystore today — replace before store publish):

```powershell
flutter build apk --release
```

### 14. Open in Android Studio (optional)

1. **File → Open** → select `apps/mobile` (not the repo root).
2. Wait for Gradle sync (Gradle **8.14**, AGP **8.11.1** — first sync downloads wrappers).
3. Select the phone in the device dropdown → **Run**.

---

## Android project pins

These are fixed in the repo; you normally do not change them:

| Setting | Value | File |
|---------|-------|------|
| `minSdk` | **29** (Android 10) | `android/app/build.gradle.kts` |
| `compileSdk` / `targetSdk` | Flutter-managed | same |
| Java / Kotlin JVM | **17** | `android/app/build.gradle.kts` |
| Gradle | **8.14** | `android/gradle/wrapper/gradle-wrapper.properties` |
| AGP | **8.11.1** | `android/settings.gradle.kts` |
| Kotlin plugin | **2.2.20** | `android/settings.gradle.kts` |
| Application ID | `com.aarms.hcm` | `android/app/build.gradle.kts` |

### Built-in Kotlin (AGP 9 readiness)

Flutter 3.44+ warns when a plugin applies the legacy `kotlin-android` Gradle plugin. This project avoids that by:

- Using **`file_selector`** (Flutter team) for DFU image pick — not `file_picker`, which still applies KGP on AGP 8.x.
- Keeping the app module on the **built-in Kotlin DSL** (`kotlin { compilerOptions { … } }` in `app/build.gradle.kts`, no `kotlin-android` plugin in the app).
- Pinning **AGP 8.11.1** until all BLE stack plugins are verified on **AGP 9.0.1 + `android.builtInKotlin=true`**.

When upgrading to AGP 9 (Flutter template default in 3.44):

1. Set `android.builtInKotlin=true` in `android/gradle.properties`.
2. Bump AGP to **9.0.1** and Gradle to **9.1.0** in `settings.gradle.kts` / `gradle-wrapper.properties`.
3. Run `flutter build apk` and resolve any remaining plugin KGP warnings per [Flutter built-in Kotlin migration](https://docs.flutter.dev/release/breaking-changes/migrate-to-built-in-kotlin/for-app-developers).

**Cross-drive Gradle note:** if the Pub cache is on another drive from the project (e.g. `C:` vs `E:`), Kotlin incremental compile can fail. This repo sets `kotlin.incremental=false` in `android/gradle.properties`.

---

## Runtime permissions (device)

The app requests BLE permissions at startup (`lib/main.dart`):

| Android version | Permissions used |
|-----------------|------------------|
| **10–11 (API 29–30)** | `ACCESS_FINE_LOCATION` (required for BLE scan) + legacy Bluetooth |
| **12+ (API 31+)** | `BLUETOOTH_SCAN`, `BLUETOOTH_CONNECT` |

If scan returns **no devices** on Android 10–11, check that **Location** is granted for the app. On Android 12+, grant **Nearby devices**.

Manifest declarations: `android/app/src/main/AndroidManifest.xml`.

---

## Connect to the wearable

After the app builds and installs:

1. Flash **SECURE** firmware (default BLE profile).
2. Launch **NiSense Link** on the phone → grant permissions.
3. **Bluetooth** tab → tap **NiSense** (default `CONFIG_BT_DEVICE_NAME`) or Watch/Pulse if renamed.
4. For SECURE firmware: complete **numeric comparison** pairing in the system dialog; the Dashboard shows the passkey from GATT `f016`.
5. Full ceremony: [`docs/clients/ANDROID_MONITOR.md`](../../docs/clients/ANDROID_MONITOR.md).

To reset a bad bond: Android **Settings → Bluetooth →** forget the device, then scan again.

### Dashboard — last readings & hold-to-start

After a bonded connect, firmware seeds BLE vitals/glucose/temp (and insulin/HOMA when available) from `home/last` plus the latest NOR summary records, then re-pushes notifies (~3–4 s). Dashboard cards update from those notifies.

**Start Vitals / Start Glucose** are **press-and-hold** for **5 s** (same timeout as the watch Measure screen, `CONFIG_UI_MEASURE_LONG_PRESS_MS`). On complete the app writes Measurement Control (`f010`) with optional 3rd byte **`0x01` = skip proximity** (`measFlagSkipProx`). Short release cancels. See [`docs/guides/PLATFORM_GUIDE.md`](../../docs/guides/PLATFORM_GUIDE.md).

### Sync & Server — records, CSV/XLSX, cloud

| Piece | Path |
|-------|------|
| BLE record sync client | `lib/ble/record_sync_client.dart` (service `…def4`, chars `f401`–`f403`) |
| Local SQLite store | `lib/services/record_local_store.dart` (All Records / Metric History) |
| CSV / XLSX exporters | `lib/services/record_csv_exporter.dart`, `record_xlsx_exporter.dart` |
| Output dir | App documents **`HCM_Logs/`** |
| Cloud sync helper | `lib/services/sync_service.dart` → `POST /api/v1/ingest/readings` |
| Server / OIDC UI | App-bar cloud icon → `lib/ui/server/server_page.dart` |

Sheets with `Timestamp_unix` also export **`Date`** (`yyyy-MM-dd`) and **`Time`** (`HH:mm:ss`)
in the phone’s local timezone (`lib/util/record_timestamp.dart`).
Raw sheets: **Glucose_Raw** / **PPG_Raw** prefer **device capture timestamps**
stored on each sample (schema v3); legacy chunks fall back to parent summary time.
Summary `Timestamp_unix` is calculation-complete time.

Why Hb / HRV / BP / Quality / SNR are often **0** on a 20 s capture:
[`docs/architecture/PPG_ALGO_PARAMS_AND_SCHEDULING.md`](../../docs/architecture/PPG_ALGO_PARAMS_AND_SCHEDULING.md#9-why-many-vitals-export-fields-are-zero).

Firmware detail: [`docs/architecture/STORAGE_NOR_RECORD_STORE.md`](../../docs/architecture/STORAGE_NOR_RECORD_STORE.md).  
Cloud Excel: `GET /api/v1/export/readings.xlsx` — [`apps/server/README.md`](../server/README.md).

---

## Troubleshooting

| Symptom | Likely cause | Action |
|---------|--------------|--------|
| App not in app drawer after install | OEM drawer search quirks | Search **NiSense Link**; or **Settings → Apps → NiSense Link → Open** |
| `flutter install` fails — no release APK | Default is release mode | Use `flutter run` or `flutter install --debug` |
| Dart VM service socket error (OnePlus) | Debug-only; app still runs | Ignore, or use `flutter run --no-dds`; hot reload may be limited |
| `cmdline-tools component is missing` | SDK tools not installed | SDK Manager → install **Android SDK Command-line Tools** |
| Gradle sync fails / Kotlin errors | Wrong JDK | Use JDK 17; invalidate caches in Android Studio |
| Kotlin incremental / cross-drive build fail | Pub cache on another drive | `kotlin.incremental=false` already in `gradle.properties` |
| `adb: device unauthorized` | USB debug not accepted | Re-plug USB; confirm RSA dialog on phone |
| Scan finds nothing (Android 10–11) | Location denied | App info → Permissions → Location → Allow |
| Scan finds nothing (Android 12+) | Nearby devices denied | Grant Bluetooth / Nearby devices |
| Connect fails with `2a05` / `ERROR_GATT_WRITE_REQUEST_BUSY` | FBP auto-subscribes to Service Changed | Fixed: `discoverServices(subscribeToServicesChanged: false)` |
| Connect fails with `requestMtu` timeout / `CONNECTION_TERMINATED_BY_LOCAL_HOST` while firmware shows Security level 4 | FBP `connect(mtu:512)` awaits MTU then disconnects on timeout | Fixed: `connect(mtu: null)`; MTU negotiated later as best-effort |
| Connect fails immediately | Stale bond | Forget device in system Bluetooth settings |
| Dashboard empty after connect | Unpaired SECURE / seed still in flight | Pair first; wait ~5 s for seed re-push |
| Hold Start does nothing / ATT error | Old firmware rejects 3-byte `f010` | Flash firmware that accepts len 1–3 + `BLE_MEAS_FLAG_SKIP_PROX` |
| `connect()` license error | `flutter_blue_plus` 2.x | Already handled in `hcm_ble_client.dart` (`License.commercial`) |
| KGP / `file_picker` warning on `flutter run` | Legacy plugin applies `kotlin-android` | Fixed: DFU uses `file_selector`; do not re-add `file_picker` until its stable 12.x release supports AGP 9 built-in Kotlin |
| Build OOM | Gradle heap | `android/gradle.properties` sets `-Xmx4G` |
| Emulator has no BLE | Expected | Use a physical phone for pairing tests |

### Runtime logs (`flutter run`)

App lines are prefixed **`[NiSense/<tag>]`** (ble, flow, gatt, cts, ui, perm). Plugin noise is `[FBP]`.

```powershell
cd apps\mobile
flutter run
# In the same console, filter:
#   [NiSense/flow] === connectAndSetup begin
#   [NiSense/ble]  connect begin / connect threw (timeout)
#   [NiSense/gatt] discover / subscribe
```

Or separately:

```powershell
adb logcat -v time | Select-String -Pattern "NiSense/|FBP|flutter"
```

The Dart VM service socket error on Oppo/OnePlus is unrelated to BLE — app still runs; use `flutter run --no-dds` if hot reload is broken.

Clean rebuild:

```powershell
cd apps\mobile
flutter clean
flutter pub get
flutter run
```

---

## UI / theme

- **Colors:** NiSense watch palette (`lib/theme/nisense_colors.dart`) — `#040C24` background, `#1E4DFF` primary
- **Medical + nav icons:** `flutter_svg` from `assets/svg/` (copied from PC monitor SVGs)
- **Branding PNGs:** `assets/icons/app.png` (launcher), `assets/icons/icon.png` (header), `assets/splash.png` (loading)

Regenerate launcher icons after changing `app.png`:

```powershell
dart run flutter_launcher_icons
```

---

## Project layout

```text
lib/
├── main.dart
├── theme/           # NiSense colors, theme, icons
├── protocol/        # hcm_protocol, gatt_registry, generated/sig_uuids.g.dart
├── ble/             # hcm_ble_client, connection_flow, record_sync_client
├── state/           # hcm_backend.dart
├── services/        # session_logger, record_*_exporter, record_local_store, sync_service, ota_service
├── auth/            # OIDC auth_service
├── net/             # api_client, server_config
├── dfu/             # smp_client.dart
└── ui/              # app_shell + pages (dashboard, sync, server, device, …)
```

---

## Pages

| Entry | Feature |
|-------|---------|
| **Bluetooth** (tab) | BLE scan, known devices, connect/disconnect |
| **Dashboard** (tab) | Live + last-reading vitals; hold-to-start Vitals/Glucose; battery / wear |
| **Raw** (tab) | PPG / glucose raw streams (former Charts) |
| **Settings** (tab) | RTC, brightness, volume, WiFi, PMIC lab |
| **Sync** (tab) | NOR `…def4` pull, local SQLite, CSV/XLSX, cloud ingest push |
| **Firmware** (tab) | Pair/forget, SMP DFU upload |
| **Server** (app-bar cloud) | Cloud login, server URL, OTA artifacts |
| Device Info / All Records / Metric History | Pushed routes from Dashboard / Sync |

GATT explorer (`services_page.dart`) remains in the tree for lab use but is not on the bottom nav.

---

## Tests

```powershell
cd apps\mobile
flutter test
```

Protocol round-trip tests live in `test/protocol_test.dart`.
