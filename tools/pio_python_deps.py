import subprocess
import sys
from importlib.util import find_spec

from SCons.Script import DefaultEnvironment

# Python modules required at build time inside the PlatformIO penv.
# Key: importable module name. Value: pip requirement specifier.
REQUIRED_MODULES = {
    "intelhex": "intelhex>=2.3",
}


def _ensure_module(module_name: str, requirement: str) -> None:
    if find_spec(module_name) is not None:
        return
    print(f"[pio_python_deps] Installing missing Python dependency: {requirement}")
    subprocess.check_call(
        [
            sys.executable,
            "-m",
            "pip",
            "install",
            "--disable-pip-version-check",
            "--quiet",
            requirement,
        ]
    )


DefaultEnvironment()

for module_name, requirement in REQUIRED_MODULES.items():
    _ensure_module(module_name, requirement)
