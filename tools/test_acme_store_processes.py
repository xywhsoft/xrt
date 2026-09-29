"""Exercise ACME grant rotation from independent processes against one store."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def _binary(path: Path) -> Path:
    candidate = path if path.is_absolute() else ROOT / path
    executable = candidate.with_suffix(".exe")
    if os.name == "nt" and executable.is_file():
        return executable
    if candidate.is_file():
        return candidate
    if executable.is_file():
        return executable
    raise FileNotFoundError(f"ACME concurrency test executable not found: {path}")


def run(binary: Path, pairs: int, output_root: Path | None) -> None:
    for index in range(pairs):
        env = os.environ.copy()
        base = output_root or (
            ROOT / "out" / "xacme-test-out-process"
            if os.name == "nt" or binary.suffix.lower() == ".exe"
            else Path(tempfile.gettempdir()) / "xacme-test-out-process"
        )
        store_root = base / f"{os.getpid()}-{index}"
        if os.name != "nt" and binary.suffix.lower() == ".exe":
            env["XACME_TEST_ROOT"] = subprocess.check_output(
                ["cygpath", "-w", str(store_root)], text=True
            ).strip()
        else:
            env["XACME_TEST_ROOT"] = str(store_root)
        processes: list[subprocess.Popen[str]] = []
        try:
            for _ in range(2):
                processes.append(
                    subprocess.Popen(
                        [str(binary)],
                        cwd=ROOT,
                        env=env,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        text=True,
                    )
                )
            results = [process.communicate(timeout=40)[0] for process in processes]
        except subprocess.TimeoutExpired as error:
            raise RuntimeError(f"ACME cross-process pair {index + 1} timed out") from error
        finally:
            for process in processes:
                if process.poll() is None:
                    process.kill()
                    process.wait()
        if any(process.returncode != 0 for process in processes):
            for slot, (process, output) in enumerate(zip(processes, results), 1):
                print(f"[process {slot}] exit={process.returncode}\n{output}")
            raise RuntimeError(f"ACME cross-process pair {index + 1} failed")
    print(f"[PASS] ACME concurrent rotation across {pairs} process pairs")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--binary",
        type=Path,
        default=Path("out/gcc/native/xacme_tests/test_store_concurrent"),
    )
    parser.add_argument("--pairs", type=int, default=3)
    parser.add_argument("--output-root", type=Path)
    args = parser.parse_args()
    if args.pairs < 1:
        parser.error("--pairs must be positive")
    run(_binary(args.binary), args.pairs, args.output_root)


if __name__ == "__main__":
    main()
