import subprocess
from pathlib import Path

from SCons.Script import DefaultEnvironment


def _is_populated(path: Path) -> bool:
    if not path.exists() or not path.is_dir():
        return False
    for entry in path.iterdir():
        if entry.name in {".git", ".DS_Store"}:
            continue
        return True
    return False


def _update_submodules(project_dir: Path, rel_paths: list[str]) -> None:
    cmd = ["git", "submodule", "update", "--init", "--recursive", *rel_paths]
    subprocess.check_call(cmd, cwd=str(project_dir))


env = DefaultEnvironment()
project_dir = Path(env["PROJECT_DIR"]).resolve()

submodules = [
    "components/Adafruit-GFX",
    "components/CalEPD",
    "third_party/libtropic",
]

missing = []
for rel in submodules:
    if not _is_populated(project_dir / rel):
        missing.append(rel)

if missing:
    print("[pio_submodules] Missing submodule contents:")
    for rel in missing:
        print(f"  - {rel}")
    print("[pio_submodules] Initializing submodules (pinned revisions)...")
    _update_submodules(project_dir, missing)
