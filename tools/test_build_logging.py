#!/usr/bin/env python3
"""Exercise parallel direct/response build logging without running compilers."""
from concurrent.futures import ThreadPoolExecutor
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
from subprocess import CompletedProcess
from tempfile import TemporaryDirectory
from time import sleep
from unittest.mock import patch

import build


class YieldingOutput(StringIO):
	def write(self, value: str) -> int:
		# Force context switches between parts of print, including its newline.
		sleep(0.0001)
		return super().write(value)


def main() -> None:
	output = YieldingOutput()
	parent = build.ROOT / "out"
	parent.mkdir(exist_ok=True)
	with TemporaryDirectory(prefix="build-log-test-", dir=parent) as directory:
		root = Path(directory).resolve()
		assert root.parent == parent.resolve()
		expected = set()
		commands = []
		for i in range(64):
			command = ["test-compiler", f"job-{i}"]
			response = root / f"job-{i}.rsp"
			if i % 2:
				command.append("x" * 25000)
				expected.add(f"[build] test-compiler @{response.relative_to(build.ROOT)} arguments=2")
			else:
				expected.add(f"[build] test-compiler job-{i}")
			commands.append((command, response))
		seen = []
		def compiler(actual, **kwargs):
			assert kwargs["cwd"] == build.ROOT
			if actual[1].startswith("@"):
				assert Path(actual[1][1:]).is_file()
			seen.append(actual)
			return CompletedProcess(actual, 0)
		with redirect_stdout(output), patch.object(build.subprocess, "run", compiler):
			with ThreadPoolExecutor(max_workers=8) as workers:
				results = list(workers.map(lambda p: build._run_compiler(*p), commands))
		assert len(seen) == 64 and all(r.returncode == 0 for r in results)
		lines = output.getvalue().splitlines()
		assert len(lines) == 64 and set(lines) == expected
		assert not list(root.glob("*.rsp"))
	print("Parallel build logging: 64 jobs, 32 direct and 32 response records, 8 threads; exact complete lines and compiler dispatch preserved (mocked)")


if __name__ == "__main__":
	main()
