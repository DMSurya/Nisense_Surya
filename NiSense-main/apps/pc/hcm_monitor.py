"""
HCM Monitor — Entry Point
=========================
Qt QML application for monitoring the HCM wearable device over BLE.

Usage::

    python hcm_monitor.py [--device <address>] [--adapter <hciN>]

Platform notes:
  Windows: Requires Windows 10 version 1709 (Fall Creators Update) or later.
           Bleak uses the WinRT Bluetooth LE API.
  Linux:   Requires BlueZ 5.43+. Run as user in the `bluetooth` group:
               sudo usermod -aG bluetooth $USER
           Optionally pass --adapter hci1 for a specific adapter.

The QML UI is in the ``qml/`` sub-directory alongside this file.
Logs are written to ~/HCM_Logs/.

See ``README.md`` in this directory and ``docs/BLE_MONITOR_FLOW_GATES.md``.
"""

import argparse
import asyncio
import logging
import platform
import sys
import os
from pathlib import Path

# Must be set before Qt Quick Controls are loaded (allows Button customization in QML).
os.environ.setdefault("QT_QUICK_CONTROLS_STYLE", "Basic")

# Windows: Python 3.8+ uses a restricted DLL search path.
# Qt plugins (e.g. qtquickcontrols2plugin.dll) depend on other Qt DLLs that
# live inside the PySide6 package directory. We must register that directory
# via os.add_dll_directory() BEFORE importing PySide6 so the loader can find
# them.  Without this, loading any Quick Controls component fails with
# "The specified module could not be found."
if sys.platform == "win32":
    import importlib.util
    _pyside6_spec = importlib.util.find_spec("PySide6")
    if _pyside6_spec and _pyside6_spec.submodule_search_locations:
        for _pyside6_dir in _pyside6_spec.submodule_search_locations:
            try:
                os.add_dll_directory(_pyside6_dir)
            except (OSError, AttributeError):
                pass

from PySide6.QtGui import QGuiApplication, QIcon
from PySide6.QtQml import QQmlApplicationEngine, qmlRegisterType
from PySide6.QtQuick import QQuickWindow, QSGRendererInterface
from PySide6.QtQuickControls2 import QQuickStyle
from PySide6.QtCore import QUrl, QEventLoop, QTimer
import qasync

from hcm_backend import HCMBackend

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s  %(levelname)-8s  %(name)s: %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger(__name__)

QML_ROOT = Path(__file__).parent / "qml" / "main.qml"


def _platform_checks() -> None:
    """Warn early if the platform prerequisites are not met."""
    system = platform.system()
    if system == "Windows":
        import winreg
        try:
            # Require Windows 10 1709+ (build 16299)
            key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                                 r"SOFTWARE\Microsoft\Windows NT\CurrentVersion")
            build = int(winreg.QueryValueEx(key, "CurrentBuildNumber")[0])
            if build < 16299:
                log.warning("Windows build %d is below 16299 (1709); BLE may not work.", build)
        except Exception:
            pass
    elif system == "Linux":
        import subprocess
        try:
            result = subprocess.run(["groups"], capture_output=True, text=True)
            if "bluetooth" not in result.stdout:
                log.warning(
                    "Current user is not in the 'bluetooth' group. "
                    "BLE may fail with permission errors. "
                    "Run: sudo usermod -aG bluetooth $USER"
                )
        except Exception:
            pass


def main() -> None:
    parser = argparse.ArgumentParser(
        description="HCM BLE Monitor — PySide6/QML wearable monitoring app"
    )
    parser.add_argument(
        "--device", metavar="ADDRESS",
        help="BLE device address to connect to automatically on startup"
    )
    parser.add_argument(
        "--adapter", metavar="ADAPTER",
        help="BLE adapter (Linux only, e.g. hci1). Defaults to system default.",
        default=None,
    )
    parser.add_argument(
        "--debug", action="store_true",
        help="Verbose BLE logging (hcm_client, hcm_backend, winrt_pairing at DEBUG)",
    )
    args = parser.parse_args()

    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)
        for name in ("hcm_client", "hcm_backend", "winrt_pairing", "bleak"):
            logging.getLogger(name).setLevel(logging.DEBUG)

    _platform_checks()

    # Use platform default renderer: D3D11 on Windows, Metal on macOS, OpenGL on Linux.
    # Forcing OpenGLRhi on Windows causes an access violation (0xC0000005) if
    # the ANGLE/OpenGL driver isn't properly initialised.
    if sys.platform != "win32" and sys.platform != "darwin":
        QQuickWindow.setGraphicsApi(QSGRendererInterface.OpenGLRhi)

    # Style must be selected before QGuiApplication (see Qt Quick Controls docs).
    QQuickStyle.setStyle("Basic")

    app = QGuiApplication(sys.argv)
    app.setApplicationName("HCM Monitor")
    app.setApplicationDisplayName("HCM Monitor")

    # Set up asyncio event loop via qasync
    loop = qasync.QEventLoop(app)
    asyncio.set_event_loop(loop)

    # Create backend and expose to QML
    backend = HCMBackend(adapter=args.adapter)

    engine = QQmlApplicationEngine()
    engine.rootContext().setContextProperty("backend", backend)

    # Expose the log directory path so QML FolderListModel can browse it
    from hcm_logger import LOG_ROOT
    LOG_ROOT.mkdir(parents=True, exist_ok=True)  # ensure directory exists
    engine.rootContext().setContextProperty("logDirPath", QUrl.fromLocalFile(str(LOG_ROOT)))

    engine.load(QUrl.fromLocalFile(str(QML_ROOT)))
    if not engine.rootObjects():
        log.critical("Failed to load QML: %s", QML_ROOT)
        sys.exit(1)

    # Graceful BLE shutdown: block until notifications are stopped.  A fire-and-
    # forget create_task() lets WinRT deliver one more notify after qasync is gone
    # ("Signal source has been deleted").
    def _on_about_to_quit() -> None:
        log.info("App closing — shutting down BLE backend")
        backend.prepare_shutdown()
        try:
            shutdown_task = asyncio.ensure_future(backend.shutdown())
            nested = QEventLoop()

            def _finish(*_args) -> None:
                if nested.isRunning():
                    nested.quit()

            shutdown_task.add_done_callback(_finish)
            QTimer.singleShot(5000, nested.quit)
            nested.exec()

            if not shutdown_task.done():
                log.warning("Shutdown did not finish before timeout")
        except Exception as exc:
            log.debug("Shutdown during quit: %s", exc)

    app.aboutToQuit.connect(_on_about_to_quit)

    # Auto-connect if address supplied
    if args.device:
        async def _auto_connect():
            try:
                await asyncio.sleep(0.5)   # let QML finish rendering first
                await backend.connectToAddress(args.device)
            except Exception as exc:
                log.error("Startup connect failed: %s", exc, exc_info=True)
        loop.create_task(_auto_connect())
    else:
        async def _auto_connect_known():
            try:
                await asyncio.sleep(0.7)   # allow UI + BLE stack to settle
                await backend.autoConnectKnown()
            except Exception as exc:
                log.error("Startup auto-connect failed: %s", exc, exc_info=True)
        loop.create_task(_auto_connect_known())

    with loop:
        sys.exit(loop.run_forever())


if __name__ == "__main__":
    main()
