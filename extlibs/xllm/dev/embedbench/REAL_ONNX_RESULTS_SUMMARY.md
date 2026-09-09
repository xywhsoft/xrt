# Real ONNX Results Summary

更新时间：`2026-03-25`

这份摘要基于当前已经落盘的真实 ONNX benchmark 结果：

- 中文中等规模：[results_real_onnx_zh_medium.md](./build/real_onnx/results_real_onnx_zh_medium.md)
- 英文技术/代码中等规模：[results_real_onnx_ide_en_large.md](./build/real_onnx/results_real_onnx_ide_en_large.md)
- 英文模型中文补测：[results_real_onnx_english_models_on_zh.md](./build/real_onnx/results_real_onnx_english_models_on_zh.md)
- 英文 MiniLM 分支：[results_real_onnx_english_family_compact.md](./build/real_onnx/results_real_onnx_english_family_compact.md)
- 多语分支：[results_real_onnx_multilingual_family_compact.md](./build/real_onnx/results_real_onnx_multilingual_family_compact.md)
- 早期 smoke run：[results_real_onnx.md](./build/real_onnx/results_real_onnx.md)

## 综合对比表

下面这张表使用当前统一口径整理：

- 中文准确率：`(LCQMC Spearman + T2Retrieval Recall@10) / 2`
- 英文准确率：`STSBenchmark Spearman`、`StackOverflowQA Recall@10`、`HumanEvalRetrieval Recall@10` 的平均
- 混合准确率：`(中文准确率 + 英文准确率) / 2`
- `-` 表示当前没有对应语言的完整评测

| 模型 | 文件大小 | RSS | 单句 CPU | 中文准确率 | 英文准确率 | 混合准确率 |
|---|---:|---:|---:|---:|---:|---:|
| `bge-small-zh-v1.5` | `90.4 MiB` | `336.4 MB` | `4.507 ms` | `83.2` | `-` | `-` |
| `bge-small-en-v1.5` | `63.4 MiB` | `345.1 MB` | `7.605 ms` | `-` | `87.0` | `-` |
| `multilingual-e5-small` | `112.7 MiB` | `662.0 MB` | `5.727 ms` | `81.1` | `82.7` | `81.9` |
| `distiluse-base-multilingual-v2-merged-onnx` | `130.0 MiB` | `363.3 MB` | `6.052 ms` | `66.2` | `73.1` | `69.7` |
| `paraphrase-multilingual-MiniLM-L12-v2` | `224.2 MiB` | `665.9 MB` | `7.013 ms` | `59.3` | `19.5` | `39.4` |
| `all-MiniLM-L6-v2` | `86.2 MiB` | `350.3 MB` | `3.198 ms` | `16.2` | `86.4` | `51.3` |
| `all-MiniLM-L12-v2` | `127.0 MiB` | `396.5 MB` | `10.707 ms` | `17.5` | `89.3` | `53.4` |
| `multi-qa-MiniLM-L6-cos-v1` | `18.3 MiB` | `288.8 MB` | `2.619 ms` | `-` | `67.5` | `-` |
| `paraphrase-MiniLM-L3-v2` | `65.8 MiB` | `893.1 MB` | `2.133 ms` | `-` | `69.5` | `-` |

## 当前结论

### 1. 中文路线

当前更合适的默认主候选是 `bge-small-zh-v1.5`。

原因：

- 在 `mteb/T2Retrieval` 上明显强于 `multilingual-e5-small`
  - `Recall@10`: `0.9211` vs `0.8753`
  - `nDCG@10`: `0.9299` vs `0.8834`
- 常驻内存和模型体积明显更小
  - RSS 约 `336 MB` vs `706 MB`
  - 模型文件约 `92.6 MB` vs `115.4 MB`
- 中文相似度 `LCQMC` 上没有明显吃亏
  - Spearman `0.7434` vs `0.7471`

默认判断：

- 如果 `xllm` 的本地 memory / kb 优先服务中文场景，`bge-small-zh-v1.5` 更适合作为主线候选。

### 2. 英文技术 / 代码路线

当前更合适的英文候选需要分成“最高质量”和“更均衡”两类来看。

原因：

- `all-MiniLM-L12-v2` 目前是英文技术问答和代码检索最强者
  - `StackOverflowQA Recall@10`: `1.0000`
  - `HumanEvalRetrieval Recall@10`: `0.8828`
- `all-MiniLM-L6-v2` 是英文路线里很强的均衡点
  - `StackOverflowQA Recall@10`: `0.9766`
  - `HumanEvalRetrieval Recall@10`: `0.8281`
  - 单句 encode 约 `3.20 ms`
- `bge-small-en-v1.5` 仍然是一个很有价值的英文紧凑基线
  - 文件更小：约 `63.4 MiB`
  - `STS Spearman` 仍然最高：`0.8625`
  - 综合英文准确率 `87.0`

默认判断：

- 如果追求英文检索质量上限：优先看 `all-MiniLM-L12-v2`
- 如果追求英文路线的性价比和平衡：优先看 `all-MiniLM-L6-v2`
- 如果追求更小文件体积且仍保持较强英文表现：`bge-small-en-v1.5` 仍然值得保留

### 3. 中英统一路线

如果必须只保留一套中英双语模型，当前仍是 `multilingual-e5-small` 最值得保留。

原因：

- 它在中文和英文任务上都能维持较强表现
- 英文侧虽然输给 `all-MiniLM-L12-v2` / `all-MiniLM-L6-v2` / `bge-small-en-v1.5`
- 中文 retrieval 虽然输给 `bge-small-zh-v1.5`
- 但它仍然是当前最像“单模型兼顾中英”的方案
- `all-MiniLM-L6-v2` 和 `all-MiniLM-L12-v2` 虽然英文很强，但中文补测只有 `16-18` 分量级，不能作为多语统一模型候选

代价：

- 模型体积更大
- tokenizer 明显更大
- RSS 接近 `650-700 MB`
- 初始化更慢

默认判断：

- 如果后续 API 允许按语言切模型，不建议让 `multilingual-e5-small` 独占主线。
- 如果必须先只上一套统一模型，它仍然是最合理的 compromise。

### 4. 已经基本可以降级的候选

`paraphrase-multilingual-MiniLM-L12-v2` 当前可以降级为对照组，不建议继续作为主线候选。

原因：

- 英文 STS 表现异常弱
- retrieval 表现也明显落后
- 包体积并不占优

## 当前推荐

### 推荐策略 A：双模型主线

- 中文：`bge-small-zh-v1.5`
- 英文/代码：`all-MiniLM-L6-v2` 或 `all-MiniLM-L12-v2`

适合：

- `xllm_memory` / `xllm_kb` 后续允许按语言或场景选 embedder
- 本地化优先
- 希望保持更低内存和更强场景匹配

补充：

- `all-MiniLM-L6-v2` 更适合追求速度与体积平衡
- `all-MiniLM-L12-v2` 更适合追求英文检索质量上限
- `bge-small-en-v1.5` 仍可作为英文紧凑 baseline 保留

### 推荐策略 B：单模型折中

- 统一使用：`multilingual-e5-small`

适合：

- 只想维护一套模型资产
- 中英混合文档很多
- 可以接受更高内存和更大 tokenizer

## 还需要补的验证

- 更贴近真实代码库的 retrieval 数据，而不只是 `HumanEvalRetrieval`
- 更长 chunk 的稳定性验证
- query/document 长度分布更接近真实 memory/kb 的压测
- 持续多轮 benchmark，确认排序不会随采样波动反转
- 后续再补 tokenizer 预热耗时、冷启动耗时、批量导入吞吐
