"""
SMP Client — MCUmgr SMP-over-BLE
=================================
Implements the MCUmgr Simple Management Protocol (SMP) transport over BLE
for image upload to Zephyr devices running CONFIG_MCUMGR_TRANSPORT_BT=y.

Protocol overview
-----------------
- Service UUID  : 8D53DC1D-1DB7-4CD3-868B-8A527460AA84  (Nordic SMP)
- Characteristic: DA2E7828-FBCE-4E01-AE9E-261174997C48  (SMP Data, WRITE + NOTIFY)

Each SMP frame = 8-byte header (big-endian) + CBOR payload:

    Offset  Field   Meaning
    0       op      2=write, 3=write-rsp, 0=read, 1=read-rsp
    1       flags   0 (reserved)
    2-3     len     CBOR payload length (big-endian)
    4-5     group   command group (0=OS, 1=IMG)
    6       seq     per-transaction sequence counter (wraps 0-255)
    7       id      command id within group

Image upload procedure
----------------------
1. Send first chunk (off=0) with image hash + total length.
   Device responds with the expected next offset (or error).
2. Send remaining chunks with sequential offsets.
3. Mark the uploaded image as "pending" (test-swap) via IMG set-state.
4. Send OS reset — MCUboot validates signature and performs the swap.

Chunk size
----------
The ATT MTU was negotiated to 247 during connection setup (L2CAP_TX_MTU=247).
Each BLE WRITE packet can carry  MTU - 3 (ATT header) = 244 bytes.
After deducting the 8-byte SMP header the max CBOR payload is 236 bytes, but
the CBOR encoding of the "data" field adds ~3 bytes overhead, leaving ~233
bytes of image data per chunk.  We use a conservative SMP_CHUNK_BYTES=220 so
short-packet retransmissions still fit within a single ATT PDU.

If the MTU is smaller (e.g. 23 bytes on first connection) the chunk size is
recomputed from the actual negotiated value.
"""

from __future__ import annotations

import asyncio
import hashlib
import logging
import struct
from pathlib import Path
from typing import Callable, Optional

import cbor2
from bleak import BleakClient

log = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Service / characteristic UUIDs
# ---------------------------------------------------------------------------
SMP_SERVICE_UUID  = "8D53DC1D-1DB7-4CD3-868B-8A527460AA84"
SMP_CHAR_UUID     = "DA2E7828-FBCE-4E01-AE9E-261174997C48"

# ---------------------------------------------------------------------------
# SMP header layout (8 bytes, big-endian)
# ---------------------------------------------------------------------------
_HDR = struct.Struct(">BBHHBBb")  # extra 'b' aligns to 8, but struct is 7+? Re-check:
#   op(1) flags(1) len(2) group(2) seq(1) id(1) = 8 bytes total
_HDR = struct.Struct(">BBHHBB")   # 8 bytes: op, flags, len_be, group_be, seq, id

# ---------------------------------------------------------------------------
# SMP operation codes
# ---------------------------------------------------------------------------
OP_READ      = 0
OP_READ_RSP  = 1
OP_WRITE     = 2
OP_WRITE_RSP = 3

# ---------------------------------------------------------------------------
# MCUmgr group IDs
# ---------------------------------------------------------------------------
GRP_OS  = 0   # OS management
GRP_IMG = 1   # Image management

# ---------------------------------------------------------------------------
# OS group command IDs
# ---------------------------------------------------------------------------
OS_ID_ECHO   = 0
OS_ID_RESET  = 5

# ---------------------------------------------------------------------------
# IMG group command IDs
# ---------------------------------------------------------------------------
IMG_ID_STATE  = 0   # get/set image state (pending / confirmed)
IMG_ID_UPLOAD = 1   # upload image data chunk

# ---------------------------------------------------------------------------
# Error / response codes
# ---------------------------------------------------------------------------
RC_OK        = 0
RC_ENOENT    = 2   # image slot empty / not found
RC_EBADSTATE = 11  # image already present with same hash — skip upload


class SMPError(Exception):
    """Raised when the device returns a non-zero SMP response code."""
    def __init__(self, rc: int, msg: str = ""):
        super().__init__(f"SMP rc={rc}: {msg}")
        self.rc = rc


