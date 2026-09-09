# MiniLM Family Benchmark Report

更新时间：`2026-03-25`

这份报告用于整理新扩展出来的 `MiniLM` 分支候选，重点观察：

- 英文通用语义
- 英文技术问答检索
- 代码检索
- 多语统一 embedding 的轻量替代可能性

对应的原始 benchmark 结果：

- 英文 family：[results_real_onnx_english_family_compact.md](./build/real_onnx/results_real_onnx_english_family_compact.md)
- 多语 family：[results_real_onnx_multilingual_family_compact.md](./build/real_onnx/results_real_onnx_multilingual_family_compact.md)
- 英文模型中文补测：[results_real_onnx_english_models_on_zh.md](./build/real_onnx/results_real_onnx_english_models_on_zh.md)

## 基准说明

### 英文 family

- 数据集：
  - `mteb/stsbenchmark-sts`
  - `mteb/StackOverflowQA`
  - `mteb/HumanEvalRetrieval`
- 规模：
  - `STS`: `256`
  - `Retrieval queries`: `128`
  - `Retrieval corpus`: `1024`

### 多语 family

- 数据集：
  - `mteb/LCQMC`
  - `mteb/stsbenchmark-sts`
  - `mteb/T2Retrieval`
  - `mteb/StackOverflowQA`
- 规模：
  - `STS`: `256`
  - `Retrieval queries`: `128`
  - `Retrieval corpus`: `1024`

### 英文模型中文补测

- 数据集：
  - `mteb/LCQMC`
  - `mteb/T2Retrieval`
- 规模：
  - `STS`: `256`
  - `Retrieval queries`: `128`
  - `Retrieval corpus`: `1024`

## 英文 family 结果

### `bge-small-en-v1.5` 参考基线

- 模型文件：`66,465,124` bytes
- `STS Spearman`: `0.8394`
- `StackOverflowQA Recall@10`: `0.9453`
- `HumanEvalRetrieval Recall@10`: `0.8125`

### `all-MiniLM-L6-v2`

- 模型文件：`90,387,630` bytes
- tokenizer：`945,297` bytes
- `STS Spearman`: `0.7876`
- `StackOverflowQA Recall@10`: `0.9766`
- `HumanEvalRetrieval Recall@10`: `0.8281`
- 单条 encode：`3.198 ms`

判断：

- 这是这轮里最值得继续跟进的英文轻量候选之一。
- 它在英文 retrieval 上明显强，速度也很好。
- 如果未来想找 `bge-small-en-v1.5` 的强竞争者，它是目前最像样的一条。

### `all-MiniLM-L12-v2`

- 模型文件：`133,203,124` bytes
- tokenizer：`945,297` bytes
- `STS Spearman`: `0.7973`
- `StackOverflowQA Recall@10`: `1.0000`
- `HumanEvalRetrieval Recall@10`: `0.8828`
- 单条 encode：`10.707 ms`

判断：

- 在英文技术问答和代码检索上，这轮是最强者。
- 但它比 `all-MiniLM-L6-v2` 更大、更慢。
- 如果客户端资源允许，这条线很值得保留为高质量英文方案。

## 英文模型中文补测结果

### `all-MiniLM-L6-v2`

- `LCQMC Spearman`: `0.1875`
- `T2Retrieval Recall@10`: `0.1358`
- 中文综合分：约 `16.2`

判断：

- 中文表现非常弱。
- 这说明它虽然是强英文通用模型，但不能承担中英统一 embedding 的角色。

### `all-MiniLM-L12-v2`

- `LCQMC Spearman`: `0.1364`
- `T2Retrieval Recall@10`: `0.2142`
- 中文综合分：约 `17.5`

判断：

- 中文也明显不够。
- 它可以当英文高质量模型，但不能和 `multilingual-e5-small` 竞争“单模型兼顾中英”这个位置。

### `multi-qa-MiniLM-L6-cos-v1`

- 模型文件：`19,161,868` bytes
- tokenizer：`943,648` bytes
- `STS Spearman`: `0.7190`
- `StackOverflowQA Recall@10`: `0.7188`
- `HumanEvalRetrieval Recall@10`: `0.5859`
- 单条 encode：`2.619 ms`

判断：

- 体积非常有吸引力，是这批里最小的一个。
- 但在这组技术问答和代码检索任务上，结果并不好。
- 当前不适合当主线候选，只适合继续作为 ultra-light 实验分支。

