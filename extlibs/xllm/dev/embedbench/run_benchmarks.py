#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import textwrap
import urllib.request
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DEV_DIR = ROOT / "dev"
EMBEDBENCH_DIR = DEV_DIR / "embedbench"
BUILD_DIR = EMBEDBENCH_DIR / "build"
SRC_DIR = EMBEDBENCH_DIR / "src"
FASTTEXT_DIR = DEV_DIR / "fastText"
WORD2VEC_DIR = DEV_DIR / "word2vec"
MINIONNX_DIR = DEV_DIR / "minionnx"
ONNXDEMO_DIR = DEV_DIR / "onnxdemo"
CASES_PATH = EMBEDBENCH_DIR / "benchmark_cases.tsv"
CORPUS_PATH = EMBEDBENCH_DIR / "benchmark_corpus.txt"
RUNTIME_VERSION = "1.24.4"
RUNTIME_BASENAME = f"onnxruntime-win-x64-{RUNTIME_VERSION}"
RUNTIME_URL = f"https://github.com/microsoft/onnxruntime/releases/download/v{RUNTIME_VERSION}/{RUNTIME_BASENAME}.zip"


def run(cmd: list[str], cwd: Path | None = None, capture: bool = False) -> subprocess.CompletedProcess[str]:
    print("+", " ".join(f'"{part}"' if " " in part else part for part in cmd))
    return subprocess.run(
        cmd,
        cwd=str(cwd) if cwd else None,
        check=True,
        text=True,
        capture_output=capture,
    )


def parse_key_value_output(stdout: str) -> dict[str, str]:
    result: dict[str, str] = {}
    for raw_line in stdout.splitlines():
        line = raw_line.strip()
        if not line or "=" not in line:
            continue
        key, value = line.split("=", 1)
        result[key] = value
    return result


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def ensure_onnx_runtime() -> Path:
    existing_root = ONNXDEMO_DIR / "build" / RUNTIME_BASENAME
    existing_dll = existing_root / "lib" / "onnxruntime.dll"
    if existing_dll.exists():
        return existing_root

    downloads_dir = BUILD_DIR / "downloads"
    ensure_dir(downloads_dir)
    zip_path = downloads_dir / f"{RUNTIME_BASENAME}.zip"
    runtime_root = BUILD_DIR / RUNTIME_BASENAME
    runtime_dll = runtime_root / "lib" / "onnxruntime.dll"
    if runtime_dll.exists():
        return runtime_root

    print(f"Downloading {RUNTIME_URL}")
    urllib.request.urlretrieve(RUNTIME_URL, zip_path)
    with zipfile.ZipFile(zip_path, "r") as archive:
        archive.extractall(BUILD_DIR)
    return runtime_root


def ensure_onnx_assets() -> dict[str, Path]:
    model_dir = BUILD_DIR / "onnx_model"
    ensure_dir(model_dir)
    run(
        [
            "powershell",
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str(MINIONNX_DIR / "prepare_demo_minimal_assets.ps1"),
            "-CorpusPath",
            str(CORPUS_PATH),
            "-ModelDir",
            str(model_dir),
        ],
        cwd=EMBEDBENCH_DIR,
    )

    ort_model_path = model_dir / "ort" / "toy_text_embedder.ort"
    model_path = ort_model_path if ort_model_path.exists() else model_dir / "toy_text_embedder.onnx"
    return {
        "model_dir": model_dir,
        "model_path": model_path,
        "vocab_path": model_dir / "vocab.txt",
    }


