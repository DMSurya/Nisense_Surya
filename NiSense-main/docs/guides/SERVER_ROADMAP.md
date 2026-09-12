# NiSense Platform API — Phased Roadmap

**Status**: Planning (rescanned 2026-08-02)  
**Scope**: `apps/server` plus mobile/device paths that feed or consume it  
**Companion**: interactive status canvas in Cursor (`server-requirements-analysis.canvas.tsx`)

## Current architecture (post Wi‑Fi bulk)

```text
Device ──BLE──► Mobile (control, fallback sync, OTA SMP)
Device ──Wi‑Fi LAN HTTP──► Mobile LocalTransferServer  (/sync/records, /ota/*)
Mobile ──HTTPS──► Platform API  (/ingest/*, /artifacts/*, admin)
Device ──MQTT (optional)──► Broker  (live vitals/glucose only; not bulk NOR)
```

Phase 1 server remains a **mobile-gateway REST API**. Device Wi‑Fi bulk is **phone-local**; it does **not** hit the cloud API. Direct MQTTS on the Pi (port 8883) is still reserved, not wired on the server.

### Material changes since the July 2026 analysis

| Area | Now | Roadmap impact |
|------|-----|----------------|
| Multi-image OTA bundles | Server validate/store + mobile relay | Treat bundle upload as **done**; focus on campaigns |
| BLE + Wi‑Fi dual transport | Implemented (protocol 3.2) | Do **not** build device→cloud bulk HTTP unless product requires it |
| Mobile cloud push after BLE sync | Wired (`SyncService.pushSyncedRecords`) | Close gap: **Wi‑Fi sync path does not push** today |
| Optional device MQTT telemetry | Firmware `cloud_telemetry.c` | Server MQTT **consumer** is a separate phase if live cloud vitals are required |
| CSV upload | Dedupe + newline-based “valid/empty” | Still not a real schema validation lifecycle |
| Migrations | Still only dynamic `0001_initial` | Schema drift risk for deployed DBs |

---

## Phase A — Foundations (do first)

Goal: safe persistence, correct idempotency, auditable mutations, and scoped reads before broader clinical use.

| # | Work item | Why | Exit criteria |
|---|-----------|-----|---------------|
| A1 | Immutable Alembic revisions for all model drift | `0001_initial` uses `create_all`; upgraded DBs miss new columns | Fresh + existing DB upgrade paths tested |
| A2 | Fix reading uniqueness to `(device_id, type, record_id)` + atomic upsert | Docs/ingest pre-check disagree with constraint that includes `ts`; races possible | Concurrent retry test: no logical duplicates |
| A3 | Principal→patient/device ACL | Any authenticated user can list patients, readings, export | Role matrix documented + enforced in ingest/query/export/patients |
| A4 | Wire `AuditLog` writes + `GET /audit` | Table exists unused | Provision, mapping, artifact, user, ingest mutations audited |
| A5 | Pin `pytest` (+ test deps); API/RBAC/DB smoke suite | Only bundle/signing unit tests today | CI-capable `pytest` green on authz + ingest + artifacts |

**Dependency order**: A1 → A2 → A3 → A4; A5 in parallel once A2/A3 have hooks.

---

## Phase B — Complete the mobile-gateway data path

Goal: whatever the phone pulls (BLE or Wi‑Fi) can be archived on the server reliably.

| # | Work item | Why | Exit criteria |
|---|-----------|-----|---------------|
| B1 | After Wi‑Fi bulk success, push local records via `/ingest/readings` | `sync_page` BLE path pushes; Wi‑Fi path only stores locally | Same cloud outcome for BLE and Wi‑Fi sync |
| B2 | CSV schema validation + list/detail/download | Upload marks any non-empty file `valid`; no retrieval API | Parser errors reported; checksum + metadata idempotency |
| B3 | Optional: parse CSV into `readings` (or reject as archive-only) | Product decision | Documented contract + tests |
| B4 | Integration test: mobile payload → ingest → export | End-to-end confidence | Fixture covering glucose/vitals/raw types |
| B5 | Package `boto3` when `storage_backend=s3` | S3 adapter exists but dep missing | MinIO/S3 smoke in compose profile |

---

## Phase C — Controlled OTA delivery

Goal: move from “latest uploaded artifact” to governed rollouts. Bundle validation stays as-is.

