# onnxdemo

`onnxdemo` 是一个最小 ONNX Runtime 文本向量化 demo。

它不追求模型质量，只验证这几件事已经通了：

- 文本经过本地 tokenizer 变成 `bag-of-words`
- ONNX 模型把 `bag-of-words` 投影为固定维度向量
- C 程序通过 ONNX Runtime C API 取回向量
- 余弦相似度可以区分“相关文本”和“无关文本”

## 模型形态

demo 模型由 [tools/generate_demo_assets.py](./tools/generate_demo_assets.py) 生成：

- 输入：`[1, vocab_size]` 的浮点型 bag-of-words
- 图结构：`MatMul(bow, token_embeddings)`
- 输出：`[1, embedding_dim]`

为了兼容中文无空格输入，tokenizer 规则是：

- ASCII 单词按 `[a-z0-9_-]+` 聚合
- CJK 字符逐字切分
- 连续 CJK 字符额外生成相邻 bigram

这让 toy 模型能在不依赖复杂 tokenizer 的前提下，先把 ONNX 推理链路跑通。

## 构建

在 `dev/onnxdemo` 下执行：

```bat
build_demo.bat
```

它会做这些事：

1. 调用 [../minionnx/prepare_demo_minimal_assets.ps1](../minionnx/prepare_demo_minimal_assets.ps1) 生成：
   - `toy_text_embedder.onnx`
   - `toy_text_embedder.ort`
   - `manual_required_ops.config`
   - `toy_text_embedder.required_operators_and_types.config`
2. 下载官方预编译 `onnxruntime-win-x64-1.24.4.zip`
3. 用 `gcc` 编译 [demo_main.c](./demo_main.c) 和 [src/onnx_demo.c](./src/onnx_demo.c)
4. 运行 demo，输出 embedding 维度、命中 token 数和余弦相似度

## 说明

- 这里使用预编译 runtime 只是为了先验证 C API 集成。
- 真正的最小 runtime 构建入口在 [../minionnx](../minionnx/README.md)。
