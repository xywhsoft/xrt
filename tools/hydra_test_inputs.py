"""Source inputs for the real Hydra client test and its Windows pipe bridge."""
from pathlib import Path
import hashlib


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def snapshot(root: Path) -> dict[str, str]:
    files = [root / 'single/xrt.h']
    for library in ['xjwt', 'xoauth2']:
        directory = root / 'extlibs' / library
        files += [*(directory / 'include').rglob('*.h'),
                  *(directory / 'tests/support').glob('*.c'), directory / 'config/modules.json']
        files += [p for folder in ['src', 'examples'] for p in (directory / folder).rglob('*')
                  if p.suffix in {'.c', '.h'}]
    files += [root / 'extlibs/xoauth2/tests/test_hydra_interop_client.c']
    files += [root / 'tools' / name for name in [
        'test_hydra_interop.py', 'hydra_test_inputs.py', 'hydra_windows_bridge.py',
        'prepare_hydra.py', 'test_mail_tls_interop.py']]
    return {p.relative_to(root).as_posix(): sha256(p) for p in sorted(set(files))}
