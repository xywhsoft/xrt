# PtrArray 指针数组库

> 动态指针数组

[返回索引](README.md)

---

## 📑 目录

- [数组操作](#数组操作)
- [元素访问](#元素访问)

---

## 数组操作

### xrtPtrArrayCreate

创建指针数组

**函数原型：**
```c
XXAPI xparray xrtPtrArrayCreate();
```

**返回值：**
- 指针数组对象

**释放：** ✅ 需要 `xpaFree` 释放

**示例：**
```c
xparray arr = xrtPtrArrayCreate();
xrtPtrArrayAppend(arr, ptr1);
xrtPtrArrayAppend(arr, ptr2);
xrtPtrArrayDestroy(arr);
```

---

### xrtPtrArrayDestroy

释放指针数组

**函数原型：**
```c
XXAPI void xrtPtrArrayDestroy(xparray pObject);
```

---

### xrtPtrArrayAppend

添加元素

**函数原型：**
```c
XXAPI uint32 xrtPtrArrayAppend(xparray pObject, ptr pVal);
```

**示例：**
```c
xparray arr = xrtPtrArrayCreate();
xrtPtrArrayAppend(arr, "item1");
xrtPtrArrayAppend(arr, "item2");
```

---

### xrtPtrArrayRemove

移除元素

**函数原型：**
```c
XXAPI bool xrtPtrArrayRemove(xparray pObject, uint32 iPos, uint32 iCount);
```

---

## 元素访问

### xrtPtrArrayGet

获取元素

**函数原型：**
```c
XXAPI ptr xrtPtrArrayGet(xparray pObject, uint32 iPos);
```

**示例：**
```c
ptr item = xrtPtrArrayGet(arr, 1);  // 注意：索引从1开始
```

---

### xrtPtrArraySet

设置元素

**函数原型：**
```c
XXAPI bool xrtPtrArraySet(xparray pObject, uint32 iPos, ptr pVal);
```

---

### xrtPtrArrayCount

获取元素数量

**函数原型：**
```c
// 直接访问结构体字段
uint32 count = arr->Count;
```

**示例：**
```c
uint count = arr->Count;
for (uint i = 1; i <= count; i++) {  // 索引从1开始
    ptr item = xrtPtrArrayGet(arr, i);
    // 处理item
}
```

---

## 使用场景

### 1. 对象集合

```c
xparray objects = xrtPtrArrayCreate();

for (int i = 0; i < 10; i++) {
    MyObject* obj = CreateObject();
    xrtPtrArrayAppend(objects, obj);
}

// 遍历
uint count = objects->Count;
for (uint i = 1; i <= count; i++) {
    MyObject* obj = xrtPtrArrayGet(objects, i);
    ProcessObject(obj);
}

xrtPtrArrayDestroy(objects);
```

---

## 相关文档

- [Array 结构体数组](api-array.md)
- [List 列表](api-list.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#ptrarray-指针数组库)

</div>