def compile_binaries(runtime_root: Path) -> dict[str, Path]:
    ensure_dir(BUILD_DIR)

    fasttext_output = BUILD_DIR / "bench_fasttext.exe"
    fasttext_sources = [
        str(path)
        for path in sorted((FASTTEXT_DIR / "vendor" / "fastText" / "src").glob("*.cc"))
        if path.name.lower() != "main.cc"
    ]
    run(
        [
            "g++",
            "-std=c++17",
            "-O2",
            "-Wall",
            "-Wextra",
            str(SRC_DIR / "bench_fasttext.cpp"),
            str(FASTTEXT_DIR / "src" / "fasttext_demo.cpp"),
            *fasttext_sources,
            "-o",
            str(fasttext_output),
        ],
        cwd=ROOT,
    )

    word2vec_trainer = BUILD_DIR / "word2vec_train.exe"
    run(
        [
            "gcc",
            "-std=c11",
            "-O2",
            "-Wall",
            "-Wextra",
            str(WORD2VEC_DIR / "vendor" / "word2vec" / "word2vec.c"),
            "-o",
            str(word2vec_trainer),
            "-lm",
            "-pthread",
        ],
        cwd=ROOT,
    )

    word2vec_output = BUILD_DIR / "bench_word2vec.exe"
    run(
        [
            "gcc",
            "-std=c11",
            "-O2",
            "-Wall",
            "-Wextra",
            str(SRC_DIR / "bench_word2vec.c"),
            str(WORD2VEC_DIR / "src" / "word2vec_demo.c"),
            "-o",
            str(word2vec_output),
            "-lm",
        ],
        cwd=ROOT,
    )

    onnx_output = BUILD_DIR / "bench_onnx.exe"
    run(
        [
            "gcc",
            "-std=c11",
            "-O2",
            "-Wall",
            "-Wextra",
            f"-I{runtime_root / 'include'}",
            str(SRC_DIR / "bench_onnx.c"),
            str(ONNXDEMO_DIR / "src" / "onnx_demo.c"),
            "-o",
            str(onnx_output),
        ],
        cwd=ROOT,
    )

    return {
        "fasttext": fasttext_output,
        "word2vec": word2vec_output,
        "word2vec_trainer": word2vec_trainer,
        "onnx": onnx_output,
    }


def run_backend(cmd: list[str]) -> dict[str, str]:
    completed = run(cmd, cwd=EMBEDBENCH_DIR, capture=True)
    if completed.stderr:
        stderr = completed.stderr.strip()
        if stderr:
            print(stderr)
    return parse_key_value_output(completed.stdout)


def file_size(path: str | Path | None) -> int:
    if not path:
        return 0
    p = Path(path)
    return p.stat().st_size if p.exists() else 0


def to_number(value: str) -> int | float | str:
    try:
        if any(marker in value for marker in (".", "e", "E")):
            return float(value)
        return int(value)
    except ValueError:
        return value


def summarize_backend(raw: dict[str, str], support_path: Path | None = None) -> dict[str, object]:
    result: dict[str, object] = {key: to_number(value) for key, value in raw.items() if not key.startswith("case.")}
    case_scores = {key[5:]: float(value) for key, value in raw.items() if key.startswith("case.")}
    result["case_scores"] = case_scores
    model_bytes = file_size(raw.get("model_path"))
    runtime_bytes = file_size(raw.get("runtime_path"))
    support_bytes = file_size(support_path)
    result["model_bytes"] = model_bytes
    result["runtime_bytes"] = runtime_bytes
    result["support_bytes"] = support_bytes
    result["package_bytes"] = model_bytes + runtime_bytes + support_bytes
    result["gap"] = float(result.get("related_avg", 0.0)) - float(result.get("unrelated_avg", 0.0))
    return result


