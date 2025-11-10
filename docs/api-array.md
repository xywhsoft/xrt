# Array 结构体数组库

> 动态结构体数组

[返回索引](README.md)

---

## 📑 目录

- [数组操作](#数组操作)
- [元素访问](#元素访问)

---

## 数组操作

### xrtArrayCreate

创建结构体数组

**函数原型：**
```c
XXAPI xarray xrtArrayCreate(uint32 iItemLength);
```

**参数：**
- `iItemSize` - 每个元素的大小

**返回值：**
- 数组对象

**释放：** ✅ 需要 `xaFree` 释放

**示例：**
```c
typedef struct {
    int id;
    str name;
} Student;

xarray arr = xrtArrayCreate(sizeof(Student));
```

---

### xrtArrayDestroy

释放数组

**函数原型：**
```c
XXAPI void xrtArrayDestroy(xarray pArr);
```

---

### xrtArrayAppend

添加元素

**函数原型：**
```c
XXAPI uint32 xrtArrayAppend(xarray pArr, uint32 iCount);
```

**参数：**
- `pArr` - 数组对象
- `iCount` - 要添加的元素数量

**返回值：**
- 第一个新元素的位置索引

**示例：**
```c
uint32 pos = xrtArrayAppend(arr, 1);
Student* s = (Student*)xrtArrayGet(arr, pos);
s->id = 1;
s->name = "Tom";
```

---

### xrtArrayInsert

插入元素

**函数原型：**
```c
XXAPI uint32 xrtArrayInsert(xarray pArr, uint32 iPos, uint32 iCount);
```

---

### xrtArrayRemove

移除元素

**函数原型：**
```c
XXAPI bool xrtArrayRemove(xarray pArr, uint32 iPos, uint32 iCount);
```

---

## 元素访问

### xrtArrayGet

获取元素

**函数原型：**
```c
XXAPI ptr xrtArrayGet(xarray pArr, uint32 iPos);
```

**示例：**
```c
Student* s = (Student*)xrtArrayGet(arr, 1);  // 索引从1开始
printf("%s\n", s->name);
```

---

### xrtArrayCount

获取元素数量

**函数原型：**
```c
// 直接访问结构体字段
uint32 count = arr->Count;
```

---

## 使用场景

### 1. 数据集合

```c
xarray students = xrtArrayCreate(sizeof(Student));

for (int i = 0; i < 100; i++) {
    uint32 pos = xrtArrayAppend(students, 1);
    Student* s = (Student*)xrtArrayGet(students, pos);
    s->id = i;
    s->name = xrtFormat("Student%d", i);
}

uint count = students->Count;
for (uint i = 1; i <= count; i++) {
    Student* s = (Student*)xrtArrayGet(students, i);
    printf("%d: %s\n", s->id, s->name);
}

xrtArrayDestroy(students);
```

---

## 相关文档

- [PtrArray 指针数组](api-ptrarray.md)
- [List 列表](api-list.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#array-结构体数组库)

</div>
