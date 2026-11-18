"""Native process exits and an independent selector-page recovery oracle."""
import argparse
import hashlib
import json
from pathlib import Path
import queue
import struct
import subprocess
import tempfile
import threading
import zlib


NAMES = ('journal.slot', 'journal.0', 'journal.1')
PAGE = 4096


def run(binary, mode, directory, version):
    return subprocess.run([str(binary), mode, str(directory), version], timeout=12,
                          text=True, capture_output=True, check=True).stdout.splitlines()


def paused(binary, mode, directory, version, *, kill, value='new'):
    command = [str(binary), mode, str(directory), version, value]
    child = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True)
    messages = queue.Queue(maxsize=1)
    reader = threading.Thread(target=lambda: messages.put(child.stdout.readline().strip()), daemon=True)
    reader.start()
    try:
        want = 'PREPARED' if mode == 'prepare' else 'PUBLISHED'
        seen = messages.get(timeout=10)
        assert seen == want, (seen, child.poll())
        assert run(binary, 'busy', directory, version) == ['BUSY']
        if kill:
            child.kill()
        else:
            child.stdin.close()
        code = child.wait(timeout=8)
        assert (code != 0) if kill else (code == 0)
        return code
    finally:
        if child.poll() is None:
            child.kill()
        child.wait(timeout=8)
        reader.join(3)
        assert not reader.is_alive()
        child.stdin.close()
        child.stdout.close()


def fixture(parent, name, image=None):
    path = parent / name
    path.mkdir()
    for index, leaf in enumerate(NAMES):
        (path / leaf).write_bytes(image[index] if image else b'')
    return path


def snapshot(path):
    return [(path / name).read_bytes() for name in NAMES]


def changed(page, offset, value, encoding):
    data = bytearray(page)
    struct.pack_into(encoding, data, offset, value)
    struct.pack_into('>I', data, PAGE - 4, zlib.crc32(data[:-4]))
    return bytes(data)


def images(binary, version, parent):
    path = fixture(parent, 'source')
    run(binary, 'create', path, version)
    before = snapshot(path)
    killed_before = paused(binary, 'prepare', path, version, kill=True)
    assert run(binary, 'recover', path, version) == ['9:old']
    staged = snapshot(path)
    assert staged[0] == before[0] and staged[1] == before[1] and staged[2] != before[2]
    killed_after = paused(binary, 'publish', path, version, kill=True)
    assert run(binary, 'recover', path, version) == ['10:new', '10:tail']
    published = snapshot(path)
    old = [before[0][offset:offset + PAGE] for offset in range(0, len(before[0]), PAGE)]
    new = [published[0][offset:offset + PAGE] for offset in range(0, len(published[0]), PAGE)]
    assert len(old) == len(new) == 3 and old[0] == new[0] and old[1] == old[2] and new[1] == new[2]
    for pages in (old, new):
        for page in pages:
            assert zlib.crc32(page[:-4]) == struct.unpack_from('>I', page, PAGE - 4)[0]
    assert struct.unpack_from('>QQ', old[1], 8) == (9, 0)
    assert struct.unpack_from('>QQ', new[1], 8) == (10, 1)

    rows = [dict(case='process exit before publish', status='generation 9', exit_code=killed_before),
            dict(case='process exit after publish', status='generation 10', exit_code=killed_after)]
    bad = bytes(PAGE)
    choices = [
        ('old plus torn', old[1], bad, ['9:old']),
        ('new left', new[1], old[2], ['10:new', '10:tail']),
        ('new right', old[1], new[2], ['10:new', '10:tail']),
        ('new plus torn', bad, new[2], ['10:new', '10:tail']),
        ('both torn', bad, bad, None),
        ('nonadjacent', old[1], changed(new[2], 8, 12, '>Q'), None),
        ('conflicting same generation', new[1], changed(new[2], 32, 1, '>I'), None),
    ]
    for index, (name, left, right, want) in enumerate(choices):
        copy = fixture(parent, f'oracle-{index}', [old[0]+left+right, *published[1:]])
        if want:
            assert run(binary, 'recover', copy, version) == want
            if want[0].startswith('10:'):
                paused(binary, 'prepare', copy, version, kill=False)
                assert snapshot(copy)[0] == published[0], 'repair the stale witness before inactive reuse'
                assert run(binary, 'recover', copy, version) == want
        else:
            assert run(binary, 'reject', copy, version) == ['CORRUPT']
        rows.append(dict(case=name, status=want[0] if want else 'CORRUPT'))

    wrong = fixture(parent, 'wrong', before)
    paused(binary, 'publish', wrong, version, kill=False, value='alien')
    # A newer valid journal under the same descriptor and generation does not
    # replace the selected committed prefix.
    copy = fixture(parent, 'changed-prefix', [published[0], published[1], snapshot(wrong)[2]])
    assert run(binary, 'reject', copy, version) == ['CORRUPT']
    copy = fixture(parent, 'bad-selected', [published[0], published[1], b''])
    assert run(binary, 'reject', copy, version) == ['CORRUPT']
    rows.extend([dict(case='newer tail with a different published prefix', status='CORRUPT'),
                 dict(case='selected data invalid with intact previous data', status='CORRUPT')])
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', action='append', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    report = dict(schema=1, complete=False, cases=[])
    args.report.parent.mkdir(parents=True, exist_ok=True)
    try:
        for binary in args.binary:
            binary = binary.resolve(strict=True)
            digest = hashlib.sha256(binary.read_bytes()).hexdigest()
            for version in ('5', '311'):
                with tempfile.TemporaryDirectory(prefix='xmqtt-slot-process-') as temporary:
                    results = images(binary, version, Path(temporary))
                report['cases'].extend(dict(binary=binary.name, sha256=digest, mqtt=version, **r) for r in results)
                print(binary.name, version, len(results), 'recovery cases agreed', flush=True)
        report['complete'] = True
    finally:
        args.report.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')


if __name__ == '__main__':
    main()
