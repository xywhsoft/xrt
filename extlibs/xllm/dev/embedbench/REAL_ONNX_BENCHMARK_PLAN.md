# Real ONNX Benchmark Plan

当前 `embedbench` 已经把 `fastText / word2vec / toy onnx` 的合成口径跑通了。

下一阶段改成“真实数据 + 真实 ONNX embedding 模型”评测，目标不是追求一次性覆盖全部榜单，而是先跑出一个对 `xllm` 有决策价值的本地 CPU 评测基线。

## 目标

- 用真实公开数据替换手工 toy case
- 只评测小模型，优先 `100 MB` 级别或附近的 ONNX 资产
- 默认按 CPU-only、本地 brute-force 检索评测
- 先覆盖：
  - 中文检索
  - 英文检索
  - 中文语义相似度
  - 英文语义相似度

## 默认候选模型

### 1. `BAAI/bge-small-zh-v1.5`

- 角色：中文优先 baseline
- 适合：中文知识库、中文记忆检索
- 关键点：
  - 检索 query 需要加中文 instruction
  - passage 不加 instruction
  - embedding 用 `[CLS]`，并做 normalize

### 2. `intfloat/multilingual-e5-small`

- 角色：中英双语 / 多语 baseline
- 适合：中英混合文档、跨语言 memory/kb
- 关键点：
  - query 要加 `query: `
  - passage 要加 `passage: `
  - STS/对称任务统一走 `query: `
  - embedding 采用 attention-mask aware mean pooling，再 normalize

### 3. `sentence-transformers/paraphrase-multilingual-MiniLM-L12-v2`

- 角色：成熟轻量对照组
- 适合：通用相似度、轻量多语 baseline
- 关键点：
- 默认 mean pooling
- `max_seq_length=128`
- 更适合短文本，不适合长 chunk

### 4. `BAAI/bge-small-en-v1.5`

- 角色：英文 retrieval / code retrieval 对照组
- 适合：英文文档、技术问答、代码检索
- 关键点：
  - query 需要加英文 instruction
  - passage 不加 instruction
  - embedding 用 `[CLS]`，并做 normalize

## 默认评测数据

### 英文检索

`mteb/StackOverflowQA`

- 规模适中
- 数据体积相对可控
- 更贴近代码/技术问答场景

### 代码检索

`mteb/HumanEvalRetrieval`

- 规模小，适合本地 CPU 快速迭代
- query 是自然语言任务描述，corpus 是代码实现
- 很适合补 IDE / code memory 的近似场景

### 中文检索

`mteb/T2Retrieval`

- 中文检索任务
- 数据规模足够，适合作为中文主检索 benchmark
- v1 先采样，不直接全量 brute-force

### 英文相似度

`mteb/stsbenchmark-sts`

- 标准 STS 基线
- 用于看 embedding 的排序一致性

### 中文相似度

`mteb/LCQMC`

- 中文句对相似度常用基线
- 标签分布比 `AFQMC` 更适合直接做相关性评测

## v1 评测口径

### Retrieval

- 指标：
  - `Recall@10`
  - `MRR@10`
  - `nDCG@10`（可选，第二阶段补）
- 评测方式：
  - CPU 上 brute-force cosine
  - 为了控制时长，v1 对大数据集做采样：
    - query 采样 `200-500`
    - corpus 采样 `2k-10k`
- 采样规则：
  - 固定随机种子
  - 每个 query 至少保留一个正样本
  - 尽量引入 hard negatives

### STS

- 指标：
  - `Spearman`
  - `Pearson`（可选）
- 评测方式：
  - 句对余弦相似度 vs 标注分数

### Runtime

- 指标：
  - 模型下载体积
  - ONNX 文件体积
  - tokenizer 体积
  - runtime DLL 体积
  - 初始化耗时
  - 单条 query 向量化耗时
  - batch 向量化吞吐
  - 常驻内存峰值

## 实现约束

- 不用 toy tokenizer
- 直接使用模型原生 tokenizer
- 不同模型严格按自己的 pooling / prefix 规则执行
- retrieval 和 STS 分开配置前处理
- 不混用不同模型的输入规范

## 代码结构建议

建议在 `dev/embedbench` 里新增第二条评测链路：

- `real_models.json`
  - 模型元数据
  - 下载地址
  - pooling 规则
  - query/doc 前缀规则
- `real_datasets.json`
  - 数据集来源
  - split
  - 采样规则
  - 字段映射
- `run_real_onnx_bench.py`
  - 下载模型与数据
  - 跑 STS / Retrieval
  - 输出统一结果

## 默认第一批要跑的矩阵

### 最小矩阵

- `bge-small-zh-v1.5`:
  - `LCQMC`
  - `T2Retrieval`

- `multilingual-e5-small`:
  - `LCQMC`
  - `stsbenchmark-sts`
  - `T2Retrieval`
  - `StackOverflowQA`
  - `HumanEvalRetrieval`

- `paraphrase-multilingual-MiniLM-L12-v2`:
  - `LCQMC`
  - `stsbenchmark-sts`
  - `StackOverflowQA`

- `bge-small-en-v1.5`:
  - `stsbenchmark-sts`
  - `StackOverflowQA`
  - `HumanEvalRetrieval`

### 第二批再补

- 更真实的代码检索任务
- 更长文档检索任务

## 先不做的事

- 不先接 ANN/HNSW
- 不先做 reranker
- 不先做跨机器性能对比
- 不先跑全量 MTEB / C-MTEB

## 成败判断

如果一套小模型同时满足下面几条，就值得进入 `xllm` 主线候选：

- CPU 延迟可接受
- 中文或中英检索至少有一条主任务明显领先
- STS 排序稳定，不出现明显塌缩
- 包体积在本地化可接受范围内
- 输入规范清晰，易于工程封装