| # | Work item | Why | Exit criteria |
|---|-----------|-----|---------------|
| C1 | Persist / expose `DeviceVersionState.target_version` assignment API | Column unused; applied-version is append-only | Device or mobile can query intended versions |
| C2 | Campaign model (artifact, device filter/group, stage, approve) | No rollout governance | Canary → cohort → fleet with audit |
| C3 | Compatibility metadata (min FW, hw variant, bundle kinds whitelist) | Bundle kinds not restricted to `resource\|model\|firmware` | Invalid components rejected at upload |
| C4 | Separate sync vs OTA success reporting | Soft-fail OTA after record sync can look “green” | Distinct statuses in applied-version / UI |
| C5 | Firmware signing/format policy | Only model Ed25519 + SHA today | Written policy: server MCUBoot check vs trust-mobile |
| C6 | Decide MAX32664 hub image as OTA kind | Hub FW path is host/BLE-oriented today | Kind in bundle or explicitly out-of-scope |

---

## Phase D — Connectivity boundary (product choice)

Pick **one** primary cloud path before investing heavily:

### Option D1 — Mobile remains the only cloud gateway (recommended default)

- Formalize in README/architecture: device never talks to Platform API for bulk.
- Server work: none for MQTT/WSS bulk; focus Phases A–C.
- Optional later: WebSocket fan-out from server→web for live **after mobile ingest** (not device WSS).
- Drop or reword deploy docs that imply public WSS/MQTTS until needed.

### Option D2 — Direct device cloud (MQTT / HTTPS)

Only if phone-less upload is a hard requirement:

| # | Work item |
|---|-----------|
| D2.1 | Device identity (cert/token) tied to provisioning |
| D2.2 | MQTTS broker + topic ACLs (or device HTTPS ingest) |
| D2.3 | Server consumer → same `readings` store + idempotency |
| D2.4 | Live telemetry bridge from firmware `cloud_telemetry` topics |
| D2.5 | Ops: open 8883 only with mutual TLS; never expose DB/API broadly |

Note: firmware MQTT today publishes **live vitals/glucose**, not NOR bulk history. Bulk archive would still need a designed protocol.

---

## Phase E — Clinical / AI product modules (later)

Deferred until A–C are auditable and scoped. Roles `clinical_trial_admin`, `investigator`, `ai_analyst` already exist in Keycloak/RBAC.

| # | Module | Server needs |
|---|--------|--------------|
| E1 | Clinical trials | Sites, protocols, enrollment, visit windows, consent flags |
| E2 | Statistics | Aggregates, cohorts, retention, de-identification rules |
| E3 | AI training | Dataset export, labeling jobs, model promotion → OTA artifacts |
| E4 | Dashboards | Stable read APIs before expanding `apps/web` beyond Data Browser |
| E5 | Calibration cloud view | Only if factory/QSPI cal must be visible centrally (else keep device-only) |

---

## Explicit non-goals (near term)

- Redirecting device Wi‑Fi bulk `POST /sync/records` at the cloud API (would need device auth, TLS on WExx, retries — redesign, not a config tweak).
- Replacing BLE control plane with Wi‑Fi.
- SoftAP / Wi‑Fi Direct product paths (explicitly out per dual-transport doc).

---

## Suggested sequencing

```text
A1–A2  persistence + idempotency
  └─ A3–A4  ACL + audit
  └─ A5     tests
B1     Wi‑Fi→cloud push parity          ⎫ can overlap late A
B2–B5  CSV + S3 + e2e                   ⎭
C1–C4  OTA campaigns                    after A4 (audit campaigns)
D      connectivity decision gate
E      clinical / AI                    after A3 + audit
```

## Tracking checklist

- [ ] Phase A complete  
  - [x] A1 Immutable Alembic revision `0002_reading_idempotency` (0001 left as historical `create_all` stamp)  
  - [x] A2 Reading idempotency via `reading_idempotency` + SAVEPOINT claim (Timescale-safe)  
  - [ ] A3 Patient/device ACL  
  - [ ] A4 AuditLog writes + GET `/audit`  
  - [x] A5 partial — `pytest` pinned; ingest idempotency unit tests added (full API/RBAC suite still open)  
- [ ] Phase B complete (including Wi‑Fi push parity)  
  - [x] B1 Wi‑Fi bulk success → `_pushToServer` via `getUnpushed` (parity with BLE path)  
  - [ ] B2–B5 CSV / e2e / S3  
- [ ] Phase C complete  
- [ ] Phase D decision recorded (D1 or D2)  
- [ ] Phase E scoped from customer Module N  

## References

- `apps/server/README.md` — Phase 1 API surface  
- `docs/architecture/BLE_WIFI_DUAL_TRANSPORT.md` — phone-local bulk (protocol 3.2)  
- `docs/build/MULTI_IMAGE_OTA.md` — bundle upload/apply  
- `docs/guides/PLATFORM_GUIDE.md` — operator checklist  
- `apps/mobile/lib/net/local_transfer_server.dart` — LAN endpoints  
- `apps/mobile/lib/services/sync_service.dart` — cloud ingest client  
- `src/net/cloud_telemetry.c` — optional device MQTT live publish  
