# Record store hardening — verification checklist

Manual / shell checks after flashing firmware that includes mode-aware ACK,
summary reserve, boot reconcile, and meas_precheck PPG footprint floor.

## Shell (`nisense rec`)

1. `nisense rec stats` — shows `dropped`, `crc_fail`, `summary_ack`.
2. Log a measurement; note `pending` / `next_id`.
3. `nisense rec ack <id> summary` — `pending` **unchanged**; `summary_ack` advances.
4. `nisense rec ack <id> full` — `pending` decreases / `tail` advances.
5. Fill store until raw appends hit summary reserve (or simulate via many PPG
   cycles without sync) — raw should log `-ENOSPC` while summaries remain.
6. Health cycle with free slots &lt; 126 — meas_precheck should refuse start.

## Mobile

1. **Sync** (summaries) → “Mark summaries received” → device pending raw still
   present; NOR space not freed.
2. **Sync raw (full)** or **Wi‑Fi sync** (always `afterId=0`) → receives raw
   previously skipped by summary cursor → wipe with FULL ACK → pending drops.
3. Device-build banner appears when `dropped` or `crc_fail_count` &gt; 0.
4. Export CSV with deliberately incomplete chunks → `# INCOMPLETE` comment line.

## BLE wire

- ACK is 6 bytes: `[0x02][up_to:u32 LE][mode u8]`.
- Legacy 5-byte ACK still treated as FULL on firmware.
