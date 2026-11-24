"""Build the shared engine's software-only Clone Driver for this game."""
from pathlib import Path
import argparse
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--engine", type=Path,
                        default=Path(__file__).resolve().parents[2] / "segagenesisrecomp")
    args, remaining = parser.parse_known_args()
    builder = args.engine.resolve() / "tools/clone_driver/build.py"
    profile = Path(__file__).with_name("profile.json")
    return subprocess.call([sys.executable, "-B", str(builder), "--profile",
                            str(profile), *remaining])


if __name__ == "__main__":
    sys.exit(main())
