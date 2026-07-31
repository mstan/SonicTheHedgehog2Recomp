#!/usr/bin/env python3
"""Fetch the Android native dependencies at reviewed, reproducible revisions."""

from __future__ import annotations

import hashlib
import shutil
import subprocess
import sys
import tarfile
import urllib.request
from pathlib import Path


ANDROID_ROOT = Path(__file__).resolve().parents[1]
EXTERNAL = ANDROID_ROOT / "external"

SDL_VERSION = "2.28.5"
SDL_ARCHIVE = EXTERNAL / f"SDL2-{SDL_VERSION}.tar.gz"
SDL_DIR = EXTERNAL / f"SDL2-{SDL_VERSION}"
SDL_URL = (
    "https://github.com/libsdl-org/SDL/releases/download/"
    f"release-{SDL_VERSION}/SDL2-{SDL_VERSION}.tar.gz"
)
SDL_SHA256 = "332cb37d0be20cb9541739c61f79bae5a477427d79ae85e352089afdaf6666e4"

UCONTEXT_DIR = EXTERNAL / "libucontext"
UCONTEXT_URL = "https://github.com/kaniini/libucontext.git"
UCONTEXT_COMMIT = "49e671dd52ff6791295d8161ad3b6da7dc5f6f9d"
THIRD_PARTY_NOTICE = (
    ANDROID_ROOT / "app" / "src" / "main" / "assets"
    / "THIRD-PARTY-LICENSES.txt"
)


def run(*args: str) -> None:
    subprocess.run(args, check=True)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def fetch_sdl() -> None:
    if SDL_DIR.is_dir():
        required = [
            SDL_DIR / "CMakeLists.txt",
            SDL_DIR / "include" / "SDL.h",
            SDL_DIR / "src" / "SDL.c",
        ]
        missing = [path for path in required if not path.is_file()]
        if missing:
            raise RuntimeError(
                f"SDL2 directory is incomplete (missing {missing[0]})"
            )
        print(f"SDL2 {SDL_VERSION}: already present")
        return

    if not SDL_ARCHIVE.exists():
        print(f"SDL2 {SDL_VERSION}: downloading {SDL_URL}")
        partial = SDL_ARCHIVE.with_suffix(SDL_ARCHIVE.suffix + ".part")
        try:
            with urllib.request.urlopen(SDL_URL) as response, partial.open("wb") as out:
                shutil.copyfileobj(response, out)
            partial.replace(SDL_ARCHIVE)
        finally:
            partial.unlink(missing_ok=True)

    actual = sha256(SDL_ARCHIVE)
    if actual != SDL_SHA256:
        raise RuntimeError(
            f"SDL archive SHA-256 mismatch: expected {SDL_SHA256}, got {actual}"
        )

    print(f"SDL2 {SDL_VERSION}: extracting verified archive")
    external_resolved = EXTERNAL.resolve()
    with tarfile.open(SDL_ARCHIVE, "r:gz") as archive:
        members = archive.getmembers()
        for member in members:
            destination = (EXTERNAL / member.name).resolve()
            if destination != external_resolved and external_resolved not in destination.parents:
                raise RuntimeError(f"unsafe archive path: {member.name}")
            if not (member.isfile() or member.isdir()):
                raise RuntimeError(f"unsupported archive entry: {member.name}")
        if hasattr(tarfile, "data_filter"):
            archive.extractall(EXTERNAL, members=members, filter="data")
        else:
            archive.extractall(EXTERNAL, members=members)

    if not SDL_DIR.is_dir():
        raise RuntimeError(f"SDL archive did not create {SDL_DIR}")


def fetch_libucontext() -> None:
    freshly_cloned = False
    if not UCONTEXT_DIR.exists():
        print("libucontext: cloning")
        run("git", "clone", "--no-checkout", UCONTEXT_URL, str(UCONTEXT_DIR))
        freshly_cloned = True
    elif not (UCONTEXT_DIR / ".git").exists():
        raise RuntimeError(f"{UCONTEXT_DIR} exists but is not a Git checkout")

    if not freshly_cloned:
        status = subprocess.run(
            ["git", "-C", str(UCONTEXT_DIR), "status", "--porcelain"],
            check=True,
            capture_output=True,
            text=True,
        ).stdout
        if status:
            raise RuntimeError(
                "libucontext checkout has local changes; refusing to replace it"
            )

    actual = subprocess.run(
        ["git", "-C", str(UCONTEXT_DIR), "rev-parse", "HEAD"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    if freshly_cloned or actual != UCONTEXT_COMMIT:
        run(
            "git",
            "-C",
            str(UCONTEXT_DIR),
            "fetch",
            "--depth",
            "1",
            "origin",
            UCONTEXT_COMMIT,
        )
        run("git", "-C", str(UCONTEXT_DIR), "checkout", "--detach", UCONTEXT_COMMIT)
        actual = subprocess.run(
            ["git", "-C", str(UCONTEXT_DIR), "rev-parse", "HEAD"],
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()
    if actual != UCONTEXT_COMMIT:
        raise RuntimeError(f"libucontext pin mismatch: expected {UCONTEXT_COMMIT}, got {actual}")
    license_text = (UCONTEXT_DIR / "LICENSE").read_text(encoding="utf-8")
    notice_text = THIRD_PARTY_NOTICE.read_text(encoding="utf-8")
    if not notice_text.endswith(license_text):
        raise RuntimeError(
            "packaged libucontext notice does not contain the pinned LICENSE verbatim"
        )
    print(f"libucontext: pinned at {actual[:12]}")


def main() -> int:
    EXTERNAL.mkdir(parents=True, exist_ok=True)
    fetch_sdl()
    fetch_libucontext()
    print("Android dependencies are ready.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"setup_deps.py: {error}", file=sys.stderr)
        raise SystemExit(1)
