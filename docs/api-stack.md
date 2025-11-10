# Stack/DynStack 栈库

> 固定和动态栈数据结构

[返回索引](README.md)

---

## 📑 目录

- [Stack 静态栈](#stack-静态栈)
- [DynStack 动态栈](#dynstack-动态栈)

---

## Stack 静态栈

### xstkCreate

创建固定深度栈

**函数原型：**
```c
XXAPI xstack xstkCreate(uint iMaxDepth);
```

**参数：**
- `iMaxDepth` - 最大深度

**释放：** ✅ 需要 `xstkFree` 释放

**示例：**
```c
xstack stk = xstkCreate(100);
xstkPush(stk, ptr1);
ptr item = xstkPop(stk);
xstkFree(stk);
```

---

### xstkPush

入栈

**函数原型：**
```c
XXAPI bool xstkPush(xstack objStack, ptr pItem);
```

**返回值：**
- `TRUE` - 成功
- `FALSE` - 栈满

---

### xstkPop

出栈

**函数原型：**
```c
XXAPI ptr xstkPop(xstack objStack);
```

**返回值：**
- 栈顶元素
- `NULL` - 栈空

---

## DynStack 动态栈

### xdsCreate

创建动态栈

**函数原型：**
```c
XXAPI xdynstack xdsCreate();
```

**释放：** ✅ 需要 `xdsFree` 释放

**说明：**
- 无深度限制
- 自动扩展

**示例：**
```c
xdynstack ds = xdsCreate();
xdsPush(ds, ptr1);
xdsPush(ds, ptr2);
ptr item = xdsPop(ds);
xdsFree(ds);
```

---

### xdsPush

入栈

**函数原型：**
```c
XXAPI void xdsPush(xdynstack objStack, ptr pItem);
```

---

### xdsPop

出栈

**函数原型：**
```c
XXAPI ptr xdsPop(xdynstack objStack);
```

---

### xdsCount

获取栈深度

**函数原型：**
```c
XXAPI uint xdsCount(xdynstack objStack);
```

---

## 使用场景

### 1. 递归模拟

```c
xdynstack call_stack = xdsCreate();

void ProcessRecursive(int level) {
    xdsPush(call_stack, (ptr)(intptr)level);
    
    if (level > 0) {
        ProcessRecursive(level - 1);
    }
    
    xdsPop(call_stack);
}
```

---

### 2. 撤销/重做

```c
xdynstack undo_stack = xdsCreate();
xdynstack redo_stack = xdsCreate();

void DoAction(Action* action) {
    xdsPush(undo_stack, action);
}

void Undo() {
    Action* action = xdsPop(undo_stack);
    if (action) {
        xdsPush(redo_stack, action);
        ReverseAction(action);
    }
}
```

---

## 相关文档

- [LList 双向链表](api-llist.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#stackdynstack-栈库)

</div>
