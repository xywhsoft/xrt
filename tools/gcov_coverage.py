"""Union extension-owned gcov JSON counters only when source and CFG identities agree.

The gcov format is documented at https://gcc.gnu.org/onlinedocs/gcc/Invoking-Gcov.html.
Version 1 has ordered branch outcomes; version 2 also supplies basic-block edge IDs.
"""

from __future__ import annotations

import gzip
import hashlib
import json
import subprocess
from pathlib import Path
from typing import Any


def source_hashes(sources: list[Path]) -> dict[str, str]:
    return {str(source.resolve()): hashlib.sha256(source.read_bytes()).hexdigest()
            for source in sources}


def compiler_gcov(compiler: str) -> str:
    companion = subprocess.run([compiler, "-print-prog-name=gcov"],
                               check=True, capture_output=True, text=True).stdout.strip()
    if not companion:
        raise RuntimeError("compiler did not identify its gcov companion")
    return companion


def gcov_json(note: Path, gcov: str) -> dict[str, Any]:
    if not note.is_file() or not note.with_suffix(".gcda").is_file():
        raise RuntimeError(f"missing executed coverage profile: {note}")
    destination = note.with_suffix(".gcov.json.gz")
    # Removing just this generated file prevents reuse after an unsuccessful gcov run.
    destination.unlink(missing_ok=True)
    result = subprocess.run([gcov, "--json-format", "-b", str(note)],
                            cwd=note.parent, check=True, capture_output=True, text=True)
    if result.stderr.strip():
        raise RuntimeError(f"gcov profile diagnostic for {note}: {result.stderr.strip()}")
    if not destination.is_file():
        raise RuntimeError(f"gcov did not create JSON for {note}")
    with gzip.open(destination, "rt", encoding="utf-8") as stream:
        return json.load(stream)


def counter(value: Any) -> int:
    if type(value) is not int or value < 0:
        raise RuntimeError(f"invalid gcov execution count: {value!r}")
    return value


