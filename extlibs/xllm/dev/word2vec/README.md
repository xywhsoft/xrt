# word2vec Demo

这个目录放一个最小可运行的 `word2vec` demo，用来验证两件事：

- 文本转向量
- 两段文本的关联性计算（余弦相似度）

## 目录

- `vendor/word2vec`
  - 官方 `word2vec.c` 训练器和许可证
- `word2vec_demo.h`
  - demo 头文件
- `src/word2vec_demo.c`
  - 二进制模型加载器和文本向量化逻辑
- `demo_main.c`
  - demo 主程序
- `demo_corpus.txt`
  - 小型训练语料
- `build_demo.bat`
  - Windows 下的最小构建脚本

## 实现方式

这个 demo 分两步：

1. 编译官方 `word2vec.c` 为训练器
2. 第一次运行时训练一个小模型，再由本地加载器读取 `.bin` 模型

文本向量的计算方式是：

- 按空格切词
- 查词向量
- 对命中的词向量做平均
- 用余弦相似度比较两段文本

这符合 `word2vec` 的常见 baseline 用法，但不是生产级语义检索实现。

## 依赖

需要本机有 `gcc`。

构建脚本使用和仓库其他 demo 一样的本地命令行编译方式，不依赖 CMake。

## 构建与运行

在当前目录执行：

```bat
build_demo.bat
```

脚本会：

1. 编译 `word2vec` 训练器
2. 编译 demo 主程序
3. 第一次运行时，用 `demo_corpus.txt` 训练 `build/demo_model.bin`
4. 输出几组内置文本的向量相似度

## 命令行用法

不带参数：

```bat
build\word2vec_demo.exe
```

输出内置样例的相似度和部分向量维度。

带 1 个参数：

```bat
build\word2vec_demo.exe "向量 检索 文本 切分"
```

输出这段文本的向量预览和命中词数。

带 2 个参数：

```bat
build\word2vec_demo.exe "向量 检索 文本 切分" "语义 检索 向量 索引"
```

输出两段文本的余弦相似度和命中词数。

## 注意

- 这是最小 demo，不是生产级封装。
- 训练语料很小，相似度结果主要用于验证链路，不代表真实质量上限。
- 对未登录词，当前 demo 的策略是直接忽略。
- 中文需要预先分词或至少用空格分隔 token；这里的训练语料已经按空格切词。

## 上游来源

- upstream: [tmikolov/word2vec](https://github.com/tmikolov/word2vec)
- license: `Apache-2.0`, 见 [vendor/word2vec/LICENSE](./vendor/word2vec/LICENSE)
