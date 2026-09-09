#!/usr/bin/env python3
"""Build a tiny text-embedding ONNX model for the local demo."""

from __future__ import annotations

import argparse
from collections import Counter
from pathlib import Path

import numpy as np
import onnx
from onnx import TensorProto, helper, numpy_helper


def is_cjk(ch: str) -> bool:
    cp = ord(ch)
    return (
        0x3400 <= cp <= 0x4DBF
        or 0x4E00 <= cp <= 0x9FFF
        or 0xF900 <= cp <= 0xFAFF
        or 0x20000 <= cp <= 0x2A6DF
    )


def tokenize(text: str) -> list[str]:
    segments = [segment.lower() for segment in text.split() if segment.strip()]
    if len(segments) >= 2:
        return segments

    tokens: list[str] = []
    ascii_buf: list[str] = []
    prev_cjk: str | None = None

    def flush_ascii() -> None:
        nonlocal ascii_buf
        if ascii_buf:
            tokens.append("".join(ascii_buf))
            ascii_buf = []

    for ch in text.lower():
        if ch.isascii() and (ch.isalnum() or ch in "_-"):
            prev_cjk = None
            ascii_buf.append(ch)
            continue

        flush_ascii()
        if is_cjk(ch):
            tokens.append(ch)
            if prev_cjk is not None:
                tokens.append(prev_cjk + ch)
            prev_cjk = ch
        else:
            prev_cjk = None

    flush_ascii()
    return tokens


def build_cooccurrence(sequences: list[list[str]], vocab_index: dict[str, int], window: int = 2) -> np.ndarray:
    size = len(vocab_index)
    matrix = np.zeros((size, size), dtype=np.float32)

    for seq in sequences:
        for i, token in enumerate(seq):
            left = max(0, i - window)
            right = min(len(seq), i + window + 1)
            src = vocab_index[token]
            for j in range(left, right):
                if i == j:
                    continue
                dst = vocab_index[seq[j]]
                matrix[src, dst] += 1.0 / float(abs(i - j))

    return matrix


def ppmi_embeddings(cooc: np.ndarray, embed_dim: int) -> np.ndarray:
    total = float(cooc.sum())
    if total <= 0.0:
        raise RuntimeError("co-occurrence matrix is empty")

    row = cooc.sum(axis=1, keepdims=True)
    col = cooc.sum(axis=0, keepdims=True)
    expected = row @ col
    ppmi = np.log((cooc * total + 1e-8) / (expected + 1e-8))
    ppmi = np.maximum(ppmi, 0.0)

    u, s, _ = np.linalg.svd(ppmi, full_matrices=False)
    rank = max(1, int(np.sum(s > 1e-8)))
    actual_dim = min(embed_dim, rank, ppmi.shape[0])
    embeddings = u[:, :actual_dim] * np.sqrt(s[:actual_dim])

    if actual_dim < embed_dim:
        padded = np.zeros((ppmi.shape[0], embed_dim), dtype=np.float32)
        padded[:, :actual_dim] = embeddings.astype(np.float32)
        embeddings = padded
    else:
        embeddings = embeddings.astype(np.float32)

    norms = np.linalg.norm(embeddings, axis=1, keepdims=True)
    embeddings /= np.clip(norms, 1e-6, None)
    return embeddings.astype(np.float32)


def save_model(weights: np.ndarray, output_dir: Path) -> None:
    vocab_size, embed_dim = weights.shape
    input_info = helper.make_tensor_value_info("bow", TensorProto.FLOAT, [1, vocab_size])
    output_info = helper.make_tensor_value_info("embedding", TensorProto.FLOAT, [1, embed_dim])
    weight_tensor = numpy_helper.from_array(weights, name="token_embeddings")
    matmul = helper.make_node("MatMul", inputs=["bow", "token_embeddings"], outputs=["embedding"])
    graph = helper.make_graph(
        nodes=[matmul],
        name="ToyTextEmbedder",
        inputs=[input_info],
        outputs=[output_info],
        initializer=[weight_tensor],
    )
    model = helper.make_model(
        graph,
        opset_imports=[helper.make_operatorsetid("", 17)],
        producer_name="xllm-onnxdemo",
    )
    model.ir_version = 10
    onnx.checker.check_model(model)
    onnx.save_model(model, output_dir / "toy_text_embedder.onnx")


def write_metadata(output_dir: Path, vocab: list[str], weights: np.ndarray) -> None:
    (output_dir / "vocab.txt").write_text("\n".join(vocab) + "\n", encoding="utf-8")
    (output_dir / "model_info.txt").write_text(
        "\n".join(
            [
                "name=toy_text_embedder",
                f"vocab_size={len(vocab)}",
                f"embedding_dim={weights.shape[1]}",
                "opset=17",
                "core_ops=MatMul",
            ]
        )
        + "\n",
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--corpus", required=True, help="Training corpus text file.")
    parser.add_argument("--output-dir", required=True, help="Directory for ONNX model and assets.")
    parser.add_argument("--embed-dim", type=int, default=16, help="Embedding vector width.")
    args = parser.parse_args()

    corpus_path = Path(args.corpus)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    lines = [line.strip() for line in corpus_path.read_text(encoding="utf-8").splitlines() if line.strip()]
    sequences = [tokenize(line) for line in lines]
    token_counts = Counter(token for seq in sequences for token in seq)
    vocab = [token for token, _ in sorted(token_counts.items(), key=lambda item: (-item[1], item[0]))]

    if len(vocab) < 2:
        raise RuntimeError("corpus is too small for the demo")

    vocab_index = {token: idx for idx, token in enumerate(vocab)}
    cooc = build_cooccurrence(sequences, vocab_index)
    weights = ppmi_embeddings(cooc, args.embed_dim)
    save_model(weights, output_dir)
    write_metadata(output_dir, vocab, weights)

    print(f"wrote model to {output_dir / 'toy_text_embedder.onnx'}")
    print(f"vocab size: {len(vocab)}")
    print(f"embedding dim: {weights.shape[1]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
