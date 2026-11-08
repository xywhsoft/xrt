"""Cross-check native ID keys with generated arbitrary-precision decimal pairs.

The implementation uses bounded decimal digit arithmetic. This oracle instead
uses Python's arbitrary-precision integer exponent arithmetic and generates
equivalent coefficient/scale spellings, with deliberate unequal controls.
No arbitrary-size power of ten is expanded and no floating-point conversion runs.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from test_protocol_fuzz import _closure


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def pairs(count: int, seed: int) -> tuple[str, list[str]]:
    rng = random.Random(seed)
    lines: list[str] = []
    expected: list[str] = []
    for trial in range(count):
        digits = str(rng.randrange(1, 10))
        digits += "".join(str(rng.randrange(10)) for _ in range(rng.randrange(30)))
        if digits[-1] == "0":
            digits = digits[:-1] + "7"
        coefficient_sign = "-" if rng.getrandbits(1) else ""
        exponent_digits = str(rng.randrange(1, 10))
        exponent_digits += "".join(str(rng.randrange(10)) for _ in range(rng.randrange(200)))
        exponent = int(exponent_digits) * (-1 if rng.getrandbits(1) else 1)
        # Include offsets larger than one decimal digit, opposite signs,
        # cancellation near zero and native integer boundaries.
        if trial % 5 == 0:
            exponent = rng.randrange(-2048, 2049)
        shift = rng.randrange(-1024, 1025)
        if shift >= 0:
            alternate = digits + "0" * shift
        else:
            point = len(digits) + shift
            alternate = digits[:point] + "." + digits[point:] if point > 0 else "0." + "0" * (-point) + digits
        equal = trial % 4 != 0
        alternate_exponent = exponent - shift + (0 if equal else 1)
        # Zero-prefixed exponent spellings and explicit plus signs are legal.
        original_exponent = ("-" if exponent < 0 else "+") + "0" * rng.randrange(8) + str(abs(exponent))
        left = coefficient_sign + digits + "e" + original_exponent
        right = coefficient_sign + alternate + "e" + str(alternate_exponent)
        assert len(left) < 4096 and len(right) < 4096
        lines.extend((left, right))
        expected.append("1" if equal else "0")
    text = "\n".join(lines) + "\n"
    if len(text) > 64 * 1024 * 1024:
        raise ValueError("oracle input exceeds 64 MiB")
    return text, expected


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cases", type=int, default=20000)
    parser.add_argument("--seed", type=int, default=0x142857)
    parser.add_argument("--compiler", default="gcc")
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if not 1 <= args.cases <= 50000:
        parser.error("cases must be within 1..50000")
    compiler = shutil.which(args.compiler)
    if not compiler:
        parser.error("compiler not found: " + args.compiler)
    label = "windows" if os.name == "nt" else "linux"
    output = args.output or ROOT / "out/service-extensions/xrpc-id" / (label + "-oracle")
    if not output.is_absolute():
        output = ROOT / output
    output.mkdir(parents=True, exist_ok=True)
    source = "extlibs/xrpc/tests/interop/id_probe.c"
    sources, defines, links, _ = _closure("xrpc_id", "extlibs/xrpc/config/modules.json")
    binary = output / ("id_probe.exe" if os.name == "nt" else "id_probe")
    inputs = [ROOT / source, *[ROOT / value for value in sources]]
    inputs += list((ROOT / "include").rglob("*.h")) + list((ROOT / "src").rglob("*.h"))
    inputs += list((ROOT / "extlibs/xrpc/include").rglob("*.h")) + list((ROOT / "extlibs/xrpc/src").rglob("*.h"))
    inputs += [Path(__file__), ROOT / "tools/test_protocol_fuzz.py", ROOT / "config/modules.json", ROOT / "extlibs/xrpc/config/modules.json"]
    hashes = {path.relative_to(ROOT).as_posix(): digest(path) for path in inputs}
    flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"] if args.sanitize else []
    command = [compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", *flags,
               *["-D" + value for value in defines], "-Iinclude", "-Iextlibs/xrpc/include",
               source, *sources, *["-l" + value for value in links], "-o", str(binary)]
    with (output / "build.log").open("w", encoding="utf-8") as handle:
        subprocess.run(command, cwd=ROOT, stdout=handle, stderr=subprocess.STDOUT, check=True, timeout=600)
    data, expected = pairs(args.cases, args.seed)
    (output / "pairs.txt").write_text(data, encoding="utf-8")
    env = os.environ.copy()
    if args.sanitize:
        env["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=1"
        env["UBSAN_OPTIONS"] = "halt_on_error=1"
    result = subprocess.run([str(binary)], input=data, capture_output=True, text=True,
                            encoding="utf-8", env=env, check=True, timeout=600)
    (output / "output.txt").write_text(result.stdout, encoding="utf-8")
    (output / "stderr.log").write_text(result.stderr, encoding="utf-8")
    actual = result.stdout.splitlines()
    if actual != expected:
        mismatch = next((index for index, pair in enumerate(zip(actual, expected)) if pair[0] != pair[1]), min(len(actual), len(expected)))
        raise AssertionError(f"oracle mismatch at pair {mismatch}: {len(actual)} results, {len(expected)} expected")
    assert hashes == {path.relative_to(ROOT).as_posix(): digest(path) for path in inputs}, "inputs changed during check"
    receipt = {"cases": args.cases, "seed": args.seed, "sanitize": args.sanitize, "command": command, "platform": platform.platform(),
               "compiler": compiler, "compiler_sha256": digest(Path(compiler)), "binary_sha256": digest(binary),
               "inputs": hashes, "pairs_sha256": digest(output / "pairs.txt"), "output_sha256": digest(output / "output.txt")}
    (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print(f"[PASS] xrpc ID arbitrary-precision exponent oracle: {args.cases} pairs; unequal controls={args.cases//4 + (args.cases%4 != 0)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
