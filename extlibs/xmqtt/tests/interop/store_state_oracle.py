"""Independent legal recovery fixture: literal MQTT packets + struct/zlib.

Reuses only the Python delta/journal envelope oracle, no C/header/binary import.
The new legal history ends with three pending deliveries across two epochs.
"""
from pathlib import Path
import json
import struct
import sys
import store_delta_oracle as envelope

original_delta = envelope.delta


def state_delta(version):
    raw = original_delta(version)
    at = 32
    for _ in range(12):
        length = struct.unpack_from('>Q', raw, at + 40)[0]
        at += 48 + length
    expiry = 60 if version == 5 else 0xffffffff
    rows = [(3, 0, 0, 0, expiry, 1, 0, 1002, b''),
            (4, 0, 0, 0, 0, 1, 0, 1003, b''),
            (2, 0, 0, 0, expiry, 2, 0, 1004, b''),
            (9, 0, 0, 0, 0, 2, 23, 0, envelope.publish(version, 0, 0, True))]
    body = raw[32:at] + b''.join(struct.pack('>BBBxHHI4xQQQQ', tag, flags, reason,
        identifier, 48, expiry, epoch, key, wall, len(data)) + data
        for tag, flags, reason, identifier, expiry, epoch, key, wall, data in rows)
    return struct.pack('>8sHBBIQ8x', b'XMQDSET\0', 1, version, 0, 16, 32 + len(body)) + body


def check_delta(data, version):
    assert data == state_delta(version)
    return dict(version=version, operations=16, epoch=2, inbox_keys=[21, 22, 23])


envelope.BINDING = b'state/broker/principal/slot'
envelope.delta = state_delta
envelope.check_delta = check_delta
journal = envelope.journal
check_journal = envelope.check_journal

if __name__ == '__main__':
    assert len(sys.argv) == 4
    mode, path = sys.argv[1], Path(sys.argv[2])
    version = 5 if sys.argv[3] == '5' else 4
    if mode == '--emit-journal':
        path.write_bytes(journal(version))
    else:
        assert mode == '--check-journal'
    print(json.dumps(check_journal(path.read_bytes(), version), sort_keys=True))
