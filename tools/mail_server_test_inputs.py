"""Hash the source and build inputs of the independent mail server test."""
import hashlib
import json
import os
from pathlib import Path


def sha256(path):
    name = str(Path(path).resolve())
    if os.name == 'nt':
        name = '\\\\?\\UNC\\' + name[2:] if name.startswith('\\\\') else '\\\\?\\' + name
    with open(name, 'rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def snapshot(root):
    root = Path(root).resolve()
    inputs = {}
    directories = ['src', 'include', 'single', 'config', 'tools']
    for product in ['xmail', 'xpop3', 'xsmtp', 'ximap']:
        directories.extend('extlibs/' + product + '/' + part
                           for part in ['src', 'include', 'single', 'config', 'examples'])
    for directory in directories:
        for path in sorted((root / directory).rglob('*')):
            if path.is_file() and path.suffix in ('.c', '.h', '.py', '.json'):
                inputs[path.relative_to(root).as_posix()] = sha256(path)
    encoded = json.dumps(inputs, sort_keys=True, separators=(',', ':')).encode()
    return {'files': inputs, 'fingerprint': hashlib.sha256(encoded).hexdigest()}
