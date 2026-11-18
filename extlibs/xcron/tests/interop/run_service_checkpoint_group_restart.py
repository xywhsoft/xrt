"""Restore a durable group into an independent live service process."""

import argparse
import json
from pathlib import Path
import tempfile

from run_checkpoint_group_crash import parse_slot, read_image
from run_checkpoint_store_crash import command, digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--writer", type=Path, required=True)
    parser.add_argument("--service", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--periodic", action="store_true")
    args = parser.parse_args()
    writer = args.writer.resolve(strict=True)
    service = args.service.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="xcron-service-group-") as base:
        directory = Path(base)
        assert command(writer, "init", directory) == "GEN 1 COUNT 2 NEXT 1000 1000 SLOTS 2"
        first = read_image(directory)
        assert parse_slot(first, 1) == (1, [1000, 1000])
        initial = command(service, "show", directory)
        assert initial == "GEN 1 NEXT 1000 1000 SUCCEEDED 0 0"
        assert read_image(directory) == first
        advanced = command(service, "advance-periodic" if args.periodic else "advance", directory)
        generation = int(advanced.split()[1])
        assert advanced == f"GEN {generation} NEXT 2000 2000 SUCCEEDED 1 1"
        assert (generation >= 3) if args.periodic else (generation == 2)
        second = read_image(directory)
        previous = [2000, 2000] if args.periodic else [1000, 1000]
        assert parse_slot(second, generation & 1) == (generation, [2000, 2000])
        assert parse_slot(second, (generation - 1) & 1) == (generation - 1, previous)
        assert command(service, "show", directory) == advanced
        assert read_image(directory) == second
        assert command(writer, "show", directory) == f"GEN {generation} COUNT 2 NEXT 2000 2000 SLOTS 2"
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(dict(writer_sha256=digest(writer.read_bytes()),
        service_sha256=digest(service.read_bytes()), runner_sha256=digest(Path(__file__).read_bytes()),
        before_sha256=digest(first), after_sha256=digest(second), initial=initial, advanced=advanced,
        periodic=args.periodic, generation=generation, fixture_removed=not directory.exists()), indent=2)+"\n", encoding="utf8")
    print("whole group recovered and advanced across live service processes")


if __name__ == "__main__":
    main()
