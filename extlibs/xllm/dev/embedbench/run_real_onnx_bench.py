#!/usr/bin/env python
from __future__ import annotations

import argparse
import json
import math
import os
import random
import time
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import numpy as np
import onnxruntime as ort
import psutil
from datasets import load_dataset
from huggingface_hub import snapshot_download
from onnxruntime_extensions import get_library_path as get_extensions_library_path
from scipy.stats import pearsonr, spearmanr
from transformers import AutoTokenizer


ROOT_DIR = Path(__file__).resolve().parent
BUILD_DIR = ROOT_DIR / "build" / "real_onnx"
HF_HOME = BUILD_DIR / "hf_home"
MODEL_CACHE_DIR = BUILD_DIR / "model_cache"
RESULTS_JSON = BUILD_DIR / "results_real_onnx.json"
RESULTS_MD = BUILD_DIR / "results_real_onnx.md"
MODEL_CONFIG_PATH = ROOT_DIR / "real_models.json"
DATASET_CONFIG_PATH = ROOT_DIR / "real_datasets.json"
RUNTIME_SAMPLE_TEXTS = [
    "如何在 C 语言里做 UTF-8 文本切分？",
    "How do I preserve source offsets in a retrieval chunker?",
    "项目记忆需要抽取 durable facts，而不是直接向量化所有对话。",
    "Use ONNX Runtime on CPU for a small sentence embedding model.",
]


def ensure_local_hf_cache() -> None:
    os.environ.setdefault("HF_HOME", str(HF_HOME))
    os.environ.setdefault("HF_HUB_CACHE", str(HF_HOME / "hub"))
    os.environ.setdefault("HF_DATASETS_CACHE", str(HF_HOME / "datasets"))


