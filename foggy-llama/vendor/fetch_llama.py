#!/usr/bin/env python3
"""Install the pinned llama.cpp runtime libraries beside foggy-server."""
import json
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import sys
import tarfile
from tempfile import TemporaryDirectory
from fetch_model import download


LIBRARY = re.compile(r"lib(?:llama|ggml(?:-[\w.-]+)?)(?:\.so(?:\.\d+)*|(?:\.\d+)*\.dylib)")


def extract_libraries(bundle, directory, tag, extension):
    with tarfile.open(bundle, "r:gz") as archive:
        members = {}
        for member in archive.getmembers():
            name = PurePosixPath(member.name).name
            if member.name == f"llama-{tag}/{name}" and LIBRARY.fullmatch(name):
                members[name] = member

        required = {f"lib{name}{extension}" for name in ("llama", "ggml", "ggml-base")}
        if not required <= members.keys():
            raise ValueError("llama.cpp release is missing required libraries")
        for name, member in members.items():
            target = directory / name
            if member.isfile():
                with archive.extractfile(member) as source, target.open("wb") as output:
                    shutil.copyfileobj(source, output)
            elif member.issym() and member.linkname in members:
                target.symlink_to(member.linkname)
            else:
                raise ValueError(f"Unexpected library entry in llama.cpp release: {member.name}")
        if any(not (directory / name).is_file() for name in members):
            raise ValueError("llama.cpp release has an unresolved library link")
    return sorted(members)


def main(directory):
    system = {"Darwin": "macos", "Linux": "ubuntu"}.get(platform.system())
    arch = {"arm64": "arm64", "aarch64": "arm64", "x86_64": "x64"}.get(platform.machine())
    release = json.loads(Path(__file__).with_name("llama-release.json").read_text())
    checksum = release["sha256"].get(f"{system}-{arch}")
    if checksum is None:
        raise ValueError(f"No course llama.cpp build for {platform.system()} {platform.machine()}")
    tag = release["tag"]
    asset = f"llama-{tag}-bin-{system}-{arch}.tar.gz"
    stamp = directory / ".llama-runtime"
    try:
        installed = json.loads(stamp.read_text())
    except (FileNotFoundError, ValueError):
        installed = {}
    libraries = installed.get("libraries", [])
    if (installed.get("sha256") == checksum and libraries and
            all(LIBRARY.fullmatch(name) and (directory / name).is_file() for name in libraries)):
        return

    directory.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(prefix=".llama-", dir=directory) as temporary:
        staging = Path(temporary)
        bundle = staging / asset
        url = f"https://github.com/ggml-org/llama.cpp/releases/download/{tag}/{asset}"
        print(f"Downloading llama.cpp {tag} for {system}-{arch}", flush=True)
        download(url, bundle, checksum)

        names = extract_libraries(bundle, staging, tag, ".dylib" if system == "macos" else ".so")
        for name in names:
            (staging / name).replace(directory / name)
        for name in set(libraries) - set(names):
            if LIBRARY.fullmatch(name):
                (directory / name).unlink(missing_ok=True)
        temporary_stamp = staging / stamp.name
        temporary_stamp.write_text(json.dumps({"sha256": checksum, "libraries": names}) + "\n")
        temporary_stamp.replace(stamp)


if __name__ == "__main__":
    try:
        main(Path(sys.argv[1]))
    except (OSError, ValueError, tarfile.TarError) as error:
        raise SystemExit(str(error)) from None
