# Path 路径处理库

> 文件路径操作和解析功能

[返回索引](README.md)

---

## 📑 目录

- [路径信息](#路径信息)
- [路径操作](#路径操作)
- [路径拼接](#路径拼接)

---

## 路径信息

### xrtPathGetDir

获取路径的目录部分

**函数原型：**
```c
XXAPI str xrtPathGetDir(str sPath, size_t iSize);
```

**参数：**
- `sPath` - 文件路径
- `iSize` - 长度（0自动）

**返回值：**
- 目录路径（不含文件名）
- 长度存储在 `xCore.iRet`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
str dir = xrtPathGetDir("C:\\folder\\file.txt", 0);
printf("%s\n", dir);  // "C:\\folder"
xrtFree(dir);
```

---

### xrtPathGetFile

获取文件名（含扩展名）

**函数原型：**
```c
XXAPI str xrtPathGetFile(str sPath, size_t iSize);
```

**返回值：**
- 文件名字符串

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
str file = xrtPathGetFile("C:\\folder\\file.txt", 0);
printf("%s\n", file);  // "file.txt"
xrtFree(file);
```

---

### xrtPathGetName

获取文件名（不含扩展名）

**函数原型：**
```c
XXAPI str xrtPathGetName(str sPath, size_t iSize);
```

**示例：**
```c
str name = xrtPathGetName("C:\\folder\\file.txt", 0);
printf("%s\n", name);  // "file"
xrtFree(name);
```

---

### xrtPathGetExt

获取扩展名

**函数原型：**
```c
XXAPI str xrtPathGetExt(str sPath, size_t iSize);
```

**返回值：**
- 扩展名（含 `.`）

**示例：**
```c
str ext = xrtPathGetExt("file.txt", 0);
printf("%s\n", ext);  // ".txt"
xrtFree(ext);
```

---

## 路径操作

### xrtPathNorm

规范化路径

**函数原型：**
```c
XXAPI str xrtPathNorm(str sPath, size_t iSize, bool bSrcRevise);
```

**参数：**
- `sPath` - 路径
- `bSrcRevise` - 是否修改源

**返回值：**
- 规范化后的路径

**内存释放：**
- `bSrcRevise=FALSE` - ✅ 需要释放

**功能：**
- 将 `/` 转为平台分隔符
- 移除重复的分隔符
- 处理 `.` 和 `..`

**示例：**
```c
str norm = xrtPathNorm("C:\\folder\\..\\file.txt", 0, FALSE);
printf("%s\n", norm);  // "C:\\file.txt"
xrtFree(norm);
```

---

## 路径拼接

### xrtPathJoin

拼接多个路径

**函数原型：**
```c
XXAPI str xrtPathJoin(int iCount, ...);
```

**参数：**
- `iCount` - 路径段数量
- `...` - 路径段（可变参数）

**返回值：**
- 拼接后的完整路径

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
str path = xrtPathJoin(3, "C:\\", "folder", "file.txt");
printf("%s\n", path);  // "C:\\folder\\file.txt"
xrtFree(path);

// Linux
str linux_path = xrtPathJoin(2, "/home/user", "document.pdf");
xrtFree(linux_path);
```

---

## 使用场景

### 1. 路径解析

```c
void ParsePath(str path) {
    str dir = xrtPathGetDir(path, 0);
    str name = xrtPathGetName(path, 0);
    str ext = xrtPathGetExt(path, 0);
    
    printf("Dir: %s\n", dir);
    printf("Name: %s\n", name);
    printf("Ext: %s\n", ext);
    
    xrtFree(dir);
    xrtFree(name);
    xrtFree(ext);
}
```

---

### 2. 路径构建

```c
str BuildConfigPath() {
    return xrtPathJoin(3, xCore.AppPath, "config", "app.ini");
}
```

---

## 相关文档

- [File 文件操作](api-file.md)
- [String 字符串处理](api-string.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#path-路径处理库)

</div>
