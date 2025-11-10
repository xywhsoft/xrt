# File 文件操作库

> 文件读写、目录管理功能

[返回索引](README.md)

---

## 📑 目录

- [文件对象](#文件对象)
- [文件读写](#文件读写)
- [文件信息](#文件信息)
- [目录操作](#目录操作)

---

## 文件对象

### xrtOpen

打开文件

**函数原型：**
```c
XXAPI xfile xrtOpen(str sPath, int bReadOnly, int iCharset);
```

**参数：**
- `sPath` - 文件路径
- `bReadOnly` - 是否只读
  - `TRUE` - 只读
  - `FALSE` - 读写
- `iCharset` - 字符集（见 Charset 库）

**返回值：**
- 成功：文件对象指针
- 失败：`NULL`

**示例：**
```c
xfile f = xrtOpen("test.txt", FALSE, XRT_CP_UTF8);
if (f) {
    // 使用文件
    xrtClose(f);
}
```

---

### xrtClose

关闭文件

**函数原型：**
```c
XXAPI void xrtClose(xfile objFile);
```

---

### xrtRead

读取数据

**函数原型：**
```c
XXAPI size_t xrtRead(xfile objFile, ptr pBuffer, size_t iSize);
```

**返回值：**
- 实际读取的字节数

---

### xrtReadLine

读取一行

**函数原型：**
```c
XXAPI str xrtReadLine(xfile objFile);
```

**返回值：**
- 行内容（自动转换为 UTF-8）

**内存释放：** ✅ 需要 `xrtFree` 释放

---

### xrtWrite

写入数据

**函数原型：**
```c
XXAPI size_t xrtWrite(xfile objFile, ptr pBuffer, size_t iSize);
```

---

### xrtWriteLine

写入一行

**函数原型：**
```c
XXAPI bool xrtWriteLine(xfile objFile, str sText, size_t iSize);
```

---

## 文件读写

### xrtFileReadAll

读取整个文件

**函数原型：**
```c
XXAPI str xrtFileReadAll(str sPath, int iCharset);
```

**参数：**
- `sPath` - 文件路径
- `iCharset` - 字符集（`XRT_CP_AUTO` 自动检测）

**返回值：**
- 文件内容（UTF-8）
- 文件大小存储在 `xCore.iRet`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
str content = xrtFileReadAll("config.ini", XRT_CP_AUTO);
if (content) {
    printf("%s\n", content);
    xrtFree(content);
}
```

---

### xrtFileWriteAll

写入整个文件

**函数原型：**
```c
XXAPI bool xrtFileWriteAll(str sPath, str sText, size_t iSize, int iCharset);
```

**参数：**
- `sPath` - 文件路径
- `sText` - 内容
- `iSize` - 长度（0自动）
- `iCharset` - 字符集

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败

**示例：**
```c
str data = "Hello World";
bool ok = xrtFileWriteAll("output.txt", data, 0, XRT_CP_UTF8);
```

---

### xrtFileGetAll

读取文件为二进制

**函数原型：**
```c
XXAPI ptr xrtFileGetAll(str sPath);
```

**返回值：**
- 二进制数据
- 大小存储在 `xCore.iRet`

**内存释放：** ✅ 需要 `xrtFree` 释放

---

### xrtFilePutAll

写入二进制文件

**函数原型：**
```c
XXAPI bool xrtFilePutAll(str sPath, ptr pMem, size_t iSize);
```

---

## 文件信息

### xrtFileExists

判断文件是否存在

**函数原型：**
```c
XXAPI bool xrtFileExists(str sPath);
```

**示例：**
```c
if (xrtFileExists("config.ini")) {
    // 文件存在
}
```

---

### xrtGetSize

获取文件大小

**函数原型：**
```c
XXAPI int64 xrtGetSize(xfile objFile);
```

---

## 目录操作

### xrtListDir

列出目录内容

**函数原型：**
```c
XXAPI str* xrtListDir(str sPath, size_t iSize);
```

**返回值：**
- 文件名数组（以 `NULL` 结尾）

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
str* files = xrtListDir("C:\\folder", 0);
if (files) {
    for (int i = 0; files[i]; i++) {
        printf("%s\n", files[i]);
    }
    xrtFree(files);
}
```

---

## 使用场景

### 1. 配置文件读取

```c
str LoadConfig(str path) {
    if (!xrtFileExists(path)) {
        return NULL;
    }
    return xrtFileReadAll(path, XRT_CP_UTF8);
}
```

---

### 2. 日志写入

```c
void AppendLog(str message) {
    xfile f = xrtOpen("app.log", FALSE, XRT_CP_UTF8);
    if (f) {
        xrtWriteLine(f, message, 0);
        xrtClose(f);
    }
}
```

---

## 相关文档

- [Path 路径处理](api-path.md)
- [Charset 字符集转换](api-charset.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#file-文件操作库)

</div>
