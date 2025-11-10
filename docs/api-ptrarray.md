# PtrArray 指针数组库

> 动态指针数组

[返回索引](README.md)

---

## 📑 目录

- [数组操作](#数组操作)
- [元素访问](#元素访问)

---

## 数组操作

### xpaCreate

创建指针数组

**函数原型：**
```c
XXAPI xparray xpaCreate();
```

**返回值：**
- 指针数组对象

**释放：** ✅ 需要 `xpaFree` 释放

**示例：**
```c
xparray arr = xpaCreate();
xpaAdd(arr, ptr1);
xpaAdd(arr, ptr2);
xpaFree(arr);
```

---

### xpaFree

释放指针数组

**函数原型：**
```c
XXAPI void xpaFree(xparray objArray);
```

---

### xpaAdd

添加元素

**函数原型：**
```c
XXAPI void xpaAdd(xparray objArray, ptr pItem);
```

**示例：**
```c
xparray arr = xpaCreate();
xpaAdd(arr, "item1");
xpaAdd(arr, "item2");
```

---

### xpaRemove

移除元素

**函数原型：**
```c
XXAPI void xpaRemove(xparray objArray, uint iIndex);
```

---

## 元素访问

### xpaGet

获取元素

**函数原型：**
```c
XXAPI ptr xpaGet(xparray objArray, uint iIndex);
```

**示例：**
```c
ptr item = xpaGet(arr, 0);
```

---

### xpaSet

设置元素

**函数原型：**
```c
XXAPI void xpaSet(xparray objArray, uint iIndex, ptr pItem);
```

---

### xpaCount

获取元素数量

**函数原型：**
```c
XXAPI uint xpaCount(xparray objArray);
```

**示例：**
```c
uint count = xpaCount(arr);
for (uint i = 0; i < count; i++) {
    ptr item = xpaGet(arr, i);
    // 处理item
}
```

---

## 使用场景

### 1. 对象集合

```c
xparray objects = xpaCreate();

for (int i = 0; i < 10; i++) {
    MyObject* obj = CreateObject();
    xpaAdd(objects, obj);
}

// 遍历
uint count = xpaCount(objects);
for (uint i = 0; i < count; i++) {
    MyObject* obj = xpaGet(objects, i);
    ProcessObject(obj);
}

xpaFree(objects);
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
