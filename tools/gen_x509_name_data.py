#!/usr/bin/env python3

"""生成 RFC 4518 使用的 Unicode 3.2 名称比较数据。"""

from __future__ import annotations

import argparse
import stringprep
import unicodedata
from pathlib import Path

from xrt_text import write_utf8


ROOT = Path(__file__).resolve().parents[1]
TARGET = ROOT / "src" / "internal" / "xrt_x509_name_data.h"
ORACLE_TARGET = ROOT / "tests" / "fixtures" / "x509_name_oracle.h"
UNICODE = unicodedata.ucd_3_2_0



def _ranges(values: list[int]) -> list[tuple[int, int]]:
	"""把有序码点压缩为闭区间。"""

	result: list[list[int]] = []
	for value in values:
		if not result or value > result[-1][1] + 1:
			result.append([value, value])
		else:
			result[-1][1] = value
	return [(first, last) for first, last in result]



def _pack(fields: list[tuple[int, int]]) -> list[int]:
	"""把定宽无符号字段按低位优先顺序压入 uint32 字。"""

	words: list[int] = []
	buffer = 0
	bits = 0
	for value, width in fields:
		assert width > 0 and 0 <= value < (1 << width)
		buffer |= value << bits
		bits += width
		while bits >= 32:
			words.append(buffer & 0xFFFFFFFF)
			buffer >>= 32
			bits -= 32
	if bits:
		words.append(buffer)

	# 解码跨字字段时可以直接读取下一个字，不需要在热路径检查尾界。
	words.append(0)
	return words



def _format_packed(
	name: str,
	fields: list[tuple[int, int]],
) -> list[str]:
	"""格式化紧凑位流，保持生成结果可重复且便于检查。"""

	words = _pack(fields)
	lines = [f"static const uint32 {name}[] = {{"]
	for offset in range(0, len(words), 6):
		part = words[offset:offset + 6]
		suffix = "," if offset + 6 < len(words) else ""
		lines.append(
			"\t" + ", ".join(f"0x{x:08X}u" for x in part) + suffix
		)
	lines.append("};")
	return lines



def _scalar_fields(values: list[int]) -> list[tuple[int, int]]:
	"""返回 Unicode 标量位流字段。"""

	return [(value, 21) for value in values]



def _record_fields(
	records: list[tuple[int, ...]],
	widths: tuple[int, ...],
) -> list[tuple[int, int]]:
	"""把同构记录展开为连续位流字段。"""

	return [
		(value, width)
		for record in records
		for value, width in zip(record, widths)
	]



def _der_length(size: int) -> bytes:
	"""编码测试向量使用的最短 DER 长度。"""

	if size < 0x80:
		return bytes([size])
	if size < 0x100:
		return bytes([0x81, size])
	return bytes([0x82, size >> 8, size & 0xFF])



def _der_value(tag: int, value: bytes) -> bytes:
	"""编码一项测试用 DER TLV。"""

	return bytes([tag]) + _der_length(len(value)) + value



def _der_name(text: str) -> bytes:
	"""把 UTF-8 Common Name 编码为完整规范 DER Name。"""

	oid = _der_value(0x06, bytes([0x55, 0x04, 0x03]))
	value = _der_value(0x0C, text.encode("utf-8"))
	attribute = _der_value(0x30, oid + value)
	rdn = _der_value(0x31, attribute)
	return _der_value(0x30, rdn)



def _prohibited(text: str) -> bool:
	"""判断文本是否包含 RFC 4518 禁止码点。"""

	return any(
		stringprep.in_table_a1(char) or
		stringprep.in_table_c8(char) or
		stringprep.in_table_c3(char) or
		stringprep.in_table_c4(char) or
		stringprep.in_table_c5(char) or
		ord(char) == 0xFFFD
		for char in text
	)



def _prepare(text: str) -> str | None:
	"""使用标准库 Unicode 3.2 数据执行 oracle StringPrep 核心步骤。"""

	mapped = "".join(stringprep.map_table_b2(char) for char in text)
	result = UNICODE.normalize("NFKC", mapped)
	return None if _prohibited(result) else result