class SMPClient:
    """
    Minimal MCUmgr SMP client for BLE image upload.

    Usage::

        async with SMPClient(bleak_client) as smp:
            await smp.upload_image(
                image_path=Path("zephyr.signed.bin"),
                progress_cb=lambda sent, total: print(f"{sent}/{total}")
            )
            await smp.set_image_pending()
            await smp.reset_device()
    """

    # Conservative default chunk size — fits in a single 244-byte ATT write PDU
    # after CBOR overhead.  Recomputed from negotiated MTU in _connect().
    DEFAULT_CHUNK_BYTES = 220

    def __init__(self, client: BleakClient) -> None:
        self._client = client
        self._seq: int = 0
        self._pending_rsp: Optional[asyncio.Future] = None
        self._rsp_lock = asyncio.Lock()
        self._chunk_bytes = self.DEFAULT_CHUNK_BYTES
        self._image_hash: bytes = b""
        # Reassembly buffer for SMP responses that span multiple BLE
        # notifications (e.g. image-list with hashes/versions can exceed one
        # ATT PDU).  Accumulated until 8-byte header + declared length arrive.
        self._rx_buf = bytearray()

    # ------------------------------------------------------------------
    # Context-manager / setup
    # ------------------------------------------------------------------
    async def __aenter__(self) -> "SMPClient":
        await self._setup()
        return self

    async def __aexit__(self, *_) -> None:
        await self._teardown()

    async def _setup(self) -> None:
        """Subscribe to SMP notifications and compute chunk size from MTU."""
        await self._client.start_notify(SMP_CHAR_UUID, self._on_notify)
        # Compute effective chunk size from negotiated ATT MTU.
        # bleak exposes mtu_size on connected clients (bleak >= 0.21).
        try:
            mtu = self._client.mtu_size  # type: ignore[attr-defined]
            # ATT WRITE header overhead = 3 bytes; SMP header = 8 bytes;
            # CBOR map overhead for {"data": <bytes>} ≈ 8 bytes.
            available = mtu - 3 - 8 - 8
            self._chunk_bytes = max(64, min(available, self.DEFAULT_CHUNK_BYTES))
            log.debug("SMP chunk size: %d bytes (MTU=%d)", self._chunk_bytes, mtu)
        except AttributeError:
            log.debug("MTU not available via bleak; using default chunk size %d", self._chunk_bytes)

    async def _teardown(self) -> None:
        try:
            await self._client.stop_notify(SMP_CHAR_UUID)
        except Exception:
            pass

    # ------------------------------------------------------------------
    # Notification handler
    # ------------------------------------------------------------------
    def _on_notify(self, _handle: int, data: bytearray) -> None:
        """Reassemble SMP response fragments, then resolve the pending future.

        A single SMP response is ``8-byte header + CBOR payload``.  When the
        payload is larger than the negotiated ATT MTU the device sends it as
        several notifications; resolving on the first fragment would feed
        truncated CBOR to ``cbor2.loads()``.  We accumulate fragments until the
        full frame (``8 + header.len`` bytes) has arrived.
        """
        if not self._pending_rsp or self._pending_rsp.done():
            # No transaction waiting — drop stray/late fragments.
            return

        self._rx_buf.extend(data)

        # Need the 8-byte header before the total frame length is known.
        if len(self._rx_buf) < 8:
            return

        # Header layout: op, flags, len(BE u16), group(BE u16), seq, id.
        payload_len = int.from_bytes(self._rx_buf[2:4], "big")
        total = 8 + payload_len

        if len(self._rx_buf) < total:
            # More fragments still to come.
            return

        frame = bytes(self._rx_buf[:total])
        del self._rx_buf[:total]
        self._pending_rsp.set_result(frame)

    # ------------------------------------------------------------------
    # Low-level frame helpers
    # ------------------------------------------------------------------
    def _build_frame(self, op: int, group: int, cmd_id: int, payload: dict) -> bytes:
        cbor_bytes = cbor2.dumps(payload)
        hdr = _HDR.pack(op, 0, len(cbor_bytes), group, self._seq, cmd_id)
        self._seq = (self._seq + 1) & 0xFF
        return hdr + cbor_bytes

    async def _transact(self, frame: bytes, timeout: float = 5.0) -> dict:
        """Send a frame and await the response notification."""
        async with self._rsp_lock:
            loop = asyncio.get_event_loop()
            self._pending_rsp = loop.create_future()
            # Drop any partial fragment left over from a prior (timed-out)
            # transaction so reassembly starts clean.
            self._rx_buf.clear()
            try:
                await self._client.write_gatt_char(
                    SMP_CHAR_UUID, bytearray(frame), response=True
                )
                rsp_bytes = await asyncio.wait_for(self._pending_rsp, timeout=timeout)
            finally:
                self._pending_rsp = None

        # Strip 8-byte SMP response header; parse CBOR payload
        if len(rsp_bytes) < 8:
            raise SMPError(-1, f"Short response: {rsp_bytes.hex()}")
        payload = cbor2.loads(rsp_bytes[8:])
        rc = payload.get("rc", 0)
        return payload

    # ------------------------------------------------------------------
    # IMG — upload
    # ------------------------------------------------------------------
    async def upload_image(
        self,
        image_path: Path,
        progress_cb: Optional[Callable[[int, int], None]] = None,
        slot: int = 0,
    ) -> None:
        """
        Stream a signed firmware image to the device's secondary slot.

        :param image_path:  Path to the signed binary (zephyr.signed.bin).
        :param progress_cb: Optional callback(bytes_sent, total_bytes).
        :param slot:        Image slot index (0 = primary app, 1 = secondary).
                            Most MCUmgr setups use slot 1 for the new image.
        """
        image_data = image_path.read_bytes()
        total = len(image_data)
        sha = hashlib.sha256(image_data).digest()
        self._image_hash = sha

        log.info("SMP upload: %s  size=%d  sha256=%s", image_path.name, total, sha.hex()[:16])

        offset = 0
        while offset < total:
            chunk = image_data[offset: offset + self._chunk_bytes]
            is_first = (offset == 0)

            if is_first:
                payload = {
                    "image": slot,
                    "len":   total,
                    "sha":   sha,
                    "data":  chunk,
                    "off":   0,
                }
            else:
                payload = {
                    "data": chunk,
                    "off":  offset,
                }

            frame = self._build_frame(OP_WRITE, GRP_IMG, IMG_ID_UPLOAD, payload)
            try:
                rsp = await self._transact(frame, timeout=15.0)
            except asyncio.TimeoutError:
                raise SMPError(-1, f"Upload timed out at offset {offset}")

            rc = rsp.get("rc", 0)
            if rc == RC_EBADSTATE and is_first:
                # Image already present with same hash — skip upload
                log.info("SMP: image already present (rc=11), skipping upload")
                if progress_cb:
                    progress_cb(total, total)
                return
            if rc not in (RC_OK,):
                raise SMPError(rc, f"upload error at offset {offset}")

            # Device may advance offset by more than one chunk if it pre-erased
            next_off = rsp.get("off", offset + len(chunk))
            if next_off <= offset:
                raise SMPError(-1, f"Device did not advance offset (stuck at {offset})")
            offset = next_off

            if progress_cb:
                progress_cb(offset, total)

        log.info("SMP upload complete (%d bytes)", total)

    # ------------------------------------------------------------------
    # IMG — set image state (mark pending for swap)
    # ------------------------------------------------------------------
    async def set_image_pending(self, confirm: bool = False) -> None:
        """
        Mark the uploaded image as pending (test-swap).

        :param confirm: If True, auto-confirm after first successful boot
                        (permanent swap). If False (default), MCUboot performs
                        a test swap — the device reverts to the old image if
                        the new image does not call mcuboot_img_confirmed().
        """
        if not self._image_hash:
            raise SMPError(-1, "No image hash available — call upload_image() first")
        payload = {"hash": self._image_hash, "confirm": confirm}
        frame = self._build_frame(OP_WRITE, GRP_IMG, IMG_ID_STATE, payload)
        rsp = await self._transact(frame, timeout=5.0)
        rc = rsp.get("rc", 0)
        if rc != RC_OK:
            raise SMPError(rc, "set_image_pending failed")
        log.info("SMP: image marked as pending (confirm=%s)", confirm)

    # ------------------------------------------------------------------
    # OS — reset (triggers MCUboot swap)
    # ------------------------------------------------------------------
    async def reset_device(self) -> None:
        """
        Send the OS reset command.  The device reboots immediately;
        MCUboot validates and swaps the pending image.
        The BLE connection will drop — expected behaviour.
        """
        frame = self._build_frame(OP_WRITE, GRP_OS, OS_ID_RESET, {})
        try:
            await self._transact(frame, timeout=3.0)
        except (asyncio.TimeoutError, Exception):
            # Connection drops right after reset — ignore the error
            pass
        log.info("SMP: reset command sent")

    # ------------------------------------------------------------------
    # IMG — get image list (diagnostic)
    # ------------------------------------------------------------------
    async def get_image_list(self) -> list[dict]:
        """Return the list of image slots reported by the device."""
        frame = self._build_frame(OP_READ, GRP_IMG, IMG_ID_STATE, {})
        rsp = await self._transact(frame, timeout=5.0)
        return rsp.get("images", [])
