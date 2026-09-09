# fastText Demo

这个目录放一个最小可运行的 `fastText` demo，用来验证两件事：

- 文本转向量
- 两段文本的关联性计算（余弦相似度）

## 目录

- `vendor/fastText`
  - 官方 `fastText` 源码最小集，只保留 `src/` 和 `LICENSE`
- `fasttext_demo.h`
  - 薄 C 风格封装头
- `src/fasttext_demo.cpp`
  - 用官方 `fastText` C++ API 包装成可被 C 调用的接口
- `demo_main.c`
  - demo 主程序
- `demo_corpus.txt`
  - 小型训练语料
- `build_demo.bat`
  - Windows 下的最小构建脚本

## 依赖

需要本机有 `g++`。

构建脚本使用的是和仓库其他示例一致的本地命令行编译方式，不依赖 CMake。

## 构建与运行

在当前目录执行：

```bat
build_demo.bat
```

脚本会：

1. 编译官方 `fastText` 源码和 demo 封装
2. 生成 `build/fasttext_demo.exe`
3. 第一次运行时，用 `demo_corpus.txt` 训练一个小模型
4. 输出几组内置文本的向量相似度

## 命令行用法

不带参数：

```bat
build\fasttext_demo.exe
```

输出内置样例的相似度和部分向量维度。

带 1 个参数：

```bat
build\fasttext_demo.exe "向量 检索 文本 切分"
```

输出这段文本的向量预览。

带 2 个参数：

```bat
build\fasttext_demo.exe "向量 检索 文本 切分" "语义 检索 向量 索引"
```

输出两段文本的余弦相似度。

## 注意

- 这是最小 demo，不是生产级封装。
- 训练语料很小，相似度结果主要用于验证链路，不代表真实质量上限。
- `fastText` 的句向量是基于词向量聚合，空格分词效果更直观；中文正式接入时仍需要单独讨论预处理策略。

## 上游来源

- upstream: [facebookresearch/fastText](https://github.com/facebookresearch/fastText)
- license: `MIT`, 见 [vendor/fastText/LICENSE](./vendor/fastText/LICENSE)
