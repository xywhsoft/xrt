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

## 批量合同

`xrtTypedStackPushBatch(stack, items)` 事务复制同类型 `xtypedarray`，顺序等同逐次
`Push`，最后一项成为栈顶。允许 `items == stack`；输入活动区的初始内容只追加一次。
类型不匹配、计数溢出、分配或元素复制失败不发布部分元素；已有值/数量不变，
为事务预留的容量可以保留。空来源成功且不修改任何值。

`xrtTypedStackPopBatch(stack, maxCount)` 返回独立拥有的 `xtypedarray*`，数量是
`min(maxCount, depth)`，按连续 `Pop` 顺序（顶到底）。零上限和空栈仍返回空拥有数组；
返回 `NULL` 才是错误。先分配/预留结果，再按 `RELOCATABLE` 合同移交字节与生命周期，
不调用元素 `Init/Move/Copy`、不逐项复制字符串。失败保持原存储地址、容量、深度和值。
交付后来源活动区不再拥有被移交元素；结果用 `xrtTypedArrayDestroy` 销毁。

`xrtTypedStackPeekBatch(stack, depth, maxCount)` 从指定深度向下复制最多指定数量，
顺序同 `PopBatch`，不修改来源。起点等于栈深合法（空结果），超过栈深报范围错误。
复制失败销毁全部部分结果，保留原错误及来源；NUL、类型宽度与过对齐均按元素描述处理。

批次的分配器、类型复制和回滚析构期间，来源禁止全部 API 重入；压入时目标也被保护。
这不是线程安全容器，调用者仍须保证类型描述和容器寿命，并外部同步跨线程访问。
只启用数组不会拉入栈包装；新增入口不引入第二套数组或盒装元素存储。

## 历史资产

本模块保留旧版 `lib/typed_special.h` 中类型栈的运行时类型所有权、定宽整数与浮点宽度回归，
同时移除旧版独立引用计数、重复元素长度字段以及重复复制/销毁实现。底层数组契约测试继续覆盖
自引用压入、过对齐、回调重入、复杂生命周期和确定性 OOM。
