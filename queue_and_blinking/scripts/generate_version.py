from pathlib import Path

from versioning import generate_header

Import("env")


project_dir = Path(env.subst("$PROJECT_DIR"))
firmware_version = env.GetProjectOption("fw_version", "1.0.0")
generate_header(project_dir, firmware_version)
