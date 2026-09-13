#!/usr/bin/env python3
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[1]
makefile = (root / "Makefile").read_text(encoding="utf-8")
version_h = (root / "include" / "version.h").read_text(encoding="utf-8")
readme = (root / "README.md").read_text(encoding="utf-8")
workflow = (root / ".github" / "workflows" / "build.yml").read_text(encoding="utf-8")
release_workflow = (root / ".github" / "workflows" / "release.yml").read_text(encoding="utf-8")


def capture(pattern: str, text: str, label: str) -> str:
    match = re.search(pattern, text, re.MULTILINE)
    if not match:
        raise SystemExit(f"version-sync: cannot find {label}")
    return match.group(1)


make_version = capture(r"^VERSION\s*:=\s*([0-9]+\.[0-9]+\.[0-9]+)\s*$", makefile, "Makefile VERSION")
header_version = capture(r'^#define\s+ORION_VERSION\s+"([^"]+)"\s*$', version_h, "ORION_VERSION")

errors = []
if make_version != header_version:
    errors.append(f"Makefile VERSION={make_version} but ORION_VERSION={header_version}")

expected = f"v{make_version}"
if expected not in readme:
    errors.append(f"README.md does not mention current version {expected}")
if f"UN_Orion-v{make_version}-install.iso" not in readme:
    errors.append("README.md installer filename is stale")
if f"UN_Orion-v{make_version}-multi-firmware-media" not in workflow:
    errors.append("build workflow artifact name is stale")
if f"UN_Orion-v{make_version}-install.iso" not in workflow:
    errors.append("build workflow installer path is stale")
if f"UN_Orion-v{make_version}-i686-bios.img" not in workflow:
    errors.append("build workflow i686 path is stale")
if f"UN_Orion {make_version} alive" not in workflow:
    errors.append("build workflow boot-version assertion is stale")

release_required = [
    'test "v$version" = "$GITHUB_REF_NAME"',
    'gh release create "$GITHUB_REF_NAME"',
    'SHA256SUMS.txt',
]
for snippet in release_required:
    if snippet not in release_workflow:
        errors.append(f"generic release workflow is missing: {snippet}")
if re.search(r"UN_Orion-v\d+\.\d+\.\d+", release_workflow):
    errors.append("generic release workflow contains a hard-coded media version")

if errors:
    for error in errors:
        print(f"version-sync: {error}", file=sys.stderr)
    raise SystemExit(1)

print(f"version-sync: {make_version} is consistent")