### `paraphrase-MiniLM-L3-v2`

- 模型文件：`68,981,619` bytes
- tokenizer：`945,285` bytes
- `STS Spearman`: `0.7969`
- `StackOverflowQA Recall@10`: `0.7813`
- `HumanEvalRetrieval Recall@10`: `0.5078`
- 单条 encode：`2.133 ms`

判断：

- 很快，STS 还可以。
- 但 retrieval 明显偏弱，尤其代码检索不够。
- 更适合“超轻量通用句相似度”而不是 memory / kb retrieval 主线。

## 多语 family 结果

### `multilingual-e5-small` 参考基线

- 模型文件：`118,138,782` bytes
- tokenizer：`22,153,944` bytes
- `LCQMC Spearman`: `0.7649`
- `STS Spearman`: `0.8054`
- `T2Retrieval Recall@10`: `0.9153`
- `StackOverflowQA Recall@10`: `0.9453`

判断：

- 仍然是当前最稳的单模型多语方案。

### `distiluse-base-multilingual-v2-merged-onnx`

- 模型文件：`136,313,389` bytes
- tokenizer：`0` bytes
- 额外 `onnxruntime-extensions` 运行时：约 `2.68 MB`
- `LCQMC Spearman`: `0.6125`
- `STS Spearman`: `0.7585`
- `T2Retrieval Recall@10`: `0.7121`
- `StackOverflowQA Recall@10`: `0.7031`

注意：

- 这是 merged model，依赖 `onnxruntime-extensions`
- 当前 benchmark 为了避免超过内部 `512` token 限制，对输入做了 `384` 字符截断

判断：

- 部署形态很简洁，单文件很适合 edge 场景
- 但这轮准确度明显落后于 `multilingual-e5-small`
- 可以保留为“部署友好型实验候选”，不适合作为当前主线

### `paraphrase-multilingual-MiniLM-L12-v2`

- 模型文件：`235,052,644` bytes
- tokenizer：`31,848,649` bytes
- `LCQMC Spearman`: `0.5735`
- `STS Spearman`: `0.0857`
- `T2Retrieval Recall@10`: `0.6127`
- `StackOverflowQA Recall@10`: `0.3047`

判断：

- 这轮结果不具备竞争力。
- 体积更大，准确度反而更差。
- 当前可以直接降级为低优先级对照组。

## 总结建议

### 最值得继续探索的新增候选

- `all-MiniLM-L6-v2`
- `all-MiniLM-L12-v2`

原因：

- 都在英文 retrieval / code retrieval 上表现强
- 体积仍在本地化可接受区间
- 比很多多语模型更轻
- 比 `multi-qa-MiniLM-L6-cos-v1` 和 `paraphrase-MiniLM-L3-v2` 更像真正可用的主线候选
- 但这条结论仅限英文 / 代码路线，不适用于多语统一路线

### 可以保留但不建议主推的候选

- `paraphrase-MiniLM-L3-v2`
- `distiluse-base-multilingual-v2-merged-onnx`

原因：

- 前者更像“超轻量通用相似度模型”
- 后者更像“部署极简模型”
- 但两者在 retrieval 上都没有给出足够强的结果

### 当前不建议继续投入的候选

- `multi-qa-MiniLM-L6-cos-v1`
- `paraphrase-multilingual-MiniLM-L12-v2`

原因：

- 在这轮实际任务上没有表现出预期价值

## 对 `xllm` 的现实启发

如果后续要继续走这个分支，最有价值的路线不是“再堆更多 MiniLM 名字相似的模型”，而是收敛成两条：

- 英文 / 代码：`all-MiniLM-L6-v2` vs `all-MiniLM-L12-v2` vs `bge-small-en-v1.5`
- 多语统一：`multilingual-e5-small` vs `distiluse-base-multilingual-v2-merged-onnx`

补充判断：

- `all-*` 在官方 SBERT 定位里是 general-purpose，但这里的“通用”更接近英文任务通用，不是多语言通用。
- 从补测看，`all-MiniLM-L6-v2` 和 `all-MiniLM-L12-v2` 都不适合拿来直接做中文或中英混合统一索引。

`paraphrase-multilingual-MiniLM-L6-v2`：

- 截至这轮整理，没有找到一个清晰、稳定、可直接纳入当前 ONNX benchmark 的公开模型仓库
- 因此没有纳入正式对比结果
