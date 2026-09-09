# minionnx

`minionnx` 用来为 `dev/onnxdemo` 的 toy embedding 模型准备最小化 ONNX Runtime 资产，而不是直接提供一个完整的通用 runtime。

默认流程分两步：

1. 生成 demo 模型与裁剪配置
2. 用这些配置去驱动 ONNX Runtime 的最小化源码构建

当前目录提供的脚本：

- `prepare_demo_minimal_assets.ps1`
  - 本地安装仅用于构建的 Python 依赖：`onnx==1.18.0`、`onnxruntime==1.24.4`
  - 调用 [../onnxdemo/tools/generate_demo_assets.py](../onnxdemo/tools/generate_demo_assets.py) 生成 `toy_text_embedder.onnx`
  - 生成手工 reduced-ops 配置
  - 使用官方 `onnxruntime.tools.convert_onnx_models_to_ort` 把 `.onnx` 转成 `.ort`，并输出 `toy_text_embedder.required_operators_and_types.config`

- `generate_reduced_ops_config.py`
  - 从任意 `.onnx` 模型图中提取 `domain;opset;op1,op2,...` 格式的 reduced-ops 配置

- `build_minimal_windows.ps1`
  - 接收一个本地 ONNX Runtime 源码目录
  - 使用 `--minimal_build --include_ops_by_config ...` 打印或执行最小化构建命令
  - 默认会附带 `--disable_ml_ops --disable_exceptions --disable_rtti --enable_reduced_operator_type_support`
  - 默认只构建 `onnxruntime` target，避免把共享库测试程序一起编进来
  - 目标是给当前 `CPU-only + embedding` 场景生成更小的模型定制版 runtime DLL

- `prepare_model_minimal_assets.ps1`
  - 针对任意固定 `.onnx` 模型生成：
    - 复制后的模型文件
    - reduced-ops config
    - `.ort` 模型
    - typed reduced-ops config

- `prepare_multilingual_e5_small_minimal_assets.ps1`
  - 以当前仓库 benchmark 缓存里的 `multilingual-e5-small` 为输入
  - 在 `build/multilingual-e5-small/` 下生成专用 minimal-build 资产

- `build_multilingual_e5_small_minimal_windows.ps1`
  - 以 `multilingual-e5-small` 的 typed config 为输入
  - 调用 `build_minimal_windows.ps1` 生成只服务这个模型的 Windows 最小化 runtime 构建命令

- `fetch_onnxruntime_source.ps1`
  - 拉取指定 tag 的 ONNX Runtime 源码并初始化子模块

- `install_minimal_msvc_env.ps1`
  - 使用 `winget` 安装最小的 Windows C/C++ 构建环境
  - 默认安装：
    - `Visual Studio BuildTools 2022`
    - `MSVC x64/x86`
    - `Windows 11 SDK`
    - 独立 `Kitware.CMake`
  - 如果传 `-WithVsCMake`，则改为安装 Visual Studio 自带的 CMake 组件

## Demo 模型的最小化目标

`onnxdemo` 里的 toy 模型非常小，只做一件事：

- 输入：`[1, vocab_size]` 的 bag-of-words 浮点张量
- 计算：`MatMul(bow, token_embedding_matrix)`
- 输出：`[1, embedding_dim]` 的文本向量

因此它的核心需求只落在 `ai.onnx:MatMul` 上。为了和官方最小化流程保持一致，这里仍然会生成：

- `manual_required_ops.config`
- `toy_text_embedder.required_operators_and_types.config`
- `toy_text_embedder.ort`

## 使用

源码级最小化构建至少需要：

- `cmake`
- Windows C/C++ toolchain（推荐 Visual Studio Build Tools / MSVC）

如果本机还没有这些依赖，可以先打印安装命令：

```powershell
pwsh -File .\install_minimal_msvc_env.ps1 -PrintOnly
```

如果你接受直接安装最小环境：