class CoverageUnion:
    def __init__(self, root: Path, sources: list[Path]):
        self.root = root.resolve()
        self.hashes = source_hashes(sources)
        if not self.hashes:
            raise RuntimeError("coverage requires a nonempty owned source set")
        self.files: dict[str, dict[str, Any]] = {}
        self.metadata: tuple[str, str] | None = None
        self.profiles: list[dict[str, Any]] = []

    def add(self, label: str, document: dict[str, Any], *, sources: list[Path] | None = None) -> None:
        if label in {profile["name"] for profile in self.profiles}:
            raise RuntimeError(f"duplicate coverage profile label: {label}")
        metadata = (str(document["format_version"]), document["gcc_version"])
        if metadata[0] not in {"1", "2"}:
            raise RuntimeError(f"unsupported gcov JSON format: {metadata[0]}")
        if self.metadata is not None and self.metadata != metadata:
            raise RuntimeError("coverage profiles use different GCC versions or JSON formats")
        if source_hashes([Path(path) for path in self.hashes]) != self.hashes:
            raise RuntimeError("owned source changed during coverage measurement")
        declared = list(self.hashes) if sources is None else [str(path.resolve()) for path in sources]
        if not declared or len(set(declared)) != len(declared) or not set(declared) <= self.hashes.keys():
            raise RuntimeError("coverage profile must declare a nonempty, unique owned source scope")
        expected = {Path(path).relative_to(self.root).as_posix() for path in declared}
        cwd = Path(document["current_working_directory"])
        incoming: dict[str, dict[str, Any]] = {}
        for file in document["files"]:
            source = Path(file["file"])
            source = (source if source.is_absolute() else cwd / source).resolve()
            if str(source) not in self.hashes:
                continue
            path = source.relative_to(self.root).as_posix()
            if path in incoming:
                raise RuntimeError(f"duplicate owned file in gcov profile: {path}")
            functions = {}
            for function in file["functions"]:
                name = function["name"]
                if name in functions:
                    raise RuntimeError(f"duplicate gcov function: {path}:{name}")
                functions[name] = tuple(function[key] for key in (
                    "start_line", "start_column", "end_line", "end_column", "blocks"))
            lines, branches, shape = {}, {}, []
            for line in file["lines"]:
                number, function = line["line_number"], line.get("function_name")
                if type(number) is not int or number <= 0 or number in lines:
                    raise RuntimeError(f"invalid or duplicate gcov line: {path}:{number}")
                if function is None or function not in functions:
                    raise RuntimeError(f"unidentified -O0 gcov function: {path}:{number}")
                lines[number] = counter(line["count"])
                outcomes = []
                for index, branch in enumerate(line["branches"]):
                    if metadata[0] == "2":
                        edge = (branch["source_block_id"], branch["destination_block_id"])
                    else:
                        edge = (index,)
                    flags = (branch["fallthrough"], branch["throw"])
                    key = (function, number, *edge)
                    if key in branches:
                        raise RuntimeError(f"duplicate gcov branch identity: {path}:{key}")
                    branches[key] = counter(branch["count"])
                    outcomes.append((edge, flags))
                shape.append((number, function, tuple(line.get("block_ids", [])), tuple(outcomes)))
            signature = (tuple(sorted(functions.items())), tuple(sorted(shape)))
            if path in self.files and self.files[path]["signature"] != signature:
                raise RuntimeError(f"incompatible source/control-flow coverage profiles: {path}")
            incoming[path] = {"signature": signature, "lines": lines, "branches": branches}
        if set(incoming) != expected:
            raise RuntimeError(f"gcov source scope mismatch: missing={sorted(expected - set(incoming))}; "
                               f"unexpected={sorted(set(incoming) - expected)}")
        # Validate the entire profile before changing the accumulated union.
        contribution = {"name": label, "new_lines": 0, "new_branches": 0,
                        "covered_lines": 0, "covered_branches": 0, "compiled_sources": sorted(expected)}
        for path, file in incoming.items():
            if path not in self.files:
                self.files[path] = {"signature": file["signature"],
                                    "lines": dict.fromkeys(file["lines"], 0),
                                    "branches": dict.fromkeys(file["branches"], 0)}
            for kind in ("lines", "branches"):
                for key, count in file[kind].items():
                    contribution["covered_" + kind] += int(count > 0)
                    contribution["new_" + kind] += int(count > 0 and self.files[path][kind][key] == 0)
                    self.files[path][kind][key] += count
        self.metadata = metadata
        self.profiles.append(contribution)

    def report(self) -> dict[str, Any]:
        if not self.profiles:
            raise RuntimeError("cannot report coverage without executed profiles")
        expected = {Path(path).relative_to(self.root).as_posix() for path in self.hashes}
        if set(self.files) != expected:
            raise RuntimeError(f"missing owned source definitions in union: {sorted(expected - self.files.keys())}")
        if source_hashes([Path(path) for path in self.hashes]) != self.hashes:
            raise RuntimeError("owned source changed before coverage report")
        files = {}
        total = {"lines": 0, "covered_lines": 0, "branches": 0, "covered_branches": 0}
        for path, file in sorted(self.files.items()):
            summary = {kind: len(file[kind]) for kind in ("lines", "branches")}
            summary.update({"covered_" + kind: sum(count > 0 for count in file[kind].values())
                            for kind in ("lines", "branches")})
            for key, count in summary.items():
                total[key] += count
            summary["uncovered_lines"] = sorted(key for key, count in file["lines"].items() if count == 0)
            summary["uncovered_branches"] = [list(key) for key, count in sorted(file["branches"].items())
                                             if count == 0]
            files[path] = summary
        for summary in [*files.values(), total]:
            for kind in ("lines", "branches"):
                summary[kind + "_percent"] = (100.0 * summary["covered_" + kind] / summary[kind]
                                             if summary[kind] else 100.0)
        return {"gcc_version": self.metadata[1], "gcov_format": self.metadata[0],
                "source_sha256": {Path(path).relative_to(self.root).as_posix(): digest
                                  for path, digest in self.hashes.items()},
                "profiles": self.profiles, "files": files, "total": total}
