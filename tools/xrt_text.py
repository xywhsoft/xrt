#!/usr/bin/env python3

"""提供构建工具共享的稳定文本写入约定。"""

from pathlib import Path



def write_utf8(path: Path, content: str) -> None:
	"""使用 UTF-8 和 LF 写入文本，兼容 Python 3.8 及后续版本。"""

	with path.open("w", encoding="utf-8", newline="\n") as stream:
		stream.write(content)