def _spread(values: list[int], count: int) -> list[int]:
	"""从有序全集中确定性均匀抽样。"""

	if len(values) <= count:
		return values
	return [values[(index * len(values)) // count] for index in range(count)]



def _oracle_content() -> str:
	"""生成覆盖 case-fold、NFKC 和组合顺序的 Name oracle 向量。"""

	casefold: list[int] = []
	normalize: list[int] = []
	pairs: list[tuple[str, str]] = []
	seen: set[tuple[str, str]] = set()

	for code in range(0x110000):
		text = chr(code)
		prepared = _prepare(text)
		if not prepared or _prepare(prepared) != prepared:
			continue
		if stringprep.map_table_b2(text) != text:
			casefold.append(code)
		if UNICODE.normalize("NFKC", text) != text:
			normalize.append(code)

	for code in _spread(casefold, 96) + _spread(normalize, 96):
		left = chr(code)
		right = _prepare(left)

		assert right is not None
		pair = (left, right)
		if pair not in seen:
			seen.add(pair)
			pairs.append(pair)

	for pair in [
		("A\u0315\u0300", "\u00E0\u0315"),
		("a\u0323\u0307", "\u1EA1\u0307"),
		("\u1100\u1161", "\uAC00"),
		("\u1100\u1161\u11A8", "\uAC01"),
	]:
		left = _prepare(pair[0])
		right = _prepare(pair[1])

		assert left is not None and right is not None and left == right
		if pair not in seen:
			seen.add(pair)
			pairs.append(pair)

	data = bytearray()
	vectors: list[tuple[int, int, int, int]] = []
	for left, right in pairs:
		left_der = _der_name(left)
		right_der = _der_name(right)
		left_offset = len(data)
		data.extend(left_der)
		right_offset = len(data)
		data.extend(right_der)
		vectors.append((
			left_offset, len(left_der), right_offset, len(right_der)
		))

	lines = [
		"#ifndef XRT_TEST_X509_NAME_ORACLE_H",
		"#define XRT_TEST_X509_NAME_ORACLE_H",
		"",
		"",
		"",
		"/* 此文件由 tools/gen_x509_name_data.py 根据 Unicode 3.2 生成。 */",
		"typedef struct xrt_test_x509_name_oracle {",
		"\tuint32 LeftOffset;",
		"\tuint16 LeftSize;",
		"\tuint32 RightOffset;",
		"\tuint16 RightSize;",
		"} xrt_test_x509_name_oracle;",
		"",
		"",
		"",
		"static const uint8 X509_NAME_ORACLE_DATA[] = {",
	]
	for offset in range(0, len(data), 12):
		part = data[offset:offset + 12]
		suffix = "," if offset + 12 < len(data) else ""
		lines.append(
			"\t" + ", ".join(f"0x{value:02X}" for value in part) + suffix
		)
	lines.extend([
		"};", "", "", "",
		"static const xrt_test_x509_name_oracle X509_NAME_ORACLE_VECTORS[] = {",
	])
	for left_offset, left_size, right_offset, right_size in vectors:
		lines.append(
			f"\t{{ {left_offset}u, {left_size}u, "
			f"{right_offset}u, {right_size}u }},"
		)
	lines.extend(["};", "", "#endif", ""])
	return "\n".join(lines)



def _content() -> str:
	"""生成完整内部头文件内容。"""

	mappings: list[tuple[int, int, int]] = []
	mapping_data: list[int] = []
	decompositions: list[tuple[int, int, int]] = []
	decomposition_data: list[int] = []
	combining: list[tuple[int, int]] = []
	marks: list[int] = []
	compositions: list[tuple[int, int, int]] = []
	prohibited: list[int] = []

	for code in range(0x110000):
		text = chr(code)
		mapped = tuple(ord(char) for char in stringprep.map_table_b2(text))
		if mapped != (code,):
			mappings.append((code, len(mapping_data), len(mapped)))
			mapping_data.extend(mapped)

		raw = UNICODE.decomposition(text)
		if raw:
			fields = [field for field in raw.split() if not field.startswith("<")]
			values = tuple(int(field, 16) for field in fields)
			decompositions.append((code, len(decomposition_data), len(values)))
			decomposition_data.extend(values)

		value = UNICODE.combining(text)
		if value:
			combining.append((code, value))
		if UNICODE.category(text) in ("Mn", "Mc", "Me"):
			marks.append(code)

		if (
			stringprep.in_table_a1(text) or
			stringprep.in_table_c8(text) or
			stringprep.in_table_c3(text) or
			stringprep.in_table_c4(text) or
			stringprep.in_table_c5(text) or
			code == 0xFFFD
		):
			prohibited.append(code)

	for code, offset, size in decompositions:
		raw = UNICODE.decomposition(chr(code))
		values = decomposition_data[offset:offset + size]
		if (
			not raw.startswith("<") and
			size == 2 and
			UNICODE.normalize("NFC", "".join(chr(value) for value in values)) == chr(code)
		):
			compositions.append((values[0], values[1], code))
	compositions.sort()

	lines = [
		"#ifndef XRT_INTERNAL_X509_NAME_DATA_H",
		"#define XRT_INTERNAL_X509_NAME_DATA_H",
		"",
		"",
		"",
		"#if defined(XRT_FEATURE_X509_NAME)",
		"",
		"/* 此文件由 tools/gen_x509_name_data.py 根据 Python Unicode 3.2 数据生成。 */",
		"/* 码点使用 21 位，映射偏移使用 14 位，映射长度使用 5 位。 */",
		f"#define XRT_X509_NAME_MAP_COUNT {len(mappings)}u",
		f"#define XRT_X509_NAME_DECOMPOSITION_COUNT {len(decompositions)}u",
		f"#define XRT_X509_NAME_CLASS_COUNT {len(combining)}u",
		f"#define XRT_X509_NAME_MARK_COUNT {len(_ranges(marks))}u",
		f"#define XRT_X509_NAME_COMPOSITION_COUNT {len(compositions)}u",
		f"#define XRT_X509_NAME_PROHIBITED_COUNT {len(_ranges(prohibited))}u",
		"",
		"",
		"",
	]
	lines.extend(_format_packed(
		"__xrtX509NameMapData", _scalar_fields(mapping_data)
	))
	lines.extend(["", "", ""])
	lines.extend(_format_packed(
		"__xrtX509NameMaps",
		_record_fields(mappings, (21, 14, 5)),
	))
	lines.extend(["", "", ""])
	lines.extend(_format_packed(
		"__xrtX509NameDecompositionData",
		_scalar_fields(decomposition_data),
	))
	lines.extend(["", "", ""])
	lines.extend(_format_packed(
		"__xrtX509NameDecompositions",
		_record_fields(decompositions, (21, 14, 5)),
	))
	lines.extend(["", "", ""])
	lines.extend(_format_packed(
		"__xrtX509NameClasses",
		_record_fields(combining, (21, 8)),
	))
	lines.extend(["", "", ""])
	lines.extend(_format_packed(
		"__xrtX509NameMarks",
		_record_fields(_ranges(marks), (21, 21)),
	))
	lines.extend(["", "", ""])
	lines.extend(_format_packed(
		"__xrtX509NameCompositions",
		_record_fields(compositions, (21, 21, 21)),
	))
	lines.extend(["", "", ""])
	lines.extend(_format_packed(
		"__xrtX509NameProhibited",
		_record_fields(_ranges(prohibited), (21, 21)),
	))
	lines.extend(["", "#endif", "", "#endif", ""])
	return "\n".join(lines)



def main() -> int:
	"""写入、检查或输出首个 apply_patch。"""

	parser = argparse.ArgumentParser()
	parser.add_argument("--check", action="store_true")
	parser.add_argument("--patch", action="store_true")
	parser.add_argument("--oracle-patch", action="store_true")
	args = parser.parse_args()
	content = _content()
	oracle = _oracle_content()

	if args.check:
		return 0 if (
			TARGET.exists() and
			TARGET.read_text(encoding="utf-8") == content and
			ORACLE_TARGET.exists() and
			ORACLE_TARGET.read_text(encoding="utf-8") == oracle
		) else 1
	if args.patch:
		print("*** Begin Patch")
		print(f"*** Add File: {TARGET}")
		for line in content.splitlines():
			print("+" + line)
		print("*** End Patch")
		return 0
	if args.oracle_patch:
		print("*** Begin Patch")
		print(f"*** Add File: {ORACLE_TARGET}")
		for line in oracle.splitlines():
			print("+" + line)
		print("*** End Patch")
		return 0
	write_utf8(TARGET, content)
	write_utf8(ORACLE_TARGET, oracle)
	return 0



if __name__ == "__main__":
	raise SystemExit(main())
