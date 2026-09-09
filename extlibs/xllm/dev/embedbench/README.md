# embedbench

`embedbench` 用来同口径比较这三条 demo 路线：

- `fastText`
- `word2vec`
- `onnx`

比较口径：

- 使用同一份训练/生成语料：[benchmark_corpus.txt](./benchmark_corpus.txt)
- 使用同一组文本对：[benchmark_cases.tsv](./benchmark_cases.tsv)
- 输出同一批指标：
  - 向量维度
  - 模型体积
  - runtime 体积
  - 支撑资产体积
  - 初始化耗时
  - 单文本平均 embedding 耗时
  - 单对文本平均 compare 耗时
  - related / unrelated 平均相似度
  - 各 case 的相似度分数

运行：

```bat
run_benchmarks.bat
```

可选参数：

```bat
run_benchmarks.bat --warmup 50 --iterations 500
```

产物：

- [build/results.json](./build/results.json)
- [build/results.md](./build/results.md)

说明：

- `fastText` 和 `word2vec` 会在 benchmark 目录下训练自己的 benchmark 模型。
- `onnx` 会复用 `minionnx` 的准备脚本，但改为使用 benchmark 语料生成单独的 ONNX/ORT 模型资产。

## Real ONNX Benchmark

除了 toy benchmark，这里还提供一条“真实数据 + 真实 ONNX embedding 模型”的评测链路：

- 模型配置：[real_models.json](./real_models.json)
- 数据集配置：[real_datasets.json](./real_datasets.json)
- 运行脚本：[run_real_onnx_bench.py](./run_real_onnx_bench.py)
- Windows 包装脚本：[run_real_onnx_bench.bat](./run_real_onnx_bench.bat)
- 设计说明：[REAL_ONNX_BENCHMARK_PLAN.md](./REAL_ONNX_BENCHMARK_PLAN.md)
- 当前结论摘要：[REAL_ONNX_RESULTS_SUMMARY.md](./REAL_ONNX_RESULTS_SUMMARY.md)
- MiniLM 分支报告：[MINILM_FAMILY_REPORT.md](./MINILM_FAMILY_REPORT.md)

默认候选模型：

- `Qdrant/bge-small-en-v1.5-onnx-Q`
- `Qdrant/bge-small-zh-v1.5`
- `WiseIntelligence/multilingual-e5-small-Optimum-ONNX-Quantized-AVX2`
- `Qdrant/paraphrase-multilingual-MiniLM-L12-v2-onnx-Q`

默认数据集：

- `mteb/LCQMC`
- `mteb/stsbenchmark-sts`
- `mteb/T2Retrieval`
- `mteb/StackOverflowQA`
- `mteb/HumanEvalRetrieval`

运行：

```bat
run_real_onnx_bench.bat
```

小规模 smoke run：

```bat
run_real_onnx_bench.bat --models bge-small-zh-v1.5 multilingual-e5-small --datasets lcqmc t2-retrieval --sts-limit 128 --retrieval-query-limit 48 --retrieval-corpus-limit 512
```

更贴近 IDE 的英文技术/代码检索：

```bat
run_real_onnx_bench.bat --run-name ide_en_medium --models bge-small-en-v1.5 multilingual-e5-small --datasets stsbenchmark-sts stackoverflowqa humaneval-retrieval --sts-limit 256 --retrieval-query-limit 128 --retrieval-corpus-limit 2000
```

产物：

- [build/real_onnx/results_real_onnx.json](./build/real_onnx/results_real_onnx.json)
- [build/real_onnx/results_real_onnx.md](./build/real_onnx/results_real_onnx.md)

说明：

- 真实 benchmark 不再使用 toy tokenizer，而是直接使用模型原生 tokenizer。
- 每个模型严格按自己的 prefix / pooling / normalize 规则执行。
- retrieval 默认做 CPU brute-force cosine，方便先比较模型质量与本地 CPU 可行性。
- `mteb/AFQMC` 当前 test split 标签单一，不适合作为相关性基线，因此默认换成 `mteb/LCQMC`。
- `mteb/HumanEvalRetrieval` 用来补一条更贴近代码检索的 baseline。
- 如果传 `--run-name`，结果会输出到 `results_real_onnx_<run-name>.json/.md`，避免多轮测试互相覆盖。
