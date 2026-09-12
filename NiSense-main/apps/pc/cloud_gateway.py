"""
Cloud gateway — optional upload of HCM Monitor CSV session logs.

Supports REST POST (stdlib only) and optional MQTT publish when paho-mqtt is installed.
Gateway endpoints are read from ~/HCM_Logs/gateway.json or passed explicitly.
"""

from __future__ import annotations

import json
import logging
import urllib.error
import urllib.request
from pathlib import Path
from typing import Optional

log = logging.getLogger(__name__)

GATEWAY_CONFIG_NAME = "gateway.json"


def gateway_config_path(log_root: Path) -> Path:
    return log_root / GATEWAY_CONFIG_NAME


def load_gateway_config(log_root: Path) -> dict:
    path = gateway_config_path(log_root)
    if not path.exists():
        return {}
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:
        log.warning("Could not read %s: %s", path, exc)
        return {}


def save_gateway_config(log_root: Path, cfg: dict) -> None:
    log_root.mkdir(parents=True, exist_ok=True)
    gateway_config_path(log_root).write_text(
        json.dumps(cfg, indent=2) + "\n", encoding="utf-8"
    )


def upload_csv_rest(
    url: str,
    csv_path: Path,
    *,
    device_id: str = "HCM",
    timeout_sec: float = 30.0,
) -> tuple[bool, str]:
    """POST a CSV file body to a REST ingest endpoint."""
    if not csv_path.is_file():
        return False, f"File not found: {csv_path}"

    data = csv_path.read_bytes()
    req = urllib.request.Request(url, data=data, method="POST")
    req.add_header("Content-Type", "text/csv; charset=utf-8")
    req.add_header("X-Device-Id", device_id)
    req.add_header("X-Filename", csv_path.name)

    try:
        with urllib.request.urlopen(req, timeout=timeout_sec) as resp:
            code = resp.getcode()
            if 200 <= code < 300:
                return True, f"HTTP {code}"
            return False, f"HTTP {code}"
    except urllib.error.HTTPError as exc:
        return False, f"HTTP {exc.code}: {exc.reason}"
    except Exception as exc:
        return False, str(exc)


def upload_csv_mqtt(
    csv_path: Path,
    *,
    host: str,
    port: int = 1883,
    topic_prefix: str = "hcm/logs",
    device_id: str = "HCM",
    username: Optional[str] = None,
    password: Optional[str] = None,
    use_ssl: bool = False,
) -> tuple[bool, str]:
    """Publish CSV contents to an MQTT topic (requires paho-mqtt)."""
    try:
        import paho.mqtt.client as mqtt  # type: ignore
    except ImportError:
        return False, "paho-mqtt not installed (pip install paho-mqtt)"

    if not csv_path.is_file():
        return False, f"File not found: {csv_path}"

    topic = f"{topic_prefix.rstrip('/')}/{device_id}/{csv_path.stem}"
    payload = csv_path.read_bytes()

    client = mqtt.Client(client_id=f"hcm_monitor_{device_id}")
    if username:
        client.username_pw_set(username, password or None)

    try:
        client.connect(host, port, keepalive=30)
        client.loop_start()
        info = client.publish(topic, payload=payload, qos=1, retain=False)
        info.wait_for_publish(timeout=10.0)
        client.loop_stop()
        client.disconnect()
        if info.rc == mqtt.MQTT_ERR_SUCCESS:
            return True, topic
        return False, f"MQTT rc={info.rc}"
    except Exception as exc:
        try:
            client.loop_stop()
            client.disconnect()
        except Exception:
            pass
        return False, str(exc)


def upload_session_files(
    log_root: Path,
    paths: list[Path],
    *,
    device_id: str = "HCM",
    config: Optional[dict] = None,
) -> list[tuple[Path, bool, str]]:
    """Upload multiple CSV/log files using gateway.json settings."""
    cfg = config if config is not None else load_gateway_config(log_root)
    rest_url = (cfg.get("rest_url") or "").strip()
    mqtt_host = (cfg.get("mqtt_host") or "").strip()
    results: list[tuple[Path, bool, str]] = []

    for path in paths:
        path = Path(path)
        if rest_url:
            ok, msg = upload_csv_rest(rest_url, path, device_id=device_id)
            results.append((path, ok, msg))
            continue
        if mqtt_host:
            ok, msg = upload_csv_mqtt(
                path,
                host=mqtt_host,
                port=int(cfg.get("mqtt_port") or 1883),
                topic_prefix=str(cfg.get("mqtt_topic_prefix") or "hcm/logs"),
                device_id=device_id,
                username=cfg.get("mqtt_username") or None,
                password=cfg.get("mqtt_password") or None,
                use_ssl=bool(cfg.get("use_ssl")),
            )
            results.append((path, ok, msg))
            continue
        results.append((path, False, "No gateway configured (rest_url or mqtt_host)"))

    return results
