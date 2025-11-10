# Array 结构体数组库

> 动态结构体数组

[返回索引](README.md)

---

## 📑 目录

- [数组操作](#数组操作)
- [元素访问](#元素访问)

---

## 数组操作

### xaCreate

创建结构体数组

**函数原型：**
```c
XXAPI xarray xaCreate(size_t iItemSize);
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

xarray arr = xaCreate(sizeof(Student));
```

---

### xaFree

释放数组

**函数原型：**
```c
XXAPI void xaFree(xarray objArray);
```

---

### xaAdd

添加元素

**函数原型：**
```c
XXAPI ptr xaAdd(xarray objArray);
```

**返回值：**
- 新元素的指针

**示例：**
```c
Student* s = (Student*)xaAdd(arr);
s->id = 1;
s->name = "Tom";
```

---

### xaInsert

插入元素

**函数原型：**
```c
XXAPI ptr xaInsert(xarray objArray, uint iIndex);
```

---

### xaRemove

移除元素

**函数原型：**
```c
XXAPI void xaRemove(xarray objArray, uint iIndex);
```

---

## 元素访问

### xaGet

获取元素

**函数原型：**
```c
XXAPI ptr xaGet(xarray objArray, uint iIndex);
```

**示例：**
```c
Student* s = (Student*)xaGet(arr, 0);
printf("%s\n", s->name);
```

---

### xaCount

获取元素数量

**函数原型：**
```c
XXAPI uint xaCount(xarray objArray);
```

---

## 使用场景

### 1. 数据集合

```c
xarray students = xaCreate(sizeof(Student));

for (int i = 0; i < 100; i++) {
    Student* s = (Student*)xaAdd(students);
    s->id = i;
    s->name = xrtFormat("Student%d", i);
}

uint count = xaCount(students);
for (uint i = 0; i < count; i++) {
    Student* s = (Student*)xaGet(students, i);
    printf("%d: %s\n", s->id, s->name);
}

xaFree(students);
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
