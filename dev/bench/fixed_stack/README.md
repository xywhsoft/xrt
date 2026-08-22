# FixedStack 性能基准

`bench_fixed_stack.c` 固化三个不分配内存的热路径：

- `Add` 写入与 `Pop` 复制；
- `Push` 复制与 `Pop` 复制；
- `ConstTop` 只读栈顶。

默认运行 `100000` 轮、每轮 `64` 个元素，共计每组 `12800000`
次栈操作。第二个参数可设置容量，范围为 `1..256`。

## GCC x64

```text
gcc -O3 -std=c11 -Wall -Wextra -Werror -m64 \
	-DXRT_FEATURE_FIXED_STACK -I include \
	dev\bench\fixed_stack\bench_fixed_stack.c \
	src\core\core.c src\memory\allocator.c src\memory\heap.c \
	src\core\error.c src\containers\fixed_stack.c \
	-o out\bench_fixed_stack.exe
out\bench_fixed_stack.exe 100000 64
```

## TinyCC x86

```text
tcc -O2 -Wall -m32 \
	-DXRT_FEATURE_FIXED_STACK -I include \
	dev\bench\fixed_stack\bench_fixed_stack.c \
	src\core\core.c src\memory\allocator.c src\memory\heap.c \
	src\core\error.c src\containers\fixed_stack.c \
	-o out\bench_fixed_stack_tcc.exe
out\bench_fixed_stack_tcc.exe 100000 64
```

当前环境基线见 `FIXED_STACK_BENCH_20260728.md`。
