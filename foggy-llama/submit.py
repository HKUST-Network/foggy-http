#!/usr/bin/env python3
"""Package only the editable CP4 files and their checksums."""
from pathlib import Path
import hashlib
import zipfile

ROOT = Path(__file__).resolve().parent
FILES = ("router.cpp", "llama_routes.cpp", "rate_limiter.cpp")


def write_submission(source, output):
    payloads = {name: (source / name).read_bytes() for name in FILES}
    sums = "".join(f"{hashlib.sha256(data).hexdigest()}  {name}\n" for name, data in payloads.items())
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
        for name, data in payloads.items():
            archive.writestr(name, data)
        archive.writestr("SHA256SUMS", sums)


if __name__ == "__main__":
    output = ROOT / "submit.zip"
    write_submission(ROOT / "src", output)
    print(output)
