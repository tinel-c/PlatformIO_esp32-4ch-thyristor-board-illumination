"""
PlatformIO post-action: OTA-flash the built firmware.bin via the monorepo fleet tool.

When this device repo is checked out as
device/PlatformIO_esp32-4ch-thyristor-board-illumination inside
ServerBootShutdownManagemement, a successful build calls:

  python scripts/status/device_fleet.py flash --id ... --bin firmware.bin

A standalone clone of this firmware repo skips OTA and prints the .bin path.
Offline / missing IP is a soft success (exit 0). A failed espota to a live
device fails the post step. Set SKIP_FLEET_OTA=1 to leave the .bin without
calling the fleet tool (UART flash of an OTA fix, etc.).
"""

Import("env")  # type: ignore[name-defined]  # noqa: F821 — PlatformIO injects this

import os
import shutil
import subprocess
import sys
from pathlib import Path

DEVICE_ID = "PlatformIO_esp32-4ch-thyristor-board-illumination"


def _is_py_launcher(path: str) -> bool:
    name = Path(path).name.lower()
    return name in ("py", "py.exe")


def _python_cmd() -> list:
    """Prefer a system Python that already has PyYAML/paho (not only PIO penv)."""
    for candidate in (
        shutil.which("py"),
        shutil.which("python"),
        shutil.which("python3"),
        sys.executable,
    ):
        if not candidate:
            continue
        probe = [candidate]
        if _is_py_launcher(candidate):
            probe = [candidate, "-3"]
        probe.extend(["-c", "import yaml, paho.mqtt.client"])
        try:
            completed = subprocess.run(
                probe,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                check=False,
            )
            if completed.returncode == 0:
                return probe[:-2]  # drop the -c import probe args
        except OSError:
            continue
    return [sys.executable]


def after_build(source, target, env):  # noqa: ARG001
    firmware = Path(str(target[0]))
    if os.environ.get("SKIP_FLEET_OTA") == "1":
        print("ota_after_build: SKIP_FLEET_OTA=1 — leaving {}".format(firmware))
        return
    # PROJECT_DIR is device/<name> when this is a monorepo submodule.
    project_dir = Path(env["PROJECT_DIR"]).resolve()
    monorepo = project_dir.parent.parent
    fleet = monorepo / "scripts" / "status" / "device_fleet.py"
    if not fleet.is_file():
        print(
            "ota_after_build: fleet tool not found at {}; "
            "leaving {} for a later flash".format(fleet, firmware)
        )
        return

    cmd = _python_cmd()
    cmd.extend(
        [
            str(fleet),
            "flash",
            "--id",
            DEVICE_ID,
            "--bin",
            str(firmware),
        ]
    )
    print("ota_after_build:", " ".join(cmd))
    completed = subprocess.run(cmd, cwd=str(monorepo), check=False)
    if completed.returncode != 0:
        sys.exit(completed.returncode)


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", after_build)  # type: ignore[name-defined]
