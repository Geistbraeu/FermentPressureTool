import re
import shutil
from pathlib import Path

Import("env")


def _extract_app_version() -> str:
    header_path = Path(env.subst("$PROJECT_DIR")) / "include" / "build_info.h"
    header_text = header_path.read_text(encoding="utf-8")
    match = re.search(r'kFirmwareVersion\[\]\s*=\s*"([^"]+)"', header_text)
    if not match:
        raise ValueError(f"Firmware version not found in {header_path}")
    return match.group(1)


version = _extract_app_version()
version = re.sub(r"[^0-9A-Za-z._-]", "_", version)
if not version:
    version = "dev"

env.Replace(PROGNAME="firmware")


def _copy_versioned_firmware_bin(source, target, env):
    source_path = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    target_path = source_path.replace("firmware.bin", f"firmware_{version}.bin")
    if source_path == target_path:
        return

    shutil.copy2(source_path, target_path)


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", _copy_versioned_firmware_bin)
