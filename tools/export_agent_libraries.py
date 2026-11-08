#!/usr/bin/env python3
"""Export the agent libraries to the flat XS/TCC SDK layout, or verify it.

Implementation bodies are preserved. Only public-header guards, export
annotations, and include spellings change. The core and transport bridges
remain owned by the destination host; this tool never exports an XRT
implementation. Run against a committed XRT checkout so UPSTREAM.txt identifies
the complete source revision.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
LIBRARIES = ("xllm", "xllm-session", "xwork")


def text(path: Path) -> str:
    return path.read_text(encoding="utf-8").replace("\r\n", "\n")


def export_files(name: str, revision: str) -> dict[str, str]:
    root = ROOT / "extlibs" / name
    api = text(root / "include" / name / "api.h")
    body = api[api.index("#ifdef __cplusplus"):]
    body = body[:body.rindex("#endif /* selected")].rstrip()
    body = body.replace("XRT_API ", "")
    guard = name.upper().replace("-", "_") + "_H"
    includes = "#include <stdbool.h>\n#include <stddef.h>\n#include <stdint.h>\n"
    if name != "xllm":
        includes += '#include "xllm.h"\n#include "xllm-executor.h"\n'
    if name == "xwork":
        includes += '#include "xllm-session.h"\n'
    files = {name + ".h": f"#ifndef {guard}\n#define {guard}\n\n" + includes +
             "\n" + body + "\n\n#endif\n"}
    manifest = json.loads(text(root / "config/modules.json"))
    files[name + ".c"] = "".join(
        '#include "src/' + Path(source).name + '"\n'
        for source in manifest["modules"][0]["sources"])
    for path in sorted((root / "src").rglob("*")):
        if path.suffix not in {".c", ".h"}:
            continue
        value = text(path)
        value = re.sub(r'#include "../internal/([^"\n]+)"', r'#include "\1"', value)
        value = value.replace(f"#include <{name}.h>",
                              f'#include "../{name}.h"\n#include "../{name}-xrt.h"')
        value = value.replace('#include "../../../xllm/src/internal/xllm_time.h"',
                              '#include "../../xllm/xllm-time.h"')
        value = value.replace('#include "xllm_time.h"', '#include "../xllm-time.h"')
        value = value.replace('#include <xhttp-client.h>', '#include "../xwork-xrt.h"')
        destination = "xllm-time.h" if path.name == "xllm_time.h" else "src/" + path.name
        if destination in files:
            raise ValueError(f"duplicate flat SDK path: {name}/{destination}")
        files[destination] = value
    if name == "xllm":
        value = text(root / "include/xllm/executor.h")
        files["xllm-executor.h"] = value.replace("#include <xllm/api.h>", '#include "xllm.h"')
    files["UPSTREAM.txt"] = (
        "UPSTREAM\n\n"
        f"来源: xrt extlibs/{name}\n基线: xrt@{revision}\n许可: MIT\n\n"
        "Exported by tools/export_agent_libraries.py. Implementations retain the\n"
        "canonical source bodies with mechanically rewritten local includes.\n"
        "Public declarations use the flat XS/TCC include layout without DLL\n"
        "annotations. Native XRT and HTTP bridges are supplied by the host.\n"
        "Use the exporter --check and the product production-tree hashes to\n"
        "verify the exact installed sources; this is not a byte-identical copy\n"
        "of the modular source layout.\n")
    return files


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--xserver", type=Path, required=True)
    parser.add_argument("--revision", help="source revision; defaults to this checkout's HEAD")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    revision = args.revision or subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    if not re.fullmatch("[0-9a-f]{40}", revision):
        parser.error("revision must be a full Git object ID")
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    dirty = subprocess.check_output(["git", "status", "--porcelain", "--", "tools/export_agent_libraries.py",
                                    *("extlibs/" + name for name in LIBRARIES)],
                                   cwd=ROOT, text=True)
    if head != revision or dirty:
        parser.error("export requires a clean, committed source tree at the selected revision")
    destination = args.xserver.resolve()
    if not (destination / "lib/xrt.h").is_file():
        parser.error("destination must be an XS source checkout")
    failures = []
    for name in LIBRARIES:
        exported = export_files(name, revision)
        source_root = destination / "lib" / name / "src"
        if source_root.is_dir():
            for stale in source_root.rglob("*"):
                if not stale.is_file() or stale.suffix not in {".c", ".h"}:
                    continue
                relative = stale.relative_to(destination / "lib" / name).as_posix()
                if relative not in exported:
                    if args.check:
                        failures.append(str(stale) + " (obsolete generated source)")
                    else:
                        if not stale.resolve().is_relative_to(source_root.resolve()):
                            raise ValueError("generated source escaped the destination library")
                        stale.unlink()
        for relative, value in exported.items():
            path = destination / "lib" / name / relative
            if args.check:
                if not path.is_file() or text(path) != value:
                    failures.append(str(path))
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(value, encoding="utf-8", newline="\n")
        print(f"{name}: {len(exported)} generated SDK files")
    if failures:
        raise SystemExit("SDK export differs:\n" + "\n".join(failures))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
