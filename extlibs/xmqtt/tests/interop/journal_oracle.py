"""Independent version1 journal fixture/oracle: Python struct + zlib only.

No C implementation, private header, generated declaration or binary is loaded.
CLI: journal_oracle.py --emit FILE | --check FILE
"""
import json
from pathlib import Path
import struct
import sys
import zlib

PAGE = 4096
START = 3 * PAGE
BINDING = b'b\0principal/slot'


def crc(data):
    return zlib.crc32(data)


def seal(page):
    struct.pack_into('>I', page, PAGE - 4, crc(page[:-4]))
    return bytes(page)


def fixture():
    descriptor = bytearray(PAGE)
    descriptor[:8] = b'XMQWAL\0\0'
    struct.pack_into('>HBBIQQI', descriptor, 8, 1, 5, 0, len(BINDING), 13, START, 1)
    descriptor[64:64 + len(BINDING)] = BINDING
    frames, chain = [], 0
    for sequence, payload in enumerate((b'first', b'second'), 1):
        header = bytearray(48)
        header[:8] = b'XMQWTX1\0'
        struct.pack_into('>QQQIII', header, 8, 64 + len(payload), sequence,
                         len(payload), chain, crc(payload), 0)
        struct.pack_into('>I', header, 44, crc(header[:44]))
        chain = crc(header + payload)
        frames.append(bytes(header) + payload + struct.pack('>8sII', b'XMQWEND\0', chain, 0))
    witness = bytearray(PAGE)
    witness[:8] = b'XMQWCOM\0'
    struct.pack_into('>QQQI', witness, 8, 13, 2, START + sum(map(len, frames)), chain)
    return seal(descriptor) + seal(witness) * 2 + b''.join(frames)


def check(data):
    assert len(data) >= START
    descriptor = data[:PAGE]
    assert descriptor[:8] == b'XMQWAL\0\0'
    assert struct.unpack_from('>HBBIQQI', descriptor, 8) == (1, 5, 0, len(BINDING), 13, START, 1)
    assert descriptor[36:64] == bytes(28) and descriptor[64:64 + len(BINDING)] == BINDING
    assert not any(descriptor[64 + len(BINDING):PAGE - 4])
    assert struct.unpack_from('>I', descriptor, PAGE - 4)[0] == crc(descriptor[:-4])
    witnesses = []
    for offset in (PAGE, 2 * PAGE):
        page = data[offset:offset + PAGE]
        if page[:8] == b'XMQWCOM\0' and struct.unpack_from('>I', page, PAGE - 4)[0] == crc(page[:-4]):
            generation, sequence, end, chain = struct.unpack_from('>QQQI', page, 8)
            assert generation == 13 and not any(page[36:PAGE - 4])
            witnesses.append((sequence, end, chain))
    assert witnesses
    sequence, end, chain = max(witnesses)
    assert end <= len(data)
    cursor, previous, count, payloads = START, 0, 0, []
    while cursor < end:
        header = data[cursor:cursor + 48]
        assert len(header) == 48 and header[:8] == b'XMQWTX1\0'
        total, number, length, prior, checksum, reserved, head_crc = struct.unpack_from('>QQQIIII', header, 8)
        assert total == length + 64 and total <= end - cursor and number == count + 1
        assert prior == previous and not reserved and head_crc == crc(header[:44])
        payload = data[cursor + 48:cursor + 48 + length]
        assert checksum == crc(payload)
        footer = data[cursor + 48 + length:cursor + total]
        magic, previous, reserved = struct.unpack('>8sII', footer)
        assert magic == b'XMQWEND\0' and not reserved and previous == crc(header + payload)
        payloads.append(payload.hex())
        cursor += total
        count += 1
    assert cursor == end and count == sequence and previous == chain
    assert payloads in [[b'first'.hex()], [b'first'.hex(), b'second'.hex()]]
    return dict(sequence=sequence, end=end, chain=chain, payloads=payloads,
                uncommitted_suffix=len(data) - end)


if __name__ == '__main__':
    assert len(sys.argv) == 3
    path = Path(sys.argv[2])
    if sys.argv[1] == '--emit':
        path.write_bytes(fixture())
    else:
        assert sys.argv[1] == '--check'
    print(json.dumps(check(path.read_bytes()), sort_keys=True))
