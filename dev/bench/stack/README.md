# Stack 性能基准

`bench_stack.c` 固化两条连续动态栈路径：

- 预留容量后的 `Push/Pop` 热路径；
- 从空栈开始的 `Add` 摊销增长路径。

默认参数为 `100000` 轮、每轮 `64` 个元素，以及 `1000000`
个增长元素。统计中的一次 operation 表示一次 Push、Pop 或 Add。

## GCC x64

```text
gcc -O3 -std=c11 -Wall -Wextra -Werror -m64 \
	-DXRT_FEATURE_ARRAY -DXRT_FEATURE_STACK -I include \
	dev\bench\stack\bench_stack.c \
	src\core\core.c src\memory\allocator.c src\memory\heap.c \
	src\core\error.c src\containers\array.c src\containers\stack.c \
	-o out\bench_stack.exe
out\bench_stack.exe 100000 64 1000000
```

## TinyCC x86

```text
tcc -O2 -Wall -m32 \
	-DXRT_FEATURE_ARRAY -DXRT_FEATURE_STACK -I include \
	dev\bench\stack\bench_stack.c \
	src\core\core.c src\memory\allocator.c src\memory\heap.c \
	src\core\error.c src\containers\array.c src\containers\stack.c \
	-o out\bench_stack_tcc.exe
out\bench_stack_tcc.exe 100000 64 1000000
```

当前环境基线见 `STACK_BENCH_20260728.md`。
