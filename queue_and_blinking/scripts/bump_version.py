import argparse
from pathlib import Path
import re

from versioning import generate_header, run_git


VERSION_PATTERN = re.compile(r"^(\d+)\.(\d+)\.(\d+)$")
CONFIG_VERSION_PATTERN = re.compile(r"(?m)^(fw_version\s*=\s*)(\d+\.\d+\.\d+)\s*$")


def read_version(project_dir):
    config = (project_dir / "platformio.ini").read_text(encoding="ascii")
    matches = list(CONFIG_VERSION_PATTERN.finditer(config))
    if len(matches) != 1:
        raise RuntimeError("expected exactly one fw_version entry in platformio.ini")
    return config, matches[0]


def bump(version, component):
    match = VERSION_PATTERN.fullmatch(version)
    if match is None:
        raise RuntimeError(f"fw_version must use MAJOR.MINOR.PATCH format: {version}")

    major, minor, patch = (int(value) for value in match.groups())
    if component == "major":
        major, minor, patch = major + 1, 0, 0
    elif component == "minor":
        minor, patch = minor + 1, 0
    else:
        patch += 1
    return f"{major}.{minor}.{patch}"


def main():
    parser = argparse.ArgumentParser(
        description="Bump fw_version, regenerate and commit version files, and create a Git tag."
    )
    component = parser.add_mutually_exclusive_group()
    component.add_argument("--minor", action="store_true", help="bump the minor version")
    component.add_argument("--major", action="store_true", help="bump the major version")
    args = parser.parse_args()

    project_dir = Path(__file__).resolve().parent.parent
    config, version_entry = read_version(project_dir)
    component_name = "major" if args.major else "minor" if args.minor else "patch"
    new_version = bump(version_entry.group(2), component_name)
    tag = f"v{new_version}"

    if run_git(project_dir, "tag", "--list", tag).stdout.strip():
        raise RuntimeError(f"Git tag already exists: {tag}")

    updated_config = config[:version_entry.start(2)] + new_version + config[version_entry.end(2):]
    (project_dir / "platformio.ini").write_text(updated_config, encoding="ascii")

    generate_header(project_dir, new_version, tag)
    run_git(project_dir, "add", "--", "platformio.ini", "src/version.h")
    run_git(
        project_dir,
        "commit",
        "--only",
        "-m",
        f"Bump version to {new_version}",
        "--",
        "platformio.ini",
        "src/version.h",
    )
    run_git(project_dir, "tag", "-a", tag, "-m", f"Release {tag}")
    print(f"Bumped version to {new_version}, committed version files, and created annotated tag {tag}")


if __name__ == "__main__":
    main()
