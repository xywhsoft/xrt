# Thread 线程管理库

> 多线程创建和管理功能

[返回索引](README.md)

---

## 📑 目录

- [数据类型](#数据类型)
- [线程操作](#线程操作)
- [平台特定操作](#平台特定操作)
- [使用场景](#使用场景)
- [注意事项](#注意事项)

---

## 数据类型

### xthread_struct

线程数据结构。

**定义：**
```c
typedef struct {
    ptr Handle;                    // 线程句柄（Windows: HANDLE, Linux: pthread_t）
    uint32 TID;                    // 线程ID
    uint32 (*Proc)(ptr param);     // 线程函数指针
    ptr Param;                     // 线程参数
} xthread_struct, *xthread;
```

**成员说明：**
- `Handle` - 平台相关的线程句柄
- `TID` - 线程ID
- `Proc` - 线程执行函数
- `Param` - 传递给线程的参数

---

## 线程操作

### xrtThreadCreate

创建并启动新线程。

**函数原型：**
```c
XXAPI xthread xrtThreadCreate(ptr pProc, ptr pParam, size_t iStackSize);
```

**参数：**
- `pProc` - 线程函数指针
- `pParam` - 传递给线程的参数
- `iStackSize` - 栈大小（字节，0 使用系统默认值）

**返回值：**
- 成功：线程对象指针 `xthread`
- 失败：`NULL`

**线程函数原型：**
```c
// Windows
DWORD WINAPI ThreadFunc(LPVOID param);

// 或通用形式
uint32 ThreadFunc(ptr param);
```

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    DWORD WINAPI WorkerThread(LPVOID param) {
        int* value = (int*)param;
        printf("线程收到参数: %d\n", *value);
        xrtSleep(1000);  // 模拟工作
        printf("线程完成\n");
        return 0;
    }
#else
    #include <pthread.h>
    void* WorkerThread(void* param) {
        int* value = (int*)param;
        printf("线程收到参数: %d\n", *value);
        xrtSleep(1000);  // 模拟工作
        printf("线程完成\n");
        return NULL;
    }
#endif

int main() {
    xrtInit();
    
    int data = 42;
    xthread t = xrtThreadCreate(WorkerThread, &data, 0);
    if (t) {
        printf("线程创建成功，TID: %u\n", t->TID);
        
        // 等待线程完成（平台相关）
        #if defined(_WIN32) || defined(_WIN64)
            WaitForSingleObject(t->Handle, INFINITE);
            CloseHandle(t->Handle);
        #else
            pthread_join((pthread_t)t->Handle, NULL);
        #endif
        
        xrtFree(t);
    } else {
        printf("线程创建失败\n");
    }
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- Windows 使用 `CreateThread` 实现
- Linux 使用 `pthread_create` 实现（需链接 `-lpthread`）
- 返回的 `xthread` 需要手动释放（`xrtFree`）
- 线程句柄需要使用平台 API 等待和关闭

---

## 平台特定操作

### 线程等待

由于线程等待需要使用平台特定 API，以下是跨平台的等待方式：

```c
#include "xrt.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
#else
    #include <pthread.h>
#endif

void WaitThread(xthread t) {
    if (!t) return;
    
    #if defined(_WIN32) || defined(_WIN64)
        WaitForSingleObject(t->Handle, INFINITE);
        CloseHandle(t->Handle);
    #else
        pthread_join((pthread_t)t->Handle, NULL);
    #endif
    
    xrtFree(t);
}
```

### 线程休眠

使用 Time 库中的 `xrtSleep` 函数：

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    printf("开始休眠...\n");
    xrtSleep(2000);  // 休眠 2000 毫秒（2秒）
    printf("休眠结束\n");
    
    xrtUnit();
    return 0;
}
```

**注意：** `xrtSleep` 在 [Time 时间处理库](api-time.md) 中定义。

---

## 使用场景

### 1. 并行任务处理

```c
#include "xrt.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    DWORD WINAPI TaskWorker(LPVOID param) {
        int id = *(int*)param;
        printf("任务 %d 开始\n", id);
        xrtSleep(500 + xrtRandRange(0, 1000));  // 模拟不同时长的任务
        printf("任务 %d 完成\n", id);
        return 0;
    }
#else
    #include <pthread.h>
    void* TaskWorker(void* param) {
        int id = *(int*)param;
        printf("任务 %d 开始\n", id);
        xrtSleep(500 + xrtRandRange(0, 1000));
        printf("任务 %d 完成\n", id);
        return NULL;
    }
#endif

int main() {
    xrtInit();
    
    #define THREAD_COUNT 4
    xthread threads[THREAD_COUNT];
    int ids[THREAD_COUNT];
    
    // 创建多个线程
    for (int i = 0; i < THREAD_COUNT; i++) {
        ids[i] = i + 1;
        threads[i] = xrtThreadCreate(TaskWorker, &ids[i], 0);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < THREAD_COUNT; i++) {
        if (threads[i]) {
            #if defined(_WIN32) || defined(_WIN64)
                WaitForSingleObject(threads[i]->Handle, INFINITE);
                CloseHandle(threads[i]->Handle);
            #else
                pthread_join((pthread_t)threads[i]->Handle, NULL);
            #endif
            xrtFree(threads[i]);
        }
    }
    
    printf("所有任务完成\n");
    
    xrtUnit();
    return 0;
}
```

---

### 2. 后台任务

```c
#include "xrt.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
#else
    #include <pthread.h>
#endif

typedef struct {
    str filename;
    int running;
} BackgroundTask;

#if defined(_WIN32) || defined(_WIN64)
DWORD WINAPI BackgroundWorker(LPVOID param) {
#else
void* BackgroundWorker(void* param) {
#endif
    BackgroundTask* task = (BackgroundTask*)param;
    
    while (task->running) {
        printf("后台任务运行中: %s\n", task->filename);
        xrtSleep(1000);
    }
    
    printf("后台任务已停止\n");
    #if defined(_WIN32) || defined(_WIN64)
        return 0;
    #else
        return NULL;
    #endif
}

int main() {
    xrtInit();
    
    BackgroundTask task = { (str)"data.log", TRUE };
    xthread t = xrtThreadCreate(BackgroundWorker, &task, 0);
    
    if (t) {
        printf("后台任务已启动\n");
        
        // 主线程工作 3 秒
        xrtSleep(3000);
        
        // 停止后台任务
        task.running = FALSE;
        
        // 等待线程结束
        #if defined(_WIN32) || defined(_WIN64)
            WaitForSingleObject(t->Handle, INFINITE);
            CloseHandle(t->Handle);
        #else
            pthread_join((pthread_t)t->Handle, NULL);
        #endif
        xrtFree(t);
    }
    
    xrtUnit();
    return 0;
}
```

---

## 注意事项

### 1. 线程安全

XRT 库的大部分函数**不是线程安全**的，特别是：

- `xCore` 全局变量（`iRet`, `LastError` 等）
- 临时内存（`xrtTempMemory`）
- JSON 解析器

```c
// ❌ 不安全：多线程共享 xCore.iRet
void ThreadFunc1() {
    str s = xrtCopyStr((str)"hello", 0);
    size_t len = xCore.iRet;  // 可能被其他线程覆盖
}

// ✅ 安全：立即保存返回值
void ThreadFunc2() {
    str s = xrtCopyStr((str)"hello", 0);
    size_t len = xCore.iRet;  // 立即保存
    // 后续操作...
}
```

### 2. 内存管理

线程中分配的内存应在同一线程中释放，或使用适当的同步机制。

### 3. 资源释放

```c
// ✅ 正确的资源释放顺序
xthread t = xrtThreadCreate(func, param, 0);
if (t) {
    // 1. 等待线程完成
    #if defined(_WIN32) || defined(_WIN64)
        WaitForSingleObject(t->Handle, INFINITE);
        // 2. 关闭句柄
        CloseHandle(t->Handle);
    #else
        pthread_join((pthread_t)t->Handle, NULL);
    #endif
    // 3. 释放结构体
    xrtFree(t);
}
```

### 4. 编译链接

在 Linux/macOS 上需要链接 pthread 库：

```bash
gcc -o program program.c -lpthread
```

---

## 相关文档

- [Time 时间处理库](api-time.md) - 包含 `xrtSleep` 函数
- [Base 基础函数库](api-base.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#thread-线程管理库)

</div>
