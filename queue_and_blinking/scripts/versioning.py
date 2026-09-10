from datetime import datetime, timezone
from pathlib import Path
import subprocess


def run_git(project_dir, *arguments):
    return subprocess.run(
        ["git", *arguments],
        cwd=project_dir,
        check=True,
        text=True,
        capture_output=True,
    )


def git_description(project_dir):
    try:
        return run_git(project_dir, "describe", "--always").stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def generate_header(project_dir, version, git_hash=None):
    header = Path(project_dir) / "src" / "version.h"
    git_hash = git_hash or git_description(project_dir)
    version_line = f'#define FW_VERSION "{version}"'
    git_hash_line = f'#define FW_GIT_HASH "{git_hash}"'

    if header.exists():
        current_lines = header.read_text(encoding="ascii").splitlines()
        if len(current_lines) == 3 and current_lines[:2] == [version_line, git_hash_line]:
            return

    build_utc = datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")
    header.write_text(
        f"{version_line}\n"
        f"{git_hash_line}\n"
        f'#define FW_BUILD_UTC "{build_utc}"\n',
        encoding="ascii",
    )
