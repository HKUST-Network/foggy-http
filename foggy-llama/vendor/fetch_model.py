#!/usr/bin/env python3
"""Download the course GGUF pinned by revision, size and SHA-256."""
from pathlib import Path
import hashlib
from http.client import HTTPException
import json
import shutil
from tempfile import TemporaryDirectory
from urllib.request import urlopen

ROOT = Path(__file__).resolve().parents[1]


def validate(path, checksum, size=None):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    if (size is not None and path.stat().st_size != size) or digest.hexdigest() != checksum:
        raise ValueError(f"Size or SHA-256 mismatch: {path.name}")


def download(url, target, checksum, size=None):
    for attempt in range(3):
        try:
            with urlopen(url, timeout=30) as response, target.open("wb") as output:
                shutil.copyfileobj(response, output, 1024 * 1024)
            validate(target, checksum, size)
            return
        except (OSError, HTTPException, ValueError) as error:
            if attempt == 2:
                raise OSError(f"Download failed for {target.name}: {error}") from error


def main():
    metadata = json.loads((ROOT / "vendor/model.json").read_text())
    target = ROOT / "models" / metadata["filename"]
    target.parent.mkdir(exist_ok=True)
    if target.exists():
        validate(target, metadata["sha256"], metadata["bytes"])
    else:
        with TemporaryDirectory(prefix=".model-", suffix=".download", dir=target.parent) as directory:
            partial = Path(directory) / target.name
            print(f"Downloading course model {target.name}", flush=True)
            download(metadata["url"], partial, metadata["sha256"], metadata["bytes"])
            partial.replace(target)
    print(target)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        raise SystemExit(str(error)) from None
