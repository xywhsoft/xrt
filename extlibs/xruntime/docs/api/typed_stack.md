# 类型栈

`typed_stack` 是 `typed_array` 上的后进先出语义层。它直接复用类型数组已经压实的连续存储、
过对齐、元素生命周期、别名处理和 OOM 失败原子性，不维护第二套容器实现。

```c
#include <xrt/typed_stack.h>
```

## 裁剪

启用 `XRUNTIME_FEATURE_TYPED_STACK` 会依赖 `XRUNTIME_FEATURE_TYPED_ARRAY`。不启用类型栈时，不生成任何
栈包装符号；类型数组也不反向依赖本模块。

## 生命周期

`xrtTypedStackInit` 初始化调用方提供的结构，使用 `xrtTypedStackUnit` 结束；
`xrtTypedStackCreate` 在堆上创建结构，使用 `xrtTypedStackDestroy` 销毁。元素类型描述只被借用，
必须覆盖栈的完整生命周期。

栈复制拥有每一个压入值。`Push` 执行类型复制；`Clear`、丢弃式 `Pop`、`Unit` 和 `Destroy`
执行类型销毁。所有权和错误规则与 `typed_array` 完全一致。

## 栈操作

- `xrtTypedStackPush` 复制压入一个值，失败时栈保持原值。
- `xrtTypedStackPop(stack, output)` 把栈顶移动到已经初始化的输出值后删除。
- `xrtTypedStackPop(stack, NULL)` 销毁并删除栈顶。
- `Peek(stack, depth)` 按距栈顶深度返回借用值；深度零等价于 `Top`。
- `Clone` 深复制完整栈；`Equals` 比较精确元素类型、深度和从栈底到栈顶的顺序。

空栈 `Pop` 和越界 `Peek` 是正常未命中，分别返回 `false` 和空指针。任何结构修改都可能使
先前借用的元素地址失效。

```c
xtypedstack Stack;
int64 Input = 42;
int64 Output = 0;

if ( !xrtTypedStackInit(&Stack, xrtTypeInt64()) ||
	 !xrtTypedStackPush(&Stack, &Input) ||
	 !xrtTypedStackPop(&Stack, &Output) ) {
	return false;
}
xrtTypedStackUnit(&Stack);
```

## 历史资产

本模块保留旧版 `lib/typed_special.h` 中类型栈的运行时类型所有权、定宽整数与浮点宽度回归，
同时移除旧版独立引用计数、重复元素长度字段以及重复复制/销毁实现。底层数组契约测试继续覆盖
自引用压入、过对齐、回调重入、复杂生命周期和确定性 OOM。
