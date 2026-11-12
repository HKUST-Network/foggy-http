#!/usr/bin/env python3
"""Package the two CP4 source files and their checksums."""
import hashlib
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parent
FILES = {
    "foggy_http.cc": ROOT.parent / "foggy-http/src/foggy_http.cc",
    "llama_routes.cc": ROOT / "src/llama_routes.cc",
}


def main():
    try:
        sources = {name: path.read_bytes() for name, path in FILES.items()}
    except OSError as error:
        raise SystemExit(str(error)) from error

    checksums = "".join(
        f"{hashlib.sha256(data).hexdigest()}  {name}\n"
        for name, data in sources.items()
    )
    output = ROOT / "submit.zip"
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
        for name, data in sources.items():
            archive.writestr(name, data)
        archive.writestr("SHA256SUMS", checksums)
    print(output)


if __name__ == "__main__":
    main()
