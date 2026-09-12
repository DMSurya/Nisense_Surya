# Multi-image BLE OTA (nisense-ota-v1)

NiSense field updates can ship as a single ZIP that the phone unpacks and
relays over three BLE channels. MCUboot only applies the **application** image.

## Bundle format

```text
nisense_ota_<version>.zip
  manifest.json
  resource.bin                    (optional UI Resource; kind "resource")
  model_pack.bin                  (optional clinical pack; both glucose variants)
  zephyr.signed.bin               (optional)
```

`manifest.json` (`format: nisense-ota-v1`) lists each component with `sha256`,
optional model `signature`, and `transfer_order` (default
`resource` → `model` → `firmware`).

## Pack

```powershell
python scripts/ota/pack_ota_bundle.py `
  --version 1.2.3 `
  --firmware build_sdk_v330/NiSense/zephyr/zephyr.signed.bin `
  --resource build_sdk_v330/resource.bin `
  --model build_sdk_v330/model_pack.bin `
  --model-variant pack `
  --model-version 12 `
  --out dist/nisense_ota_1.2.3.zip
```

Resource and Model updates write the **inactive** A/B slot, CRC-validate, then
flip NVS — same safety model as firmware slot staging.
## Upload to server

Web UI: **Artifacts** → Kind = `bundle (ZIP)` → choose the ZIP → Upload.

Or CLI (dev provider):

```powershell
python scripts/ota/upload_artifact.py `
  --base-url http://localhost:8000/api/v1 `
  --dev-token --roles device_engineer `
  --kind bundle --version 1.2.3 `
  --file dist/nisense_ota_1.2.3.zip
```

curl equivalent:

```bash
TOKEN=$(curl -s -XPOST localhost:8000/api/v1/auth/dev-token \
  -H 'content-type: application/json' \
  -d '{"username":"admin","roles":["device_engineer"]}' | jq -r .access_token)

curl -s -XPOST localhost:8000/api/v1/artifacts \
  -H "Authorization: Bearer $TOKEN" \
  -F kind=bundle -F version=1.2.3 \
  -F file=@dist/nisense_ota_1.2.3.zip
```

Re-upload the same version: delete the row in the Artifacts UI (or
`DELETE /api/v1/artifacts/{id}`), then upload again. Duplicate
`(kind, variant, version)` returns HTTP 409.

Server checks on upload: ZIP + `manifest.json` + per-component SHA-256; model
components also get Ed25519 verification when `NISENSE_REQUIRE_SIGNED_MODELS=true`
or when a `signature` is present in the manifest.

## Apply on device (mobile)

1. Bond / connect the watch.
2. Server page → **Update bundle** (role: `device_engineer` / `super_admin`).
3. Phone transfers **XIP → model → firmware** with progress; device shows LVGL
   progress during BLE transfer.
4. After SMP reset, MCUboot applies the app image (no LCD during boot apply).

## Device behaviour

- **XIP / model:** write **inactive** A/B slot → CRC (+ optional model Ed25519) →
  `ota_compat` check → flip NVS active slot.
- **Firmware:** SMP upload to QSPI `image-1` → pending → reset → MCUboot
  swap-using-move → app `boot_write_img_confirmed()`.
- **Progress:** LVGL overlay during BLE transfer; no progress during MCUboot.

See also [`OTA_BOOTLOADER.md`](OTA_BOOTLOADER.md) and [`PARTITION_LAYOUT.md`](PARTITION_LAYOUT.md).
