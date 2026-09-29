"""Build mail extensions with gcov and report extension-owned coverage.

Run on Linux or Windows with GCC and gcov. Use --report-only to inspect the
profiles produced by an earlier coverage build without running tests again.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SUITES = {
	"xmail": "xmail_tests",
	"xpop3": "xpop3_tests",
	"xsmtp": "xsmtp_tests",
	"ximap": "ximap_tests",
}
METRIC = re.compile(
	r"^(Lines executed|Branches executed|Taken at least once):"
	r"([0-9]+(?:\.[0-9]+)?)% of ([0-9]+)$"
)


def _sources(product: str) -> list[Path]:
	manifest = ROOT / "extlibs" / product / "config" / "modules.json"
	data = json.loads(manifest.read_text(encoding="utf-8"))
	prefix = f"extlibs/{product}/src/"
	return sorted({
		Path(source) for module in data["modules"]
		for source in module.get("sources", [])
		if source.startswith(prefix) and source.endswith(".c")
	})


def _object_dir(product: str, suite: str, compiler: str) -> Path:
	# build.py stores native GCC objects here for both supported hosts.
	if Path(compiler).name.lower() not in {"gcc", "gcc.exe"}:
		raise ValueError("coverage tool currently supports --compiler gcc")
	return ROOT / "out" / "gcc" / "native" / suite / "obj"


def _build(product: str, suite: str, compiler: str, object_dir: Path) -> None:
	# Stale profiles from an earlier binary must not inflate the result.
	if object_dir.is_dir():
		for profile in object_dir.glob("*.gcda"):
			profile.unlink()
	command = [
		sys.executable, str(ROOT / "tools" / "build.py"),
		"--compiler", compiler,
		"--manifest", f"extlibs/{product}/config/modules.json",
		"--suite", suite, "--no-single", "--jobs", "4",
		"--cflag=--coverage", "--ldflag=--coverage",
	]
	print(f"[coverage build] {product} ({suite})", flush=True)
	subprocess.run(command, cwd=ROOT, check=True)


def _gcov(source: Path, note: Path, gcov: str) -> tuple[int, int, int, int]:
	if not note.is_file():
		raise RuntimeError(f"missing gcov notes for {source}: {note}")
	report = subprocess.run(
		[gcov, "-n", "-b", "-o", str(note), str(ROOT / source)],
		cwd=ROOT, check=True, capture_output=True, text=True,
	).stdout
	target = "/" + source.as_posix()
	current = False
	values: dict[str, tuple[float, int]] = {}
	for line in report.splitlines():
		if line.startswith("File '") and line.endswith("'"):
			current = line[6:-1].replace("\\", "/").endswith(target)
			continue
		if not current:
			continue
		match = METRIC.fullmatch(line)
		if match:
			values[match.group(1)] = (float(match.group(2)), int(match.group(3)))
		elif line == "No branches":
			values["Taken at least once"] = (100.0, 0)
	if "Lines executed" not in values:
		raise RuntimeError(f"gcov omitted source line coverage: {source}")
	line_percent, line_count = values["Lines executed"]
	branch_percent, branch_count = values.get("Taken at least once", (100.0, 0))
	return (
		round(line_percent * line_count / 100), line_count,
		round(branch_percent * branch_count / 100), branch_count,
	)


def measure(product: str, compiler: str, gcov: str,
		report_only: bool) -> tuple[float, float]:
	suite = SUITES[product]
	object_dir = _object_dir(product, suite, compiler)
	if not report_only:
		_build(product, suite, compiler, object_dir)
	totals = [0, 0, 0, 0]
	for source in _sources(product):
		stem = "__".join(source.with_suffix("").parts)
		note = object_dir / f"{stem}.gcno"
		covered, lines, taken, branches = _gcov(source, note, gcov)
		line_text = f"{covered}/{lines} ({covered / lines:.1%})"
		branch_text = (
			f"{taken}/{branches} ({taken / branches:.1%})"
			if branches else "0/0"
		)
		print(
			f"  {source}: lines {line_text}, branches {branch_text}",
			flush=True,
		)
		for index, count in enumerate((covered, lines, taken, branches)):
			totals[index] += count
	line_rate = totals[0] / totals[1]
	branch_rate = totals[2] / totals[3] if totals[3] else 1.0
	print(
		f"[coverage] {product}: lines {totals[0]}/{totals[1]} "
		f"({line_rate:.2%}), branches {totals[2]}/{totals[3]} "
		f"({branch_rate:.2%})",
		flush=True,
	)
	return line_rate, branch_rate


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--product", choices=[*SUITES, "all"], default="all")
	parser.add_argument("--compiler", default="gcc")
	parser.add_argument("--gcov", default="gcov")
	parser.add_argument("--report-only", action="store_true")
	parser.add_argument("--min-lines", type=float, default=0.0,
		help="minimum extension-owned line coverage in percent")
	parser.add_argument("--min-branches", type=float, default=0.0,
		help="minimum extension-owned branch outcome coverage in percent")
	args = parser.parse_args()
	products = list(SUITES) if args.product == "all" else [args.product]
	failed = False
	for product in products:
		lines, branches = measure(
			product, args.compiler, args.gcov, args.report_only)
		if lines * 100 < args.min_lines or branches * 100 < args.min_branches:
			print(f"[coverage fail] {product} below configured baseline")
			failed = True
	return 1 if failed else 0


if __name__ == "__main__":
	raise SystemExit(main())