def write_results(results: dict[str, dict[str, object]]) -> None:
    results_path = BUILD_DIR / "results.json"
    markdown_path = BUILD_DIR / "results.md"

    results_path.write_text(json.dumps(results, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    header = "| Backend | Dim | Vocab | Model KB | Runtime KB | Support KB | Package KB | Init ms | Embed ms/text | Compare ms/pair | Related Avg | Unrelated Avg | Gap |\n"
    header += "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n"
    rows = []
    for backend in ("fasttext", "word2vec", "onnx"):
        item = results[backend]
        rows.append(
            "| {backend} | {dim} | {vocab} | {model:.1f} | {runtime:.1f} | {support:.1f} | {package:.1f} | {init:.3f} | {embed:.6f} | {compare:.6f} | {related:.6f} | {unrelated:.6f} | {gap:.6f} |".format(
                backend=backend,
                dim=item.get("vector_dim", 0),
                vocab=item.get("vocab_size", -1),
                model=float(item["model_bytes"]) / 1024.0,
                runtime=float(item["runtime_bytes"]) / 1024.0,
                support=float(item["support_bytes"]) / 1024.0,
                package=float(item["package_bytes"]) / 1024.0,
                init=float(item.get("init_ms", 0.0)),
                embed=float(item.get("embed_avg_ms", 0.0)),
                compare=float(item.get("compare_avg_ms", 0.0)),
                related=float(item.get("related_avg", 0.0)),
                unrelated=float(item.get("unrelated_avg", 0.0)),
                gap=float(item.get("gap", 0.0)),
            )
        )

    case_lines = ["", "## Case Scores", ""]
    case_header = "| Backend | " + " | ".join(results["fasttext"]["case_scores"].keys()) + " |\n"
    case_header += "|" + "---|" * (len(results["fasttext"]["case_scores"]) + 1) + "\n"
    case_lines.append(case_header.rstrip())
    for backend in ("fasttext", "word2vec", "onnx"):
        item = results[backend]["case_scores"]
        values = " | ".join(f"{float(score):.6f}" for score in item.values())
        case_lines.append(f"| {backend} | {values} |")

    markdown = "# Embedding Benchmark\n\n"
    markdown += f"- Corpus: `{CORPUS_PATH}`\n"
    markdown += f"- Cases: `{CASES_PATH}`\n"
    markdown += f"- Generated: `{BUILD_DIR}`\n\n"
    markdown += header + "\n".join(rows) + "\n"
    markdown += "\n".join(case_lines) + "\n"
    markdown_path.write_text(markdown, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Run same-caliber benchmark for fastText, word2vec, and ONNX demos.")
    parser.add_argument("--iterations", type=int, default=500)
    parser.add_argument("--warmup", type=int, default=50)
    args = parser.parse_args()

    ensure_dir(BUILD_DIR)
    runtime_root = ensure_onnx_runtime()
    onnx_assets = ensure_onnx_assets()
    binaries = compile_binaries(runtime_root)

    fasttext_prefix = BUILD_DIR / "fasttext_bench_model"
    word2vec_model = BUILD_DIR / "word2vec_bench_model.bin"

    raw_fasttext = run_backend(
        [
            str(binaries["fasttext"]),
            "--corpus",
            str(CORPUS_PATH),
            "--cases",
            str(CASES_PATH),
            "--model-prefix",
            str(fasttext_prefix),
            "--warmup",
            str(args.warmup),
            "--iterations",
            str(args.iterations),
        ]
    )
    raw_word2vec = run_backend(
        [
            str(binaries["word2vec"]),
            "--corpus",
            str(CORPUS_PATH),
            "--cases",
            str(CASES_PATH),
            "--trainer",
            str(binaries["word2vec_trainer"]),
            "--model",
            str(word2vec_model),
            "--warmup",
            str(args.warmup),
            "--iterations",
            str(args.iterations),
        ]
    )
    raw_onnx = run_backend(
        [
            str(binaries["onnx"]),
            "--cases",
            str(CASES_PATH),
            "--runtime-dll",
            str(runtime_root / "lib" / "onnxruntime.dll"),
            "--model",
            str(onnx_assets["model_path"]),
            "--vocab",
            str(onnx_assets["vocab_path"]),
            "--warmup",
            str(args.warmup),
            "--iterations",
            str(args.iterations),
        ]
    )

    results = {
        "fasttext": summarize_backend(raw_fasttext),
        "word2vec": summarize_backend(raw_word2vec),
        "onnx": summarize_backend(raw_onnx, Path(onnx_assets["vocab_path"])),
    }

    write_results(results)

    print("")
    print("Benchmark results written to:")
    print(f"  {BUILD_DIR / 'results.json'}")
    print(f"  {BUILD_DIR / 'results.md'}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as exc:
        sys.stderr.write(f"command failed with exit code {exc.returncode}\n")
        if exc.stdout:
            sys.stderr.write(exc.stdout)
        if exc.stderr:
            sys.stderr.write(exc.stderr)
        raise
