"""Native process exclusion and crash release for the locked MQTT journal."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import queue
import subprocess
import tempfile
import threading


def result(binary, *args):
    return subprocess.run([str(binary), *args], timeout=12, capture_output=True,
                          check=True, text=True).stdout.strip()


def race(binary, version, stop, workers=6):
    with tempfile.TemporaryDirectory(prefix='xmqtt-journal-process-') as temp:
        directory = Path(temp)
        first = directory / 'session.wal'
        first.touch(exist_ok=False)
        os.link(first, directory / 'alias.wal')
        expected = queue.Queue()
        active = []
        readers = []
        try:
            for index in range(workers):
                name = 'alias.wal' if index % 2 else first.name
                command = [str(binary), 'hold', str(directory), name, version]
                child = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                         stderr=subprocess.STDOUT, text=True)
                active.append(child)

                def observe(process=child, position=index):
                    expected.put((position, process.stdout.readline().strip()))

                reader = threading.Thread(target=observe, daemon=True)
                readers.append(reader)
                reader.start()
            messages = [expected.get(timeout=12) for _ in active]
            winners = [index for index, marker in messages if marker == 'READY']
            assert len(winners) == 1, messages
            winner = active[winners[0]]
            for index, marker in messages:
                if index != winners[0]:
                    assert marker == 'BUSY' and active[index].wait(timeout=8) == 3, messages
            assert result(binary, 'busy', str(directory), first.name, version) == 'BUSY'
            assert result(binary, 'busy', str(directory), 'alias.wal', version) == 'BUSY'
            if stop == 'kill':
                winner.kill()
            else:
                winner.stdin.close()
            exit_code = winner.wait(timeout=8)
            assert (exit_code != 0) if stop == 'kill' else (exit_code == 0)
            assert result(binary, 'recover', str(directory), 'alias.wal', version) == 'RECOVERED'
            return dict(version=version, stop=stop, contenders=workers, acquired=1, busy=workers-1,
                        original_and_alias_busy=True, exit_code=exit_code, recovered=True)
        finally:
            for child in active:
                if child.poll() is None:
                    child.kill()
                child.wait(timeout=8)
                child.stdin.close()
            for reader in readers:
                reader.join(3)
                assert not reader.is_alive()
            for child in active:
                child.stdout.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, action='append', required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    rows = []
    for executable in args.binary:
        binary = executable.resolve(strict=True)
        checksum = hashlib.sha256(binary.read_bytes()).hexdigest()
        for version in ('311', '5'):
            for stop in ('close', 'kill'):
                row = race(binary, version, stop)
                rows.append(dict(binary=binary.name, binary_sha256=checksum, **row))
                print(binary.name, version, stop, 'serialized and recovered', flush=True)
    args.report.write_text(json.dumps(dict(schema=1, platform=os.name, complete=True, cases=rows),
                                      indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__':
    main()
