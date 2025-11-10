# OS 操作系统库

> 进程管理和系统调用功能

[返回索引](README.md)

---

## 📑 目录

- [进程执行](#进程执行)
- [使用场景](#使用场景)
- [平台差异](#平台差异)

---

## 进程执行

### xrtRun

运行外部程序（异步）

**函数原型：**
```c
XXAPI ptr xrtRun(str sPath, size_t iSize);
```

**参数：**
- `sPath` - 程序路径或命令行
- `iSize` - 路径长度（0表示自动计算）

**返回值：**
- Windows: 返回进程句柄 (`HANDLE`)
- Linux: 返回进程ID (`pid_t`) 转换为指针

**说明：**
- 异步执行，不等待程序结束
- 立即返回进程句柄/ID
- 可用于启动后台进程

**示例：**
```c
// 启动记事本（Windows）
ptr handle = xrtRun("notepad.exe", 0);
if (handle) {
    printf("Process started\n");
    // 进程在后台运行
}

// 启动带参数的程序
ptr handle = xrtRun("ffmpeg -i input.mp4 output.avi", 0);

// Linux: 启动程序
ptr pid = xrtRun("/usr/bin/gedit", 0);
```

---

### xrtStart

打开文件或URL（仅 Windows）

**函数原型：**
```c
XXAPI ptr xrtStart(str sPath, size_t iSize);
```

**参数：**
- `sPath` - 文件路径或URL
- `iSize` - 路径长度（0表示自动计算）

**返回值：**
- 成功：返回关联程序的进程句柄
- 失败：返回 `NULL`

**平台支持：** 仅 Windows

**说明：**
- 使用系统默认程序打开文件
- 可以打开 URL（使用默认浏览器）
- 可以打开文档（使用关联程序）
- 类似于双击文件的效果

**示例：**
```c
#ifdef _WIN32
    // 用默认程序打开文本文件
    xrtStart("C:\\test.txt", 0);
    
    // 用默认浏览器打开网页
    xrtStart("https://www.example.com", 0);
    
    // 打开图片
    xrtStart("C:\\photo.jpg", 0);
    
    // 打开文件夹
    xrtStart("C:\\MyFolder", 0);
#endif
```

---

### xrtChain

运行程序并等待结束（同步）

**函数原型：**
```c
XXAPI int xrtChain(str sPath, size_t iSize);
```

**参数：**
- `sPath` - 程序路径或命令行
- `iSize` - 路径长度（0表示自动计算）

**返回值：**
- 程序的退出码
- Windows: 程序的返回值
- Linux: 程序的退出状态

**说明：**
- **同步执行**，会阻塞当前线程
- 等待程序执行完毕后返回
- 返回程序的退出码

**示例：**
```c
// 执行命令并等待完成
int exit_code = xrtChain("dir C:\\", 0);  // Windows
printf("Exit code: %d\n", exit_code);

// Linux: 执行 shell 命令
int result = xrtChain("ls -l /home", 0);

// 执行批处理/脚本并获取结果
int code = xrtChain("backup.bat", 0);
if (code == 0) {
    printf("Backup successful\n");
} else {
    printf("Backup failed with code: %d\n", code);
}
```

---

## 使用场景

### 1. 启动外部工具

```c
// 启动文本编辑器
void OpenEditor(str filename) {
    #ifdef _WIN32
        str cmd = xrtFormat("notepad.exe %s", filename);
        xrtRun(cmd, 0);
        xrtFree(cmd);
    #else
        str cmd = xrtFormat("gedit %s &", filename);
        system(cmd);
        xrtFree(cmd);
    #endif
}
```

---

### 2. 执行系统命令

```c
// 清理临时文件
void CleanupTemp() {
    #ifdef _WIN32
        int result = xrtChain("del /Q C:\\Temp\\*.*", 0);
    #else
        int result = xrtChain("rm -f /tmp/*", 0);
    #endif
    
    if (result == 0) {
        printf("Cleanup completed\n");
    }
}
```

---

### 3. 打开URL或文件

```c
// 在浏览器中打开帮助页面
void OpenHelp() {
    #ifdef _WIN32
        xrtStart("https://docs.example.com/help", 0);
    #else
        xrtRun("xdg-open https://docs.example.com/help", 0);
    #endif
}

// 用默认程序查看文档
void ViewDocument(str filepath) {
    #ifdef _WIN32
        xrtStart(filepath, 0);
    #else
        str cmd = xrtFormat("xdg-open \"%s\"", filepath);
        xrtRun(cmd, 0);
        xrtFree(cmd);
    #endif
}
```

---

### 4. 批量处理

```c
// 使用外部工具批量转换文件
void ConvertFiles(str* files, int count) {
    for (int i = 0; i < count; i++) {
        str cmd = xrtFormat(
            "ffmpeg -i \"%s\" \"%s.mp4\"",
            files[i], files[i]
        );
        
        // 等待每个转换完成
        int code = xrtChain(cmd, 0);
        if (code == 0) {
            printf("Converted: %s\n", files[i]);
        } else {
            printf("Failed: %s (code=%d)\n", files[i], code);
        }
        
        xrtFree(cmd);
    }
}
```

---

### 5. 后台服务启动

```c
// 启动后台服务
void StartService() {
    #ifdef _WIN32
        ptr handle = xrtRun("service.exe -daemon", 0);
        if (handle) {
            printf("Service started\n");
            // 可以保存 handle 用于后续操作
        }
    #else
        pid_t pid = (pid_t)(intptr_t)xrtRun("./service -d", 0);
        printf("Service PID: %d\n", pid);
    #endif
}
```

---

## 平台差异

### Windows vs Linux

| 功能 | Windows | Linux |
|------|---------|-------|
| `xrtRun` | 返回 `HANDLE` | 返回 `pid_t` |
| `xrtStart` | ✅ 支持 | ❌ 不支持 |
| `xrtChain` | 返回退出码 | 返回退出状态 |

### Windows 特有功能

```c
#ifdef _WIN32
    // ShellExecute 封装
    xrtStart("mailto:user@example.com", 0);  // 打开邮件客户端
    xrtStart("file:///C:/folder", 0);         // 在资源管理器中打开
#endif
```

### Linux 替代方案

```c
#ifndef _WIN32
    // xrtStart 的替代方案
    void LinuxStart(str path) {
        str cmd = xrtFormat("xdg-open \"%s\" &", path);
        xrtRun(cmd, 0);
        xrtFree(cmd);
    }
#endif
```

---

## 最佳实践

### 1. 跨平台处理

```c
// 定义跨平台的打开函数
void OpenFile(str path) {
    #ifdef _WIN32
        xrtStart(path, 0);
    #else
        str cmd = xrtFormat("xdg-open \"%s\"", path);
        xrtRun(cmd, 0);
        xrtFree(cmd);
    #endif
}
```

---

### 2. 命令参数转义

```c
// 处理含空格的路径
str EscapePath(str path) {
    #ifdef _WIN32
        return xrtFormat("\"%s\"", path);
    #else
        // Linux 需要更复杂的转义
        // 简化版本
        return xrtFormat("'%s'", path);
    #endif
}

void RunWithFile(str program, str file) {
    str escaped = EscapePath(file);
    str cmd = xrtFormat("%s %s", program, escaped);
    xrtRun(cmd, 0);
    xrtFree(cmd);
    xrtFree(escaped);
}
```

---

### 3. 检查执行结果

```c
// 同步执行并检查结果
bool ExecuteCommand(str command) {
    int code = xrtChain(command, 0);
    
    #ifdef _WIN32
        return (code == 0);
    #else
        // Linux: 检查退出状态
        return WIFEXITED(code) && (WEXITSTATUS(code) == 0);
    #endif
}
```

---

### 4. 异步执行管理

```c
// 保存进程句柄以便后续管理
typedef struct {
    ptr handle;
    str name;
} ProcessInfo;

ProcessInfo* StartProcess(str command, str name) {
    ptr handle = xrtRun(command, 0);
    if (!handle) {
        return NULL;
    }
    
    ProcessInfo* info = xrtMalloc(sizeof(ProcessInfo));
    info->handle = handle;
    info->name = xrtCopyStr(name, 0);
    return info;
}

void WaitProcess(ProcessInfo* info) {
    #ifdef _WIN32
        WaitForSingleObject((HANDLE)info->handle, INFINITE);
        CloseHandle((HANDLE)info->handle);
    #else
        pid_t pid = (pid_t)(intptr_t)info->handle;
        waitpid(pid, NULL, 0);
    #endif
    
    xrtFree(info->name);
    xrtFree(info);
}
```

---

## 安全注意事项

### 1. 命令注入防护

```c
// ❌ 危险：直接拼接用户输入
void UnsafeRun(str user_input) {
    str cmd = xrtFormat("process %s", user_input);
    xrtRun(cmd, 0);  // 用户可输入: "; rm -rf /"
    xrtFree(cmd);
}

// ✅ 安全：验证和转义
void SafeRun(str user_input) {
    // 1. 验证输入
    if (!IsValidFilename(user_input)) {
        return;
    }
    
    // 2. 转义特殊字符
    str escaped = EscapeShellArg(user_input);
    str cmd = xrtFormat("process %s", escaped);
    xrtRun(cmd, 0);
    xrtFree(cmd);
    xrtFree(escaped);
}
```

---

### 2. 路径验证

```c
// 验证可执行文件路径
bool IsValidExecutable(str path) {
    // 检查文件存在
    if (!xrtFileExists(path)) {
        return false;
    }
    
    // 检查扩展名（Windows）
    #ifdef _WIN32
        str ext = xrtPathGetExt(path, 0);
        bool valid = (
            xrtStrComp(ext, ".exe", 0, FALSE) == 0 ||
            xrtStrComp(ext, ".bat", 0, FALSE) == 0 ||
            xrtStrComp(ext, ".cmd", 0, FALSE) == 0
        );
        xrtFree(ext);
        return valid;
    #else
        // Linux: 检查可执行权限
        return (access(path, X_OK) == 0);
    #endif
}
```

---

## 常见问题

### 1. 程序路径问题

```c
// ❌ 相对路径可能找不到
xrtRun("tool.exe", 0);  // 可能失败

// ✅ 使用绝对路径
str full_path = xrtPathJoin(2, xCore.AppPath, "tool.exe");
xrtRun(full_path, 0);
xrtFree(full_path);
```

---

### 2. 工作目录

```c
// 子进程的工作目录是继承的
// 如果需要特定工作目录，在命令中指定
str cmd = xrtFormat("cd /path && ./program", 0);
xrtRun(cmd, 0);
xrtFree(cmd);
```

---

### 3. 输出重定向

```c
// 将输出重定向到文件
str cmd = xrtFormat("program > output.log 2>&1", 0);
int code = xrtChain(cmd, 0);
xrtFree(cmd);

// 读取输出
str output = xrtFileReadAll("output.log", XRT_CP_UTF8);
printf("Output: %s\n", output);
xrtFree(output);
```

---

## 相关文档

- [File 文件操作](api-file.md) - 文件系统操作
- [Path 路径处理](api-path.md) - 路径拼接和解析
- [Thread 线程管理](api-thread.md) - 多线程功能
- [主索引](README.md) - 返回文档首页

---

<div align="center">

[⬆️ 返回顶部](#os-操作系统库)

</div>
