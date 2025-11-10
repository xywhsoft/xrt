# Stack/DynStack 栈库

> 固定和动态栈数据结构

[返回索引](README.md)

---

## 📑 目录

- [Stack 静态栈](#stack-静态栈)
- [DynStack 动态栈](#dynstack-动态栈)

---

## Stack 静态栈

### xrtStackCreate

创建固定深度栈

**函数原型：**
```c
XXAPI xstack xrtStackCreate(uint32 iMaxCount, uint32 iItemLength);
```

**参数：**
- `iMaxCount` - 最大深度
- `iItemLength` - 每个元素的字节数（0表示指针栈）

**释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
// 指针栈
xstack stk = xrtStackCreate(100, 0);
xrtStackPushPtr(stk, ptr1);
ptr item = xrtStackPopPtr(stk);
xrtFree(stk);

// 结构体栈
xstack stk2 = xrtStackCreate(100, sizeof(MyStruct));
MyStruct* s = (MyStruct*)xrtStackPush(stk2);
s->value = 42;
xrtFree(stk2);
```

---

### xrtStackPush / xrtStackPushPtr

入栈

**函数原型：**
```c
XXAPI ptr xrtStackPush(xstack objSTK);  // 结构体栈，返回新元素指针
XXAPI uint32 xrtStackPushPtr(xstack objSTK, ptr pVal);  // 指针栈
```

**返回值：**
- xrtStackPush: 返回新元素的内存地址
- xrtStackPushPtr: 返回新元素的索引

---

### xrtStackPop / xrtStackPopPtr

出栈

**函数原型：**
```c
XXAPI ptr xrtStackPop(xstack objSTK);  // 结构体栈
XXAPI ptr xrtStackPopPtr(xstack objSTK);  // 指针栈
```

**返回值：**
- 栈顶元素
- `NULL` - 栈空

---

## DynStack 动态栈

### xrtDynStackCreate

创建动态栈

**函数原型：**
```c
XXAPI xdynstack xrtDynStackCreate(uint32 iItemLength);
```

**参数：**
- `iItemLength` - 每个元素的字节数（0表示指针栈）

**释放：** ✅ 需要 `xrtDynStackDestroy` 释放

**说明：**
- 无深度限制
- 自动扩展

**示例：**
```c
xdynstack ds = xrtDynStackCreate(0);  // 指针栈
xrtDynStackPushPtr(ds, ptr1);
xrtDynStackPushPtr(ds, ptr2);
ptr item = xrtDynStackPopPtr(ds);
xrtDynStackDestroy(ds);
```

---

### xrtDynStackPush / xrtDynStackPushPtr

入栈

**函数原型：**
```c
XXAPI ptr xrtDynStackPush(xdynstack objSTK);  // 结构体栈
XXAPI uint32 xrtDynStackPushPtr(xdynstack objSTK, ptr pVal);  // 指针栈
```

---

### xrtDynStackPop / xrtDynStackPopPtr

出栈

**函数原型：**
```c
XXAPI ptr xrtDynStackPop(xdynstack objSTK);  // 结构体栈
XXAPI ptr xrtDynStackPopPtr(xdynstack objSTK);  // 指针栈
```

---

### xrtDynStackCount

获取栈深度

**访问方式：**
```c
uint32 count = ds->Count;  // 直接访问结构体字段
```

---

## 使用场景

### 1. 递归模拟

```c
xdynstack call_stack = xrtDynStackCreate(0);

void ProcessRecursive(int level) {
    xrtDynStackPushPtr(call_stack, (ptr)(intptr)level);
    
    if (level > 0) {
        ProcessRecursive(level - 1);
    }
    
    xrtDynStackPopPtr(call_stack);
}
```

---

### 2. 撤销/重做

```c
xdynstack undo_stack = xrtDynStackCreate(0);
xdynstack redo_stack = xrtDynStackCreate(0);

void DoAction(Action* action) {
    xrtDynStackPushPtr(undo_stack, action);
}

void Undo() {
    Action* action = xrtDynStackPopPtr(undo_stack);
    if (action) {
        xrtDynStackPushPtr(redo_stack, action);
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
