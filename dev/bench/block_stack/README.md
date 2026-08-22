# BlockStack 性能基准

`bench_block_stack.c` 固化三条稳定地址分块栈路径：

- 预留容量后的 `Push/Pop` 热路径；
- 从空栈开始的 `Add` 按块增长路径；
- 已构建深栈的循环顺序 `ConstGet` 路径。

默认参数为 `100000` 轮、每轮 `64` 个元素、`1000000` 个增长元素，
以及 `4000000` 次索引访问。统计中的一次 operation 表示一次
Push、Pop、Add 或 ConstGet。基准还验证首次添加元素的地址在全部增长后保持不变。

## GCC x64

```text
gcc -O3 -std=c11 -Wall -Wextra -Werror -m64 \
	-DXRT_FEATURE_ARRAY -DXRT_FEATURE_BLOCK_STACK -I include \
	dev\bench\block_stack\bench_block_stack.c \
	src\core\core.c src\memory\allocator.c src\memory\heap.c \
	src\core\error.c src\containers\array.c \
	src\containers\block_stack.c \
	-o out\bench_block_stack.exe
out\bench_block_stack.exe 100000 64 1000000 4000000
```

## TinyCC x86

```text
tcc -O2 -Wall -m32 \
	-DXRT_FEATURE_ARRAY -DXRT_FEATURE_BLOCK_STACK -I include \
	dev\bench\block_stack\bench_block_stack.c \
	src\core\core.c src\memory\allocator.c src\memory\heap.c \
	src\core\error.c src\containers\array.c \
	src\containers\block_stack.c \
	-o out\bench_block_stack_tcc.exe
out\bench_block_stack_tcc.exe 100000 64 1000000 4000000
```

当前环境基线见 `BLOCK_STACK_BENCH_20260728.md`。