```powershell
pwsh -File .\install_minimal_msvc_env.ps1
```

先准备 demo 资产：

```powershell
pwsh -File .\prepare_demo_minimal_assets.ps1
```

如果你有本地 ONNX Runtime 源码和 `cmake`，可以继续生成最小化 runtime：

```powershell
pwsh -File .\build_minimal_windows.ps1 -OrtRepo D:\src\onnxruntime
```

如果本机还没有 `cmake`，脚本会只打印推荐命令，不会强行失败。

## multilingual-e5-small 固定模型裁剪

如果你的目标是：

- 只跑 `multilingual-e5-small`
- 只加载 `.ort`
- 只保留 CPU 推理

那建议直接走这条专用流程：

```powershell
pwsh -File .\prepare_multilingual_e5_small_minimal_assets.ps1
pwsh -File .\fetch_onnxruntime_source.ps1
pwsh -File .\build_multilingual_e5_small_minimal_windows.ps1 -OrtRepo .\build\onnxruntime-src
```

其中会使用：

- [model.onnx](../embedbench/build/real_onnx/model_cache/WiseIntelligence__multilingual-e5-small-Optimum-ONNX-Quantized-AVX2/model.onnx)
- 生成的 ORT 模型：[model.ort](./build/multilingual-e5-small/ort/model.ort)
- 生成的 typed config：[model.required_operators_and_types.config](./build/multilingual-e5-small/ort/model.required_operators_and_types.config)

当前这份 `multilingual-e5-small` 的 typed config 只保留了一小组必需算子，其中包含量化相关的 `com.microsoft` 域算子，因此非常适合 model-specific minimal build。

## 说明

- 当前仓库里的 `onnxdemo` 为了保证“先能跑起来”，默认使用官方预编译的 `onnxruntime-win-x64-1.24.4.zip` 进行验证。
- 这个包已经是 CPU 版，不是带 CUDA / DirectML / QNN 的专用 provider 包；如果还想继续缩减 DLL，关键是做源码级 custom build，而不是只盯着 provider 开关。
- 对当前这类 `multilingual-e5-small` CPU-only embedding 场景，推荐路线是：
  1. 保留量化模型
  2. 优先转换为 `.ort` 模型格式
  3. 用模型导出的 reduced-ops config 只保留必需算子
  4. 用 `MinSizeRel + --minimal_build + --disable_ml_ops + --disable_exceptions + --disable_rtti + --enable_reduced_operator_type_support` 构建共享库
- 真正的 runtime 裁剪，依赖 ONNX Runtime 官方的 minimal build 路线：`ORT format + reduced operator config + model-specific custom build`。

## 当前实测

基于当前仓库缓存里的 `multilingual-e5-small` 量化模型，按“固定模型 + 只加载 `.ort` + CPU-only”路线实测得到：

- 精简版 DLL：[onnxruntime.dll](./build/onnxruntime-1.24.4/build/Windows/MinSizeRel/MinSizeRel/onnxruntime.dll)
- 体积：`1,982,976` bytes，约 `1.89 MiB`
- 对比官方 CPU 预编译 DLL：[onnxruntime.dll](../onnxdemo/build/onnxruntime.dll)
- 官方 DLL 体积：`14,203,464` bytes，约 `13.55 MiB`

这意味着当前专用最小化 runtime 相比官方 CPU DLL 约缩减了 `86%`。

另外还用一个最小验证器确认了：

- 能动态加载这份精简 DLL
- 能成功创建 `multilingual-e5-small` 的 `.ort` session

验证器源码见 [verify_minimal_runtime.c](./verify_minimal_runtime.c)。

参考：

- [ONNX Runtime custom build](https://onnxruntime.ai/docs/build/custom.html)
- [Reduced operator config](https://onnxruntime.ai/docs/reference/operators/reduced-operator-config-file.html)
- [ORT format models](https://onnxruntime.ai/docs/performance/model-optimizations/ort-format-models.html)
