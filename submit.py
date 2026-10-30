#!/usr/bin/env python3
"""Package the editable sources after comparing their SHA-256 with the build."""
import argparse
import hashlib
from pathlib import Path
import re
import zipfile

ROOT = Path(__file__).resolve().parent
FILES = {'foggy_http.cc': 'foggy-http/src/foggy_http.cc',
         'nginx-stream.conf': 'foggy-http/conf/nginx-stream.conf'}


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def manifest(payloads):
    return ''.join(f'{sha256(payloads[name])}  {name}\n' for name in FILES).encode()


def parse_manifest(data):
    sums = {}
    for line in data.decode('ascii').splitlines():
        match = re.fullmatch(r'([0-9a-f]{64})  (foggy_http\.cc|nginx-stream\.conf)', line)
        if not match or match[2] in sums:
            raise ValueError('Invalid SHA256SUMS')
        sums[match[2]] = match[1]
    if set(sums) != set(FILES):
        raise ValueError('SHA256SUMS must identify exactly the two editable files')
    return sums


def check(payloads, sums):
    for name in FILES:
        if sha256(payloads[name]) != sums[name]:
            raise ValueError(f'SHA-256 mismatch: {name}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', default='submit.zip')
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--verify', metavar='ZIP', help='compare ZIP, current files, and build')
    group.add_argument('--record-build', action='store_true', help='called by make after linking')
    args = parser.parse_args()
    try:
        build = ROOT/'build'
        current = {name: (ROOT/path).read_bytes() for name, path in FILES.items()}
        snapshots = {name: (build/name).read_bytes() for name in FILES}
        for name in ('http.o', 'date-client', 'date-server', 'echo-client', 'echo-server'):
            if not (build/name).is_file():
                raise ValueError(f'Missing build output: {name}')
        if args.record_build:
            sums = {name: sha256(data) for name, data in snapshots.items()}
            check(current, sums)
            (build/'SHA256SUMS').write_bytes(manifest(snapshots))
            print((build/'SHA256SUMS').read_text(), end='')
            return
        sums = parse_manifest((build/'SHA256SUMS').read_bytes())
        check(current, sums)
        check(snapshots, sums)
        if args.verify:
            output = ROOT/args.verify
            with zipfile.ZipFile(output) as archive:
                if len(archive.infolist()) != 3 or set(archive.namelist()) != {*FILES, 'SHA256SUMS'}:
                    raise ValueError('ZIP must contain exactly two editable files and SHA256SUMS')
                archived_sums = parse_manifest(archive.read('SHA256SUMS'))
                check({name: archive.read(name) for name in FILES}, archived_sums)
                if archived_sums != sums:
                    raise ValueError('ZIP differs from current sources/build')
        else:
            output = ROOT/args.output
            with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as archive:
                for name, data in current.items():
                    archive.writestr(name, data)
                archive.writestr('SHA256SUMS', manifest(current))
        print(manifest(current).decode(), end='')
        print(f'ZIP SHA-256: {sha256(output.read_bytes())}')
        print(output)
    except (OSError, ValueError, UnicodeError, KeyError, zipfile.BadZipFile) as error:
        parser.exit(1, f'{error}\nRun make and repeat your checks before packaging.\n')


if __name__ == '__main__':
    main()
