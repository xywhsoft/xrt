#!/usr/bin/env python3

"""维护旧版文件完整性和逐文件迁移台账。"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from xrt_text import write_utf8


ROOT = Path(__file__).resolve().parents[1]
BASELINE_ROOT = ROOT / "dev" / "ver1"
LEDGER = ROOT / "dev" / "refactor" / "baseline.json"
RELOCATIONS = ROOT / "dev" / "refactor" / "relocations.json"
MANIFEST = ROOT / "config" / "modules.json"
GENERATED_SUFFIXES = {".exe", ".o", ".obj", ".log", ".err", ".i"}
SOURCE_SUFFIXES = {".c", ".h"}
SCRIPT_SUFFIXES = {".bat", ".sh", ".ps1", ".py"}
DOCUMENT_SUFFIXES = {".md", ".txt"}
AUDIT_RANK = {"pending": 0, "reviewed": 1, "migrated": 2, "verified": 3}
AUDIT_DECISIONS = {"retain", "refine", "replace", "merge", "retire"}
CURRENT_ASSET_KEYS = (
	"public_headers", "internal_headers", "sources", "tests",
	"single_tests", "fuzz_sources", "examples", "benchmarks", "docs",
)
CURRENT_PATH_PREFIXES = (
	".github/", "config/", "dev/bench/", "docs/", "examples/", "fuzz/",
	"extlibs/", "include/", "single/", "src/", "tests/", "tools/",
)
CURRENT_ROOT_ASSETS = {"LICENSE", "README.md", "build.bat", "build.sh"}



def _normalize_relative_path(path: str) -> str:
	"""规范化台账路径，并兼容 Python 3.8。"""

	normalized = Path(path).as_posix()
	return normalized[2:] if normalized.startswith("./") else normalized



def _kind(path: Path) -> str:
	"""按照用途初步分类旧版文件。"""

	if path.suffix.lower() in SOURCE_SUFFIXES:
		return "source"
	if path.suffix.lower() in SCRIPT_SUFFIXES:
		return "script"
	if path.suffix.lower() in DOCUMENT_SUFFIXES:
		return "document"
	if path.suffix.lower() in GENERATED_SUFFIXES:
		return "generated"
	return "asset"



def _line_count(data: bytes) -> int | None:
	"""统计文本行数，二进制文件不产生伪造行数。"""

	if b"\0" in data:
		return None
	return len(data.splitlines())



def _is_current_path(value: str) -> bool:
	"""判断一个台账引用是否明确指向当前仓库文件。"""

	return (value in CURRENT_ROOT_ASSETS) or value.startswith(
		CURRENT_PATH_PREFIXES
	)



def _normalize_values(values: list[str], split_path_commas: bool) -> list[str]:
	"""把早期命令写入的分号或路径逗号列表还原为独立值。"""

	result: list[str] = []
	for value in values or []:
		for part in value.split(";"):
			part = part.strip()
			if not part:
				continue

			parts = [item.strip() for item in part.split(",")]
			if split_path_commas and (len(parts) > 1) and all(
				_is_current_path(item) for item in parts
			):
				result.extend(parts)
			else:
				result.append(part)
	return list(dict.fromkeys(result))



def _normalize_modules(values: list[str]) -> list[str]:
	"""把模块参数中的分号和逗号列表还原为独立模块名。"""

	result: list[str] = []
	for value in values or []:
		for part in value.replace(";", ",").split(","):
			part = part.strip()
			if part:
				result.append(part)
	return list(dict.fromkeys(result))



def _normalize_record(record: dict) -> None:
	"""规范化一个文件级或区间级审计记录。"""

	record["module"] = _normalize_modules(record.get("module", []))
	record["target"] = _normalize_values(record.get("target", []), True)
	record["evidence"] = _normalize_values(record.get("evidence", []), True)



def _scan() -> list[dict]:
	"""扫描旧版树并计算稳定哈希。"""

	entries: list[dict] = []
	for path in sorted(item for item in BASELINE_ROOT.rglob("*") if item.is_file()):
		data = path.read_bytes()
		entries.append({
			"path": path.relative_to(BASELINE_ROOT).as_posix(),
			"kind": _kind(path),
			"size": len(data),
			"lines": _line_count(data),
			"sha256": hashlib.sha256(data).hexdigest(),
			"audit": "pending",
			"decision": None,
			"module": [],
			"target": [],
			"evidence": [],
			"note": None,
		})
	return entries



def _normalize(data: dict) -> dict:
	"""补齐旧台账字段，并把已完成的整文件结论转换为完整区间。"""

	for item in data["files"]:
		item.setdefault("audit", "pending")
		item.setdefault("decision", None)
		item.setdefault("module", [])
		item.setdefault("target", [])
		item.setdefault("evidence", [])
		item.setdefault("note", None)
		_normalize_record(item)
		if "segments" not in item:
			item["segments"] = []
			if (item["lines"] is not None) and (item["audit"] != "pending"):
				item["segments"].append({
					"start": 1,
					"end": item["lines"],
					"audit": item["audit"],
					"decision": item["decision"],
					"module": item["module"],
					"target": item["target"],
					"evidence": item["evidence"],
					"note": item["note"],
				})
		for segment in item["segments"]:
			_normalize_record(segment)
	data["schema"] = 3
	return data



def _load() -> dict:
	"""读取并规范化迁移台账。"""

	if not LEDGER.exists():
		raise SystemExit("baseline does not exist")
	return _normalize(json.loads(LEDGER.read_text(encoding="utf-8")))



def _load_modules() -> list[dict]:
	"""读取模块清单，供旧资产覆盖检查使用。"""

	if not MANIFEST.exists():
		raise SystemExit("module manifest does not exist")
	return json.loads(MANIFEST.read_text(encoding="utf-8"))["modules"]



def _load_relocations() -> dict:
	"""读取模块迁出后的清单边界、产品根目录与少量特殊重定位。"""

	if not RELOCATIONS.is_file():
		raise SystemExit("refactor relocation registry does not exist")
	data = json.loads(RELOCATIONS.read_text(encoding="utf-8"))
	if data.get("schema") != 1:
		raise SystemExit("unsupported refactor relocation registry schema")
	for key in ("manifests", "roots", "historical_modules", "path_overrides"):
		if key not in data:
			raise SystemExit(f"refactor relocation registry has no {key}")
	return data



def _load_audit_module_names(relocations: dict) -> set[str]:
	"""读取核心、扩展、备份与历史模块名，供迁移记录校验。"""

	names = {module["name"] for module in _load_modules()}
	for value in relocations["manifests"]:
		path = ROOT / value
		if not path.is_file():
			raise SystemExit(f"audit module manifest does not exist: {value}")
		data = json.loads(path.read_text(encoding="utf-8"))
		names.update(module["name"] for module in data.get("modules", []))
	names.update(relocations["historical_modules"])
	return names



def _resolve_current_path(value: str, relocations: dict) -> tuple[str | None, str | None]:
	"""解析当前资产；模块迁出后只接受登记根目录中的唯一同路径资产。"""

	if (ROOT / value).is_file():
		return value, None

	override = relocations["path_overrides"].get(value)
	if override is not None:
		if (ROOT / override).is_file():
			return override, None
		return None, f"registered relocation is missing: {value} -> {override}"

	candidates = [
		(Path(root) / value).as_posix()
		for root in relocations["roots"]
		if (ROOT / root / value).is_file()
	]
	if len(candidates) == 1:
		return candidates[0], None
	if len(candidates) > 1:
		return None, f"ambiguous relocated path: {value} -> {candidates}"
	return None, None



def _validate_module_assets(entries: dict[str, dict]) -> list[str]:
	"""验证模块声明引用的旧资产和当前证据文件都存在。"""

	errors: list[str] = []
	names: set[str] = set()
	for module in _load_modules():
		name = module["name"]
		if name in names:
			errors.append(f"duplicate module name: {name}")
		names.add(name)
		for path in module.get("legacy", []):
			normalized = _normalize_relative_path(path)
			if normalized not in entries:
				errors.append(f"{name}: legacy asset is not in baseline: {normalized}")
		for key in CURRENT_ASSET_KEYS:
			for path in module.get(key, []):
				if not (ROOT / path).is_file():
					errors.append(f"{name}: current {key} asset is missing: {path}")
		if (module.get("state") == "stable") and not module.get("docs"):
			errors.append(f"{name}: stable module has no authoritative document")
	return errors



def _validate_audit_record(
	path: str,
	location: str,
	record: dict,
	modules: set[str],
	relocations: dict,
) -> list[str]:
	"""验证一项审计结论不会脱离承接实现和验证证据。"""

	errors: list[str] = []
	audit = record.get("audit")
	decision = record.get("decision")
	prefix = f"{path}{location}"

	if audit not in AUDIT_RANK:
		return [f"{prefix}: invalid audit state: {audit}"]
	if audit == "pending":
		return errors
	if decision not in AUDIT_DECISIONS:
		errors.append(f"{prefix}: invalid audit decision: {decision}")

	unknown = sorted(set(record.get("module", [])) - modules)
	if unknown:
		errors.append(f"{prefix}: unknown modules: {unknown}")

	if audit != "verified":
		return errors

	for key in ("module", "target", "evidence", "note"):
		if not record.get(key):
			errors.append(f"{prefix}: verified record has no {key}")

	for target in record.get("target", []):
		if not _is_current_path(target):
			errors.append(f"{prefix}: invalid current target: {target}")
		else:
			resolved, relocation_error = _resolve_current_path(target, relocations)
			if relocation_error is not None:
				errors.append(f"{prefix}: {relocation_error}")
			elif resolved is None:
				errors.append(f"{prefix}: current target is missing: {target}")

	for evidence in record.get("evidence", []):
		if (not any(ch.isspace() for ch in evidence)) and _is_current_path(evidence):
			resolved, relocation_error = _resolve_current_path(evidence, relocations)
			if relocation_error is not None:
				errors.append(f"{prefix}: {relocation_error}")
			elif resolved is None:
				errors.append(f"{prefix}: evidence file is missing: {evidence}")
	return errors



def _validate_audit_records(data: list[dict]) -> list[str]:
	"""验证全部文件和区间结论的状态、模块及当前证据。"""

	relocations = _load_relocations()
	modules = _load_audit_module_names(relocations)
	errors: list[str] = []
	for item in data:
		segments = item["segments"]
		if segments:
			for segment in segments:
				location = f":{segment['start']}-{segment['end']}"
				errors.extend(
					_validate_audit_record(
						item["path"], location, segment, modules, relocations
					)
				)
		else:
			errors.extend(
				_validate_audit_record(
					item["path"], "", item, modules, relocations
				)
			)
	return errors



def _write(data: dict) -> None:
	"""以稳定格式写回迁移台账。"""

	write_utf8(
		LEDGER,
		json.dumps(data, ensure_ascii=False, indent=2) + "\n",
	)



def _validate_segments(item: dict) -> list[str]:
	"""验证一个文本文件的审计区间合法且互不重叠。"""

	errors: list[str] = []
	previous_end = 0
	for segment in sorted(item["segments"], key=lambda value: (value["start"], value["end"])):
		start = segment["start"]
		end = segment["end"]
		if (item["lines"] is None) or (start < 1) or (end < start) or (end > item["lines"]):
			errors.append(f"{item['path']}: invalid segment {start}-{end}")
		elif start <= previous_end:
			errors.append(f"{item['path']}: overlapping segment {start}-{end}")
		previous_end = max(previous_end, end)
	return errors



def _refresh_file_audit(item: dict) -> None:
	"""在区间覆盖整文件时汇总文件级结论。"""

	segments = sorted(item["segments"], key=lambda value: value["start"])
	if (item["lines"] is None) or not segments:
		return
	position = 1
	for segment in segments:
		if segment["start"] != position:
			item["audit"] = "pending"
			item["decision"] = None
			return
		position = segment["end"] + 1
	if position != (item["lines"] + 1):
		item["audit"] = "pending"
		item["decision"] = None
		return

	item["audit"] = min(segments, key=lambda value: AUDIT_RANK[value["audit"]])["audit"]
	decisions = {segment["decision"] for segment in segments}
	item["decision"] = decisions.pop() if len(decisions) == 1 else "mixed"
	item["module"] = sorted({value for segment in segments for value in segment["module"]})
	item["target"] = sorted({value for segment in segments for value in segment["target"]})
	item["evidence"] = sorted({value for segment in segments for value in segment["evidence"]})
	item["note"] = "由逐行区间审计汇总。"



def _merge_snapshot(
	stored: list[dict],
	current: list[dict],
) -> tuple[list[dict], dict[str, int]]:
	"""增量合并旧版快照，只保留内容未变化文件的审计证据。"""

	stored_by_path = {item["path"]: item for item in stored}
	current_paths = {item["path"] for item in current}
	counts = {
		"preserved": 0,
		"changed": 0,
		"added": 0,
		"removed": len(set(stored_by_path) - current_paths),
	}
	merged: list[dict] = []
	for item in current:
		previous = stored_by_path.get(item["path"])
		if (previous is not None) and (previous["sha256"] == item["sha256"]):
			merged.append(previous)
			counts["preserved"] += 1
			continue

		fresh = dict(item)
		fresh["segments"] = []
		merged.append(fresh)
		counts["changed" if previous is not None else "added"] += 1
	return merged, counts



def snapshot(refresh: bool) -> int:
	"""建立只读旧版基线。"""

	if LEDGER.exists() and not refresh:
		raise SystemExit("baseline already exists; use --refresh explicitly")
	entries = _scan()
	counts = None
	if LEDGER.exists():
		entries, counts = _merge_snapshot(_load()["files"], entries)
	data = _normalize({
		"schema": 3,
		"root": "dev/ver1",
		"files": entries,
	})
	LEDGER.parent.mkdir(parents=True, exist_ok=True)
	_write(data)
	if counts is not None:
		print(
			"[refresh] "
			f"preserved={counts['preserved']} "
			f"changed={counts['changed']} "
			f"added={counts['added']} "
			f"removed={counts['removed']}"
		)
	print(f"[snapshot] files={len(entries)} ledger={LEDGER.relative_to(ROOT)}")
	return 0



def check() -> int:
	"""确认旧版参考树没有被重构过程修改。"""

	if not LEDGER.exists():
		raise SystemExit("baseline does not exist")
	failed = False
	stored = _load()["files"]
	current = _scan()
	stored_hashes = {item["path"]: item["sha256"] for item in stored}
	current_hashes = {item["path"]: item["sha256"] for item in current}
	if stored_hashes != current_hashes:
		missing = sorted(stored_hashes.keys() - current_hashes.keys())
		added = sorted(current_hashes.keys() - stored_hashes.keys())
		changed = sorted(
			path for path in (stored_hashes.keys() & current_hashes.keys())
			if stored_hashes[path] != current_hashes[path]
		)
		print(f"[failed] missing={missing} added={added} changed={changed}")
		failed = True
	entries = {item["path"]: item for item in stored}
	errors = [error for item in stored for error in _validate_segments(item)]
	errors.extend(_validate_audit_records(stored))
	errors.extend(_validate_module_assets(entries))
	if errors:
		for error in errors:
			print(f"[failed] {error}")
		failed = True
	else:
		print(f"[pass] audit records files={len(stored)}")
	if failed:
		return 1
	print(f"[pass] legacy baseline files={len(stored)}")
	return 0



def _module_asset_audit(item: dict, module: str) -> str:
	"""汇总一个旧资产对指定模块已经达到的审计阶段。"""

	states = [
		segment["audit"] for segment in item["segments"]
		if module in segment["module"]
	]
	if states:
		return min(states, key=lambda value: AUDIT_RANK[value])
	if module in item["module"]:
		return item["audit"]
	return "pending"



def assets(selected: list[str], require_verified: bool) -> int:
	"""按模块显示旧实现、测试、文档和范例的审计闭环状态。"""

	entries = {item["path"]: item for item in _load()["files"]}
	modules = _load_modules()
	known = {module["name"] for module in modules}
	unknown = sorted(set(selected) - known)
	if unknown:
		raise SystemExit(f"unknown modules: {unknown}")

	failed = False
	for module in modules:
		name = module["name"]
		if selected and (name not in selected):
			continue
		counts = {state: 0 for state in AUDIT_RANK}
		missing: list[str] = []
		for path in module.get("legacy", []):
			normalized = _normalize_relative_path(path)
			item = entries.get(normalized)
			if item is None:
				missing.append(normalized)
				continue
			counts[_module_asset_audit(item, name)] += 1
		total = sum(counts.values())
		detail = " ".join(f"{state}={counts[state]}" for state in AUDIT_RANK)
		print(f"[assets] module={name} total={total} {detail}")
		for path in missing:
			print(f"[failed] {name}: missing legacy asset: {path}")
		if missing or (require_verified and (counts["verified"] != total)):
			failed = True
	return 1 if failed else 0



def summary() -> int:
	"""显示迁移台账状态。"""

	data = _load()["files"]
	counts: dict[str, int] = {}
	authored_lines = 0
	covered_lines = 0
	for item in data:
		counts[item["audit"]] = counts.get(item["audit"], 0) + 1
		if (item["lines"] is not None) and (item["kind"] != "generated"):
			authored_lines += item["lines"]
			covered_lines += sum(segment["end"] - segment["start"] + 1 for segment in item["segments"])
	for name in sorted(counts):
		print(f"{name}: {counts[name]}")
	print(f"audited authored lines: {covered_lines}/{authored_lines}")
	return 0



def upgrade() -> int:
	"""升级台账结构，不改变任何已有审计结论。"""

	data = _load()
	_write(data)
	print(f"[upgrade] schema={data['schema']} files={len(data['files'])}")
	return 0



def mark(
	paths: list[str],
	audit: str,
	decision: str,
	modules: list[str],
	targets: list[str],
	evidence: list[str],
	note: str | None,
) -> int:
	"""记录已经完成的逐文件审计结论和迁移证据。"""

	data = _load()
	entries = {item["path"]: item for item in data["files"]}
	normalized = [_normalize_relative_path(path) for path in paths]
	missing = [path for path in normalized if path not in entries]
	if missing:
		raise SystemExit(f"legacy paths not found: {missing}")
	segmented = []
	for path in normalized:
		item = entries[path]
		segments = item["segments"]
		if ((item["lines"] is not None) and segments and not (
			(len(segments) == 1) and
			(segments[0]["start"] == 1) and
			(segments[0]["end"] == item["lines"])
		)):
			segmented.append(path)
	if segmented:
		raise SystemExit(
			"fine-grained audit already exists; use revise-range: "
			f"{segmented}"
		)

	for path in normalized:
		item = entries[path]
		item["audit"] = audit
		item["decision"] = decision
		item["module"] = modules
		item["target"] = targets
		item["evidence"] = evidence
		item["note"] = note
		if item["lines"] is not None:
			item["segments"] = [{
				"start": 1,
				"end": item["lines"],
				"audit": audit,
				"decision": decision,
				"module": modules,
				"target": targets,
				"evidence": evidence,
				"note": note,
			}]
	_write(data)
	for path in normalized:
		print(f"[mark] {path} audit={audit} decision={decision}")
	return 0



def mark_range(
	path: str,
	start: int,
	end: int,
	audit: str,
	decision: str,
	modules: list[str],
	targets: list[str],
	evidence: list[str],
	note: str | None,
) -> int:
	"""记录跨模块旧文件中一个互不重叠的逐行审计结论。"""

	data = _load()
	normalized = _normalize_relative_path(path)
	entries = {item["path"]: item for item in data["files"]}
	if normalized not in entries:
		raise SystemExit(f"legacy path not found: {normalized}")
	item = entries[normalized]
	if (item["lines"] is None) or (start < 1) or (end < start) or (end > item["lines"]):
		raise SystemExit(f"invalid line range: {start}-{end}")
	for segment in item["segments"]:
		if not ((end < segment["start"]) or (start > segment["end"])):
			raise SystemExit(
				f"line range overlaps existing segment: {segment['start']}-{segment['end']}"
			)
	item["segments"].append({
		"start": start,
		"end": end,
		"audit": audit,
		"decision": decision,
		"module": modules,
		"target": targets,
		"evidence": evidence,
		"note": note,
	})
	item["segments"].sort(key=lambda value: value["start"])
	_refresh_file_audit(item)
	_write(data)
	print(f"[mark-range] {normalized}:{start}-{end} audit={audit} decision={decision}")
	return 0



def revise_range(
	path: str,
	start: int,
	end: int,
	audit: str,
	decision: str,
	modules: list[str],
	targets: list[str],
	evidence: list[str],
	note: str | None,
) -> int:
	"""重划已经审计的连续区间，并完整保留区间两侧的原结论。"""

	data = _load()
	normalized = _normalize_relative_path(path)
	entries = {item["path"]: item for item in data["files"]}
	if normalized not in entries:
		raise SystemExit(f"legacy path not found: {normalized}")
	item = entries[normalized]
	if (item["lines"] is None) or (start < 1) or (end < start) or (end > item["lines"]):
		raise SystemExit(f"invalid line range: {start}-{end}")

	position = start
	for segment in sorted(item["segments"], key=lambda value: value["start"]):
		if (segment["end"] < start) or (segment["start"] > end):
			continue
		if segment["start"] > position:
			raise SystemExit(f"line range contains an unaudited gap at {position}")
		position = min(end, segment["end"]) + 1
		if position > end:
			break
	if position <= end:
		raise SystemExit(f"line range contains an unaudited gap at {position}")

	segments: list[dict] = []
	for segment in item["segments"]:
		if (segment["end"] < start) or (segment["start"] > end):
			segments.append(segment)
			continue
		if segment["start"] < start:
			left = dict(segment)
			left["end"] = start - 1
			segments.append(left)
		if segment["end"] > end:
			right = dict(segment)
			right["start"] = end + 1
			segments.append(right)
	segments.append({
		"start": start,
		"end": end,
		"audit": audit,
		"decision": decision,
		"module": modules,
		"target": targets,
		"evidence": evidence,
		"note": note,
	})
	item["segments"] = sorted(segments, key=lambda value: value["start"])
	errors = _validate_segments(item)
	if errors:
		raise SystemExit(errors[0])
	_refresh_file_audit(item)
	_write(data)
	print(f"[revise-range] {normalized}:{start}-{end} audit={audit} decision={decision}")
	return 0



def main() -> int:
	"""解析并执行审计命令。"""

	parser = argparse.ArgumentParser()
	parser.add_argument("command", choices=[
		"snapshot", "check", "summary", "assets", "upgrade", "mark", "mark-range",
		"revise-range",
	])
	parser.add_argument("--refresh", action="store_true")
	parser.add_argument("--path", action="append", default=[])
	parser.add_argument("--audit", choices=["pending", "reviewed", "migrated", "verified"])
	parser.add_argument("--decision", choices=["retain", "refine", "replace", "merge", "retire"])
	parser.add_argument("--module", action="append", default=[])
	parser.add_argument("--target", action="append", default=[])
	parser.add_argument("--evidence", action="append", default=[])
	parser.add_argument("--note")
	parser.add_argument("--start", type=int)
	parser.add_argument("--end", type=int)
	parser.add_argument("--require-verified", action="store_true")
	args = parser.parse_args()
	if args.command == "snapshot":
		return snapshot(args.refresh)
	if args.command == "check":
		return check()
	if args.command == "summary":
		return summary()
	if args.command == "assets":
		return assets(args.module, args.require_verified)
	if args.command == "upgrade":
		return upgrade()
	if not args.path or args.audit is None or args.decision is None:
		raise SystemExit("mark requires --path, --audit and --decision")
	if args.command in {"mark-range", "revise-range"}:
		if (len(args.path) != 1) or (args.start is None) or (args.end is None):
			raise SystemExit(
				f"{args.command} requires one --path, --start and --end"
			)
		if args.command == "revise-range":
			return revise_range(
				args.path[0], args.start, args.end, args.audit, args.decision,
				args.module, args.target, args.evidence, args.note,
			)
		return mark_range(
			args.path[0], args.start, args.end, args.audit, args.decision,
			args.module, args.target, args.evidence, args.note,
		)
	return mark(args.path, args.audit, args.decision, args.module, args.target, args.evidence, args.note)



if __name__ == "__main__":
	raise SystemExit(main())