def load_json_file(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def sanitize_run_name(run_name: str) -> str:
    allowed = []
    for char in run_name:
        if char.isalnum() or char in ("-", "_"):
            allowed.append(char)
        else:
            allowed.append("_")
    return "".join(allowed).strip("_")


def slugify_repo_id(repo_id: str) -> str:
    return repo_id.replace("/", "__")


def sum_file_sizes(root: Path, skip_hidden_cache: bool = True) -> int:
    total = 0
    for file_path in root.rglob("*"):
        if not file_path.is_file():
            continue
        relative = file_path.relative_to(root)
        if skip_hidden_cache and relative.parts and relative.parts[0] == ".cache":
            continue
        total += file_path.stat().st_size
    return total


def tokenizer_asset_size(root: Path) -> int:
    total = 0
    for name in (
        "tokenizer.json",
        "tokenizer_config.json",
        "special_tokens_map.json",
        "vocab.txt",
        "sentencepiece.bpe.model",
        "unigram.json",
        "merges.txt",
        "spiece.model",
    ):
        path = root / name
        if path.exists():
            total += path.stat().st_size
    return total


def onnxruntime_dll_size() -> int:
    ort_root = Path(ort.__file__).resolve().parent
    capi_dir = ort_root / "capi"
    total = 0
    if capi_dir.exists():
        for file_path in capi_dir.glob("*.dll"):
            total += file_path.stat().st_size
    return total


def onnxruntime_extensions_library_size() -> int:
    try:
        return Path(get_extensions_library_path()).stat().st_size
    except OSError:
        return 0


def stable_sample(items: list[str], limit: int, seed: int) -> list[str]:
    if len(items) <= limit:
        return list(items)
    rng = random.Random(seed)
    return rng.sample(items, limit)


def join_fields(row: dict[str, Any], fields: list[str]) -> str:
    values = []
    for field in fields:
        raw_value = row.get(field)
        if raw_value is None:
            continue
        value = str(raw_value).strip()
        if value:
            values.append(value)
    return "\n".join(values)


def cosine_similarity_matrix(left: np.ndarray, right: np.ndarray) -> np.ndarray:
    return left @ right.T


def compute_rank_metrics(
    ranked_doc_ids: list[str],
    positive_doc_ids: set[str],
    top_k: int,
) -> tuple[float, float, float]:
    top_ids = ranked_doc_ids[:top_k]
    hits = 0
    dcg = 0.0
    first_hit_rank = 0
    for rank, doc_id in enumerate(top_ids, start=1):
        if doc_id in positive_doc_ids:
            hits += 1
            dcg += 1.0 / math.log2(rank + 1.0)
            if first_hit_rank == 0:
                first_hit_rank = rank
    recall = hits / max(len(positive_doc_ids), 1)
    mrr = 1.0 / first_hit_rank if first_hit_rank else 0.0
    ideal_hits = min(len(positive_doc_ids), top_k)
    idcg = sum(1.0 / math.log2(rank + 1.0) for rank in range(1, ideal_hits + 1))
    ndcg = dcg / idcg if idcg else 0.0
    return recall, mrr, ndcg


def transform_score(raw_score: Any, transform: dict[str, Any] | None) -> float:
    if not transform:
        return float(raw_score)
    transform_type = transform.get("type")
    if transform_type == "map":
        mapping = transform.get("mapping", {})
        return float(mapping[str(raw_score)])
    raise ValueError(f"unsupported score transform: {transform_type}")


@dataclass
class RetrievalDataset:
    query_ids: list[str]
    query_texts: list[str]
    corpus_ids: list[str]
    corpus_texts: list[str]
    positives_by_query_id: dict[str, set[str]]


class OnnxEmbedder:
    def __init__(self, spec: dict[str, Any], threads: int) -> None:
        self.spec = spec
        self.repo_id = spec["repo_id"]
        self.model_file_name = spec["model_file"]
        self.input_mode = spec.get("input_mode", "tokenized")
        self.max_length = int(spec.get("max_length", 0))
        self.max_input_chars = int(spec.get("max_input_chars", 0))
        self.pooling = spec["pooling"]
        self.normalize = bool(spec.get("normalize", True))
        self.requires_extensions = bool(spec.get("requires_extensions", False))
        self.output_name = spec.get("output_name", "")
        self.task_prefixes = spec.get("task_prefixes", {})
        self.model_dir = MODEL_CACHE_DIR / slugify_repo_id(self.repo_id)
        self.model_dir.parent.mkdir(parents=True, exist_ok=True)
        start = time.perf_counter()
        snapshot_download(self.repo_id, local_dir=str(self.model_dir))
        self.download_and_prepare_ms = (time.perf_counter() - start) * 1000.0

        tokenizer_start = time.perf_counter()
        self.tokenizer = None
        session_options = ort.SessionOptions()
        session_options.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
        if threads > 0:
            session_options.intra_op_num_threads = threads
            session_options.inter_op_num_threads = 1
        if self.requires_extensions:
            session_options.register_custom_ops_library(get_extensions_library_path())
        if self.input_mode == "tokenized":
            self.tokenizer = AutoTokenizer.from_pretrained(
                str(self.model_dir),
                trust_remote_code=False,
                use_fast=True,
            )
        self.session = ort.InferenceSession(
            str(self.model_dir / self.model_file_name),
            sess_options=session_options,
            providers=["CPUExecutionProvider"],
        )
        self.prepare_session_ms = (time.perf_counter() - tokenizer_start) * 1000.0
        self.input_names = [item.name for item in self.session.get_inputs()]
        self.output_names = [item.name for item in self.session.get_outputs()]

    def prefixed_texts(self, texts: list[str], prefix_key: str) -> list[str]:
        prefix = self.task_prefixes.get(prefix_key, "")
        prepared = [prefix + text if prefix else text for text in texts]
        if self.max_input_chars > 0:
            prepared = [text[: self.max_input_chars] for text in prepared]
        return prepared

    def _normalize_vectors(self, vectors: np.ndarray) -> np.ndarray:
        if self.normalize:
            norms = np.linalg.norm(vectors, axis=1, keepdims=True)
            vectors = vectors / np.clip(norms, 1.0e-12, None)
        return vectors.astype(np.float32)

    def _pool_tokenized(self, output: np.ndarray, attention_mask: np.ndarray | None) -> np.ndarray:
        output = np.asarray(output, dtype=np.float32)
        if output.ndim == 2:
            vectors = output
        elif output.ndim == 3:
            if self.pooling == "cls":
                vectors = output[:, 0, :]
            elif self.pooling == "mean":
                if attention_mask is None:
                    raise ValueError("mean pooling requires attention_mask")
                mask = np.asarray(attention_mask, dtype=np.float32)[:, :, None]
                numerators = (output * mask).sum(axis=1)
                denominators = np.clip(mask.sum(axis=1), 1.0e-12, None)
                vectors = numerators / denominators
            else:
                raise ValueError(f"unsupported pooling: {self.pooling}")
        else:
            raise ValueError(f"unexpected model output rank: {output.ndim}")
        return self._normalize_vectors(vectors)

    def _select_output_tensor(self, outputs: list[np.ndarray]) -> np.ndarray:
        output_map = {name: value for name, value in zip(self.output_names, outputs)}
        if self.output_name:
            return output_map[self.output_name]
        if len(outputs) == 1:
            return outputs[0]
        for value in outputs:
            if np.asarray(value).ndim == 2:
                return value
        return outputs[0]

    def _encode_tokenized_batch(self, batch: list[str]) -> np.ndarray:
        if self.tokenizer is None:
            raise RuntimeError("tokenizer backend is not initialized")
        encoded = self.tokenizer(
            batch,
            padding=True,
            truncation=True,
            max_length=self.max_length,
            return_tensors="np",
        )
        feed = {}
        for input_name in self.input_names:
            if input_name in encoded:
                feed[input_name] = encoded[input_name]
            elif input_name == "token_type_ids":
                feed[input_name] = np.zeros_like(encoded["input_ids"], dtype=np.int64)
            else:
                raise KeyError(f"tokenizer did not produce required input: {input_name}")

        outputs = self.session.run(None, feed)
        attention_mask = feed.get("attention_mask")
        return self._pool_tokenized(self._select_output_tensor(outputs), attention_mask)

    def _pool_merged_single(self, outputs: list[np.ndarray]) -> np.ndarray:
        output_map = {name: value for name, value in zip(self.output_names, outputs)}
        hidden_states = np.asarray(output_map[self.output_names[-1]], dtype=np.float32)
        if hidden_states.ndim == 2:
            return self._normalize_vectors(hidden_states)
        if hidden_states.ndim != 3 or hidden_states.shape[0] != 1:
            raise ValueError(f"unexpected merged model output shape: {hidden_states.shape}")

        token_vectors = hidden_states[0]
        offsets = np.asarray(output_map.get("offset_mapping"), dtype=np.int64)
        if offsets.ndim == 2 and offsets.shape[0] == token_vectors.shape[0]:
            content_mask = np.any(offsets != 0, axis=1)
            if np.any(content_mask):
                token_vectors = token_vectors[content_mask]
        vector = token_vectors.mean(axis=0, keepdims=True)
        return self._normalize_vectors(vector)

    def _encode_merged_batch(self, batch: list[str]) -> np.ndarray:
        vectors = []
        for text in batch:
            outputs = self.session.run(None, {"text": np.asarray([text])})
            vectors.append(self._pool_merged_single(outputs))
        return np.vstack(vectors)

    def encode(self, texts: list[str], prefix_key: str, batch_size: int) -> np.ndarray:
        if not texts:
            return np.zeros((0, 0), dtype=np.float32)

        prepared = self.prefixed_texts(texts, prefix_key)
        batches = []
        for index in range(0, len(prepared), batch_size):
            batch = prepared[index : index + batch_size]
            if self.input_mode == "tokenized":
                batches.append(self._encode_tokenized_batch(batch))
            elif self.input_mode == "merged_text":
                batches.append(self._encode_merged_batch(batch))
            else:
                raise ValueError(f"unsupported input mode: {self.input_mode}")

        return np.vstack(batches)


def load_sts_examples(spec: dict[str, Any], limit: int, seed: int) -> list[dict[str, Any]]:
    dataset = load_dataset(
        spec["repo_id"],
        spec.get("config"),
        split=spec["split"],
    )
    indices = stable_sample(list(range(len(dataset))), min(limit, len(dataset)), seed)
    examples = []
    for index in indices:
        row = dataset[int(index)]
        examples.append(
            {
                "text_a": str(row[spec["text_a_field"]]),
                "text_b": str(row[spec["text_b_field"]]),
                "score": transform_score(row[spec["score_field"]], spec.get("score_transform")),
            }
        )
    return examples


def load_retrieval_examples(spec: dict[str, Any], query_limit: int, corpus_limit: int, seed: int) -> RetrievalDataset:
    queries = load_dataset(
        spec["repo_id"],
        spec["queries_config"],
        split=spec["queries_split"],
    )
    corpus = load_dataset(
        spec["repo_id"],
        spec["corpus_config"],
        split=spec["corpus_split"],
    )
    qrels = load_dataset(
        spec["repo_id"],
        spec.get("qrels_config"),
        split=spec["qrels_split"],
    )

    query_text_by_id = {}
    for row in queries:
        query_text_by_id[str(row[spec["query_id_field"]])] = join_fields(row, spec["query_text_fields"])

    positives_by_query_id: dict[str, set[str]] = defaultdict(set)
    for row in qrels:
        score = float(row[spec["qrels_score_field"]])
        if score <= 0:
            continue
        query_id = str(row[spec["qrels_query_id_field"]])
        corpus_id = str(row[spec["qrels_corpus_id_field"]])
        positives_by_query_id[query_id].add(corpus_id)

    eligible_query_ids = [
        query_id
        for query_id, positive_ids in positives_by_query_id.items()
        if positive_ids and query_id in query_text_by_id
    ]
    selected_query_ids = stable_sample(eligible_query_ids, min(query_limit, len(eligible_query_ids)), seed)

    positive_doc_ids = set()
    selected_positives = {}
    for query_id in selected_query_ids:
        selected_positives[query_id] = set(positives_by_query_id[query_id])
        positive_doc_ids.update(selected_positives[query_id])

    corpus_id_to_text = {}
    negative_candidates = []
    for row in corpus:
        corpus_id = str(row[spec["corpus_id_field"]])
        corpus_text = join_fields(row, spec["corpus_text_fields"])
        corpus_id_to_text[corpus_id] = corpus_text
        if corpus_id not in positive_doc_ids:
            negative_candidates.append(corpus_id)

    remaining_capacity = max(corpus_limit - len(positive_doc_ids), 0)
    sampled_negatives = stable_sample(negative_candidates, min(remaining_capacity, len(negative_candidates)), seed + 17)
    selected_corpus_ids = list(sorted(positive_doc_ids)) + list(sampled_negatives)

    return RetrievalDataset(
        query_ids=selected_query_ids,
        query_texts=[query_text_by_id[query_id] for query_id in selected_query_ids],
        corpus_ids=selected_corpus_ids,
        corpus_texts=[corpus_id_to_text[corpus_id] for corpus_id in selected_corpus_ids],
        positives_by_query_id=selected_positives,
    )


def evaluate_sts(
    embedder: OnnxEmbedder,
    dataset_spec: dict[str, Any],
    limit: int,
    seed: int,
    batch_size: int,
) -> dict[str, Any]:
    examples = load_sts_examples(dataset_spec, limit, seed)
    texts_a = [item["text_a"] for item in examples]
    texts_b = [item["text_b"] for item in examples]
    gold_scores = np.asarray([item["score"] for item in examples], dtype=np.float32)

    vectors_a = embedder.encode(texts_a, "sts_a", batch_size)
    vectors_b = embedder.encode(texts_b, "sts_b", batch_size)
    predicted_scores = np.sum(vectors_a * vectors_b, axis=1)

    if np.unique(gold_scores).size < 2 or np.unique(predicted_scores).size < 2:
        spearman = 0.0
        pearson = 0.0
    else:
        spearman = spearmanr(gold_scores, predicted_scores).correlation
        pearson = pearsonr(gold_scores, predicted_scores).statistic

    return {
        "task_type": "sts",
        "examples": len(examples),
        "spearman": float(spearman) if spearman == spearman else 0.0,
        "pearson": float(pearson) if pearson == pearson else 0.0,
        "score_mean": float(np.mean(predicted_scores)),
        "score_std": float(np.std(predicted_scores)),
    }


def evaluate_retrieval(
    embedder: OnnxEmbedder,
    dataset_spec: dict[str, Any],
    query_limit: int,
    corpus_limit: int,
    seed: int,
    batch_size: int,
    top_k: int,
) -> dict[str, Any]:
    dataset = load_retrieval_examples(dataset_spec, query_limit, corpus_limit, seed)
    query_vectors = embedder.encode(dataset.query_texts, "retrieval_query", batch_size)
    corpus_vectors = embedder.encode(dataset.corpus_texts, "retrieval_document", batch_size)

    scores = cosine_similarity_matrix(query_vectors, corpus_vectors)
    corpus_ids = np.asarray(dataset.corpus_ids)
    recall_scores = []
    mrr_scores = []
    ndcg_scores = []

    for query_index, query_id in enumerate(dataset.query_ids):
        ranked_indices = np.argsort(scores[query_index])[::-1]
        ranked_doc_ids = corpus_ids[ranked_indices].tolist()
        recall, mrr, ndcg = compute_rank_metrics(
            ranked_doc_ids,
            dataset.positives_by_query_id[query_id],
            top_k,
        )
        recall_scores.append(recall)
        mrr_scores.append(mrr)
        ndcg_scores.append(ndcg)

    return {
        "task_type": "retrieval",
        "queries": len(dataset.query_ids),
        "corpus": len(dataset.corpus_ids),
        "recall_at_k": float(np.mean(recall_scores)),
        "mrr_at_k": float(np.mean(mrr_scores)),
        "ndcg_at_k": float(np.mean(ndcg_scores)),
        "top_k": top_k,
    }


def measure_runtime(embedder: OnnxEmbedder, batch_size: int) -> dict[str, Any]:
    process = psutil.Process()
    warmup_texts = RUNTIME_SAMPLE_TEXTS * 2
    embedder.encode(warmup_texts, "retrieval_query", batch_size)

    start_single = time.perf_counter()
    for text in RUNTIME_SAMPLE_TEXTS:
        embedder.encode([text], "retrieval_query", 1)
    single_ms = ((time.perf_counter() - start_single) * 1000.0) / len(RUNTIME_SAMPLE_TEXTS)

    batch_texts = (RUNTIME_SAMPLE_TEXTS * max(batch_size, 8))[:batch_size]
    start_batch = time.perf_counter()
    embedder.encode(batch_texts, "retrieval_query", batch_size)
    batch_ms = (time.perf_counter() - start_batch) * 1000.0

    model_dir_size = sum_file_sizes(embedder.model_dir)
    model_file_size = (embedder.model_dir / embedder.model_file_name).stat().st_size
    tokenizer_size = tokenizer_asset_size(embedder.model_dir)
    runtime_dll_size = onnxruntime_dll_size()
    extensions_library_size = onnxruntime_extensions_library_size() if embedder.requires_extensions else 0

    return {
        "download_and_prepare_ms": round(embedder.download_and_prepare_ms, 3),
        "session_init_ms": round(embedder.prepare_session_ms, 3),
        "single_encode_ms": round(single_ms, 6),
        "batch_encode_ms": round(batch_ms, 6),
        "batch_texts_per_second": round((len(batch_texts) * 1000.0) / batch_ms, 3) if batch_ms > 0 else 0.0,
        "rss_mb": round(process.memory_info().rss / (1024.0 * 1024.0), 3),
        "model_dir_kb": round(model_dir_size / 1024.0, 3),
        "model_file_kb": round(model_file_size / 1024.0, 3),
        "tokenizer_kb": round(tokenizer_size / 1024.0, 3),
        "runtime_dll_kb": round(runtime_dll_size / 1024.0, 3),
        "extensions_runtime_kb": round(extensions_library_size / 1024.0, 3),
        "support_assets_kb": round(max(model_dir_size - model_file_size - tokenizer_size, 0) / 1024.0, 3),
    }


def render_markdown(results: dict[str, Any]) -> str:
    lines = [
        "# Real ONNX Embedding Benchmark",
        "",
        f"- generated_at: `{results['generated_at']}`",
        f"- seed: `{results['seed']}`",
        f"- batch_size: `{results['batch_size']}`",
        f"- top_k: `{results['top_k']}`",
        "",
    ]

    for model_result in results["models"]:
        runtime = model_result["runtime"]
        lines.extend(
            [
                f"## {model_result['model_id']}",
                "",
                f"- repo: `{model_result['repo_id']}`",
                f"- init: `{runtime['session_init_ms']} ms`",
                f"- single encode: `{runtime['single_encode_ms']} ms/text`",
                f"- batch throughput: `{runtime['batch_texts_per_second']} texts/s`",
                f"- model file: `{runtime['model_file_kb']} KB`",
                f"- tokenizer: `{runtime['tokenizer_kb']} KB`",
                f"- runtime dll: `{runtime['runtime_dll_kb']} KB`",
                f"- extensions runtime: `{runtime['extensions_runtime_kb']} KB`",
                f"- rss: `{runtime['rss_mb']} MB`",
                "",
            ]
        )

        for task_result in model_result["tasks"]:
            if task_result["task_type"] == "sts":
                lines.extend(
                    [
                        f"### {task_result['dataset_id']} (STS)",
                        "",
                        f"- examples: `{task_result['examples']}`",
                        f"- spearman: `{task_result['spearman']:.6f}`",
                        f"- pearson: `{task_result['pearson']:.6f}`",
                        "",
                    ]
                )
            else:
                lines.extend(
                    [
                        f"### {task_result['dataset_id']} (Retrieval)",
                        "",
                        f"- queries: `{task_result['queries']}`",
                        f"- corpus: `{task_result['corpus']}`",
                        f"- Recall@{task_result['top_k']}: `{task_result['recall_at_k']:.6f}`",
                        f"- MRR@{task_result['top_k']}: `{task_result['mrr_at_k']:.6f}`",
                        f"- nDCG@{task_result['top_k']}: `{task_result['ndcg_at_k']:.6f}`",
                        "",
                    ]
                )

    return "\n".join(lines)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Benchmark real ONNX embedding models on real datasets.")
    parser.add_argument("--models", nargs="*", default=None, help="Model ids to run. Default: all configured models.")
    parser.add_argument("--datasets", nargs="*", default=None, help="Dataset ids to run. Default: all configured datasets.")
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--batch-size", type=int, default=16)
    parser.add_argument("--threads", type=int, default=0, help="ORT intra-op thread count. 0 keeps runtime default.")
    parser.add_argument("--top-k", type=int, default=10)
    parser.add_argument("--sts-limit", type=int, default=400)
    parser.add_argument("--retrieval-query-limit", type=int, default=120)
    parser.add_argument("--retrieval-corpus-limit", type=int, default=2000)
    parser.add_argument("--run-name", default="", help="Optional suffix for output files, for example ide_en_medium.")
    parser.add_argument("--output-json", default=str(RESULTS_JSON))
    parser.add_argument("--output-md", default=str(RESULTS_MD))
    return parser.parse_args()


def select_specs(config_items: list[dict[str, Any]], requested_ids: list[str] | None) -> list[dict[str, Any]]:
    if not requested_ids:
        return list(config_items)
    requested = set(requested_ids)
    selected = [item for item in config_items if item["id"] in requested]
    missing = requested.difference(item["id"] for item in selected)
    if missing:
        raise KeyError(f"unknown ids: {sorted(missing)}")
    return selected


def main() -> int:
    ensure_local_hf_cache()
    args = parse_args()
    BUILD_DIR.mkdir(parents=True, exist_ok=True)

    model_specs = select_specs(load_json_file(MODEL_CONFIG_PATH)["models"], args.models)
    dataset_specs = select_specs(load_json_file(DATASET_CONFIG_PATH)["datasets"], args.datasets)

    model_results = []
    for model_spec in model_specs:
        print(f"[real-onnx] loading model {model_spec['id']} from {model_spec['repo_id']}")
        embedder = OnnxEmbedder(model_spec, args.threads)
        runtime = measure_runtime(embedder, args.batch_size)

        task_results = []
        for dataset_spec in dataset_specs:
            print(f"[real-onnx] evaluating {model_spec['id']} on {dataset_spec['id']}")
            if dataset_spec["task_type"] == "sts":
                task_result = evaluate_sts(
                    embedder,
                    dataset_spec,
                    limit=min(args.sts_limit, dataset_spec.get("default_limit", args.sts_limit)),
                    seed=args.seed,
                    batch_size=args.batch_size,
                )
            else:
                task_result = evaluate_retrieval(
                    embedder,
                    dataset_spec,
                    query_limit=min(args.retrieval_query_limit, dataset_spec.get("default_query_limit", args.retrieval_query_limit)),
                    corpus_limit=min(args.retrieval_corpus_limit, dataset_spec.get("default_corpus_limit", args.retrieval_corpus_limit)),
                    seed=args.seed,
                    batch_size=args.batch_size,
                    top_k=args.top_k,
                )
            task_result["dataset_id"] = dataset_spec["id"]
            task_result["dataset_label"] = dataset_spec["label"]
            task_results.append(task_result)

        model_results.append(
            {
                "model_id": model_spec["id"],
                "repo_id": model_spec["repo_id"],
                "runtime": runtime,
                "tasks": task_results,
            }
        )

    results = {
        "generated_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "run_name": sanitize_run_name(args.run_name) if args.run_name else "",
        "seed": args.seed,
        "batch_size": args.batch_size,
        "top_k": args.top_k,
        "models": model_results,
    }

    if args.run_name:
        run_name = sanitize_run_name(args.run_name)
        output_json = BUILD_DIR / f"results_real_onnx_{run_name}.json"
        output_md = BUILD_DIR / f"results_real_onnx_{run_name}.md"
    else:
        output_json = Path(args.output_json)
        output_md = Path(args.output_md)
    output_json.parent.mkdir(parents=True, exist_ok=True)
    output_json.write_text(json.dumps(results, indent=2, ensure_ascii=False), encoding="utf-8")
    output_md.write_text(render_markdown(results), encoding="utf-8")
    print(f"[real-onnx] wrote {output_json}")
    print(f"[real-onnx] wrote {output_md}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
