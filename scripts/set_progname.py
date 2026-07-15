import re
import shutil
from pathlib import Path

Import("env")


def _extract_app_version() -> str:
    try:
        build_flags = env.GetProjectOption("build_flags", [])
        if isinstance(build_flags, str):
            build_flags = [build_flags]

        pattern = re.compile(r'APP_VERSION\s*=\s*"([^"]+)"')
        for flag in build_flags:
            normalized = str(flag).replace(r'\"', '"')
            match = pattern.search(normalized)
            if match:
                return match.group(1)
    except Exception:
        pass

    try:
        ini_path = Path(env.subst("$PROJECT_DIR")) / "platformio.ini"
        ini_text = ini_path.read_text(encoding="utf-8")
        match = re.search(r'-DAPP_VERSION=\\?"([^"\r\n]+)\\?"', ini_text)
        if match:
            return match.group(1)
    except Exception:
        pass

    # Fallback for environments where APP_VERSION is already resolved into CPPDEFINES.
    cppdefines = env.get("CPPDEFINES", [])
    for define in cppdefines:
        if isinstance(define, tuple) and len(define) >= 2 and define[0] == "APP_VERSION":
            value = str(define[1]).strip('"')
            return value
        if define == "APP_VERSION":
            return "dev"
    return "dev"


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
