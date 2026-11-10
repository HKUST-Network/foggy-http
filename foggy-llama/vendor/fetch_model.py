#!/usr/bin/env python3
"""Download the course GGUF pinned by revision, size and SHA-256."""
from pathlib import Path
import hashlib
import json
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def validate(path, metadata):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    if path.stat().st_size != metadata["bytes"] or digest.hexdigest() != metadata["sha256"]:
        raise ValueError(f"Model size or SHA-256 mismatch: {path}")


def main():
    metadata = json.loads((ROOT / "vendor/model.json").read_text())
    target = ROOT / "models" / metadata["filename"]
    target.parent.mkdir(exist_ok=True)
    if target.exists():
        validate(target, metadata)
    else:
        partial = target.with_suffix(".download")
        try:
            subprocess.run(["curl", "-fL", "--retry", "2", metadata["url"], "-o", str(partial)], check=True)
            validate(partial, metadata)
            partial.replace(target)
        except Exception:
            partial.unlink(missing_ok=True)
            raise
    print(target)


if __name__ == "__main__":
    main()
