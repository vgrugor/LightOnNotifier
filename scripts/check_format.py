#!/usr/bin/env python3
"""Check tracked-style C++ sources without inspecting the local credential file."""

import argparse
from pathlib import Path
import re
import shutil
import subprocess
import sys


FORMATTER_VERSION = "18.1.8"
REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
LOCAL_CONFIGURATION = REPOSITORY_ROOT / "src/infrastructure/env.cpp"


def source_files():
    files = []
    for directory in ("include", "src", "test"):
        for path in (REPOSITORY_ROOT / directory).rglob("*"):
            if path == LOCAL_CONFIGURATION or not path.is_file():
                continue
            if path.suffix in (".cpp", ".h", ".hpp") or path.name == "env.cpp.example":
                files.append(path)
    return sorted(files)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fix", action="store_true", help="Format the checked files in place")
    parser.add_argument("--formatter", default="clang-format", help="Path to clang-format")
    arguments = parser.parse_args()
    formatter = shutil.which(arguments.formatter)
    if formatter is None:
        print("Install requirements-dev.txt to provide clang-format.", file=sys.stderr)
        return 2
    version = subprocess.run([formatter, "--version"], capture_output=True, text=True, check=True)
    if not re.search(r"\bversion " + re.escape(FORMATTER_VERSION) + r"\b", version.stdout):
        print("Expected clang-format " + FORMATTER_VERSION + ": " + version.stdout.strip(),
              file=sys.stderr)
        return 2
    files = source_files()
    if not files:
        print("No C++ files found.", file=sys.stderr)
        return 2
    options = ["-i"] if arguments.fix else ["--dry-run", "--Werror"]
    result = subprocess.run([formatter, "--style=file"] + options + [str(path) for path in files],
                            cwd=REPOSITORY_ROOT)
    if result.returncode == 0:
        print("Formatted" if arguments.fix else "Formatting passed", "for", len(files), "files.")
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
