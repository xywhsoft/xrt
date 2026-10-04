"""Fetch and verify the fixed Linux x86_64 Hydra SQLite test runtime."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import platform
import shutil
import sys
import tarfile
import urllib.request

from hydra_test_inputs import sha256

VERSION = 'v2.3.0'
URL = 'https://github.com/ory/hydra/releases/download/v2.3.0/hydra_2.3.0-linux_sqlite_64bit.tar.gz'
ARCHIVE_SHA256 = '9a05dca5076427d18f6a57bc58542c75989a0e37a605c85962ffc8a7cc3ba707'
BINARY_SHA256 = 'd34aa4c8f89ca0cf70d29a37506041c542c8351e5852e5a2dcc206134ddac127'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=Path('out/hydra-runtime'))
    parser.add_argument('--archive', type=Path, help='use an already downloaded archive with the same pinned checksum')
    args = parser.parse_args()
    if sys.platform != 'linux' or platform.machine() not in {'x86_64', 'amd64'}:
        parser.error('this fixed runtime requires Linux x86_64')
    output = args.output_dir.resolve(); output.mkdir(parents=True, exist_ok=False)
    archive = output / 'hydra.tar.gz'
    if args.archive:
        shutil.copyfile(args.archive.resolve(), archive)
    else:
        with urllib.request.urlopen(URL, timeout=60) as response, archive.open('xb') as stream:
            total = 0
            while chunk := response.read(1048576):
                total += len(chunk)
                if total > 80 * 1048576: raise ValueError('oversized Hydra archive')
                stream.write(chunk)
    if sha256(archive) != ARCHIVE_SHA256: raise ValueError('Hydra archive checksum differs from the pinned release')
    binary = output / 'hydra'
    with tarfile.open(archive, 'r:gz') as package:
        matches = [member for member in package.getmembers() if member.name == 'hydra' and member.isfile()]
        if len(matches) != 1 or matches[0].size > 160 * 1048576: raise ValueError('unexpected Hydra archive member')
        with package.extractfile(matches[0]) as source, binary.open('xb') as stream:
            shutil.copyfileobj(source, stream)
    if sha256(binary) != BINARY_SHA256: raise ValueError('Hydra executable checksum differs from the pinned release')
    binary.chmod(0o755)
    report = {'version': VERSION, 'url': URL, 'archive_sha256': ARCHIVE_SHA256,
              'binary_sha256': BINARY_SHA256, 'binary': str(binary)}
    (output / 'runtime.json').write_text(json.dumps(report, indent=2) + '\n')
    print('[Hydra runtime]', VERSION, BINARY_SHA256, flush=True)
    return 0


if __name__ == '__main__': raise SystemExit(main())
