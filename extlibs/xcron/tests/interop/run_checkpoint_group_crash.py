"""Terminate owned group writers and independently validate both wire slots."""

import argparse
import json
from pathlib import Path
import struct
import tempfile
import zlib

from run_checkpoint_store_crash import command, digest, kill_at


SLOT = 8192
HEADER = struct.Struct("<4sIQQIIII")
RECORD = 180


def read_image(directory):
    image = (directory / "group.xcg").read_bytes()
    assert len(image) == 2 * SLOT
    return image


def parse_record(record):
    assert record[:4] == b"XCCP" and len(record) == RECORD
    assert zlib.crc32(record[:-4]) == int.from_bytes(record[-4:], "little")
    return struct.unpack_from("<QQ", record, 8), struct.unpack_from("<q", record, 40)[0]


def parse_slot(image, index):
    offset = index * SLOT
    header = image[offset:offset + HEADER.size]
    magic, version, generation, group, capacity, count, group_crc, header_crc = HEADER.unpack(header)
    if magic != b"XCGS" or zlib.crc32(header[:-4]) != header_crc:
        return None
    assert version == 1 and generation % 2 == index and group == 940 and capacity == 32
    assert count <= capacity
    records = [image[start:start + RECORD]
               for start in range(offset + HEADER.size, offset + HEADER.size + RECORD * count, RECORD)]
    if zlib.crc32(b"".join(record[-4:] for record in records)) != group_crc:
        return None
    decoded = [parse_record(record) for record in records]
    assert [key for key, _ in decoded] == [(j + 1, 1) for j in range(count)]
    return generation, [next_at for _, next_at in decoded]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--fault-probe", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    client = args.client.resolve(strict=True)
    probe = args.fault_probe.resolve(strict=True)
    cases = []
    with tempfile.TemporaryDirectory(prefix="xcron-group-crash-") as base:
        for action, marker, recovered, expected in (
            ("interrupt-torn", "GROUP TORN", "GEN 1 COUNT 2 NEXT 1000 1000 SLOTS 1", None),
            ("interrupt-flush", "GROUP BEFORE FLUSH", "GEN 2 COUNT 2 NEXT 2000 2000 SLOTS 2", (2, [2000, 2000])),
        ):
            directory = Path(base) / action
            directory.mkdir()
            assert command(client, "init", directory) == "GEN 1 COUNT 2 NEXT 1000 1000 SLOTS 2"
            before = read_image(directory)
            assert parse_slot(before, 0) == (0, []) and parse_slot(before, 1) == (1, [1000, 1000])
            kill_exit = kill_at(probe, action, directory, marker, contender=client)
            after = read_image(directory)
            assert after[SLOT:] == before[SLOT:] and parse_slot(after, 0) == expected
            assert parse_record(after[HEADER.size:HEADER.size + RECORD]) == ((1, 1), 2000)
            assert command(client, "show", directory) == recovered and read_image(directory) == after
            assert command(client, "advance", directory) == (
                "GEN 2 COUNT 2 NEXT 2000 2000 SLOTS 2" if expected is None else
                "GEN 3 COUNT 2 NEXT 3000 3000 SLOTS 2")
            cases.append(dict(action=action, kill_exit=kill_exit, contender_denied=True, recovered=recovered,
                              before_sha256=digest(before), after_sha256=digest(after)))
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(dict(client_sha256=digest(client.read_bytes()),
        probe_sha256=digest(probe.read_bytes()), runner_sha256=digest(Path(__file__).read_bytes()),
        cases=cases, fixture_removed=not Path(base).exists()), indent=2)+"\n", encoding="utf8")
    print("checkpoint groups recovered only complete generations")


if __name__ == "__main__":
    main()
