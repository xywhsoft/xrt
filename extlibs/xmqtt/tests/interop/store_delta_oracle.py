"""Independent typed-delta + enclosing journal v1 oracle (struct/zlib only).

No project C/header/native codec is loaded. A literal MQTT PUBLISH/property
builder provides independent expected bytes for both protocol levels.
CLI: --emit-delta/--check-delta/--emit-journal/--check-journal FILE 5|311
"""
from pathlib import Path
import json
import struct
import sys
import zlib

START = 12288
BINDING = b'delta/broker/principal/slot'
SCHEMA = 0x584d4401


def variable(number):
    result = bytearray()
    while True:
        byte = number % 128
        number //= 128
        result.append(byte | (128 if number else 0))
        if not number:
            return bytes(result)


def text(value):
    return struct.pack('>H', len(value)) + value


def publish(version, qos, identifier, incoming):
    body = text(b'delta/topic') + (struct.pack('>H', identifier) if qos else b'')
    if version == 5:
        properties = b'\x26' + text(b'meta') + text(b'value') + b'\x01\x01\x09' + text(b'a\0b')
        if incoming:
            properties += b'\x0b\x01'
        body += variable(len(properties)) + properties
    body += b'x\0z'
    return bytes([0x30 | qos << 1]) + variable(len(body)) + body


def delta(version):
    expiry = 60 if version == 5 else 0xffffffff
    # tag, flags, reason, id, expiry, epoch, key, wall, body
    rows = [(1, 0, 0, 0, 0, 1, 0, 1000, b'delta-client'),
            (2, 0, 0, 0, expiry, 1, 0, 1001, b''),
            (5, 3, 0, 7, 0, 1, 11, 0, publish(version, 1, 7, False)),
            (6, 0, 0, 7, 0, 1, 11, 0, b''),
            (5, 3, 0, 8, 0, 1, 12, 0, publish(version, 2, 8, False)),
            (7, 0, 0x10 if version == 5 else 0, 8, 0, 1, 12, 0, b''),
            (8, 7, 0, 8, 0, 1, 12, 0, b''),
            (8, 4, 0, 7, 0, 1, 11, 0, b''),
            (9, 0, 0, 17, 0, 1, 21, 0, publish(version, 2, 17, True)),
            (9, 0, 0, 0, 0, 1, 22, 0, publish(version, 0, 0, True)),
            (10, 0, 0, 17, 0, 1, 21, 0, b''),
            (11, 0, 0, 17, 0, 1, 21, 0, b''),
            (3, 1, 0, 0, expiry, 1, 0, 2000, b''),
            (4, 0, 2, 0, 0, 1, 0, 2001, b''),
            (12, 0, 0, 0, 0, 1, 21, 0, b''),
            (12, 0, 0, 0, 0, 1, 22, 0, b'')]
    encoded = []
    for tag, flags, reason, identifier, expiry, epoch, key, wall, body in rows:
        encoded.append(struct.pack('>BBBxHHI4xQQQQ', tag, flags, reason, identifier,
                                   48, expiry, epoch, key, wall, len(body)) + body)
    length = 32 + sum(map(len, encoded))
    assert length == (986 if version == 5 else 890)
    return struct.pack('>8sHBBIQQ', b'XMQDSET\0', 1, version, 0, len(rows), length, 0) + b''.join(encoded)


def check_delta(data, version):
    assert data == delta(version)
    return dict(version=version, operations=16, bytes=len(data), independent_literal_packet_bytes=True)


def seal(page):
    struct.pack_into('>I', page, 4092, zlib.crc32(page[:4092]))
    return bytes(page)


def journal(version):
    payload = delta(version)
    header = struct.pack('>8sQQQIII', b'XMQWTX1\0', 64 + len(payload), 1, len(payload), 0, zlib.crc32(payload), 0)
    header += struct.pack('>I', zlib.crc32(header))
    chain = zlib.crc32(header + payload)
    frame = header + payload + struct.pack('>8sII', b'XMQWEND\0', chain, 0)
    descriptor = bytearray(4096)
    descriptor[:8] = b'XMQWAL\0\0'
    struct.pack_into('>HBBIQQI', descriptor, 8, 1, version, 0, len(BINDING), 9, START, SCHEMA)
    descriptor[64:64 + len(BINDING)] = BINDING
    witness = bytearray(4096)
    witness[:8] = b'XMQWCOM\0'
    struct.pack_into('>QQQI', witness, 8, 9, 1, START + len(frame), chain)
    return seal(descriptor) + seal(witness) * 2 + frame


def check_journal(data, version):
    assert len(data) >= START
    descriptor = data[:4096]
    assert descriptor[:8] == b'XMQWAL\0\0'
    assert struct.unpack_from('>HBBIQQI', descriptor, 8) == (1, version, 0, len(BINDING), 9, START, SCHEMA)
    assert not any(descriptor[36:64]) and descriptor[64:64 + len(BINDING)] == BINDING
    assert not any(descriptor[64 + len(BINDING):4092])
    assert struct.unpack_from('>I', descriptor, 4092)[0] == zlib.crc32(descriptor[:4092])
    witnesses = []
    for at in (4096, 8192):
        page = data[at:at + 4096]
        if page[:8] != b'XMQWCOM\0' or struct.unpack_from('>I', page, 4092)[0] != zlib.crc32(page[:4092]):
            continue
        generation, sequence, end, chain = struct.unpack_from('>QQQI', page, 8)
        assert generation == 9 and not any(page[36:4092])
        witnesses.append((sequence, end, chain))
    assert witnesses
    sequence, end, chain = max(witnesses)
    assert end <= len(data) and sequence in (0, 1)
    if not sequence:
        assert end == START and not chain
        operations = 0
    else:
        header = data[START:START + 48]
        magic, total, number, length, prior, checksum, reserved, head_crc = struct.unpack('>8sQQQIIII', header)
        assert magic == b'XMQWTX1\0' and total == end - START and total == length + 64 and number == 1 and not prior and not reserved
        assert head_crc == zlib.crc32(header[:44])
        body = data[START + 48:START + 48 + length]
        assert checksum == zlib.crc32(body) and body == delta(version)
        footer = data[end - 16:end]
        assert struct.unpack('>8sII', footer) == (b'XMQWEND\0', chain, 0)
        assert chain == zlib.crc32(header + body)
        operations = check_delta(body, version)['operations']
    return dict(version=version, sequence=sequence, operations=operations, end=end,
                uncommitted_suffix=len(data) - end)


if __name__ == '__main__':
    assert len(sys.argv) == 4
    mode, path = sys.argv[1], Path(sys.argv[2])
    version = 5 if sys.argv[3] == '5' else 4
    if mode == '--emit-delta':
        path.write_bytes(delta(version))
    elif mode == '--emit-journal':
        path.write_bytes(journal(version))
    else:
        assert mode in ('--check-delta', '--check-journal')
    print(json.dumps((check_journal if mode.endswith('journal') else check_delta)(path.read_bytes(), version), sort_keys=True))
