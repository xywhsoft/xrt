# Thread 线程管理库

> 多线程和同步功能

[返回索引](README.md)

---

## 📑 目录

- [线程操作](#线程操作)
- [线程同步](#线程同步)

---

## 线程操作

### xrtThreadCreate

创建线程

**函数原型：**
```c
XXAPI xthread xrtThreadCreate(ptr pProc, ptr pParam, size_t iStackSize);
```

**参数：**
- `pProc` - 线程函数
- `pParam` - 传递给线程的参数
- `iStackSize` - 栈大小（0使用默认）

**返回值：**
- 线程对象

**示例：**
```c
ptr ThreadFunc(ptr param) {
    int* value = (int*)param;
    printf("Thread: %d\n", *value);
    return NULL;
}

int data = 42;
xthread t = xrtThreadCreate(ThreadFunc, &data, 0);
// 等待线程结束：需要自己实现或使用系统 API
```

---

**注意：** xrt.h 中只提供了 `xrtThreadCreate` 函数，线程的等待和释放需要使用系统 API：

```c
#ifdef _WIN32
    WaitForSingleObject(t->Handle, INFINITE);
    CloseHandle(t->Handle);
#else
    pthread_join((pthread_t)t->Handle, NULL);
#endif
xrtFree(t);
```

---

### xrtSleep

线程休眠

**函数原型：**
```c
XXAPI void xrtSleep(uint32 ms);
```

**参数：**
- `ms` - 毫秒数

**示例：**
```c
xrtSleep(1000);  // 休眠1秒
```

---

## 使用场景

### 1. 并行处理

```c
ptr WorkerThread(ptr param) {
    int id = *(int*)param;
    printf("Worker %d started\n", id);
    // 处理任务
    return NULL;
}

void ParallelProcess() {
    xthread threads[4];
    int ids[4] = {1, 2, 3, 4};
    
    for (int i = 0; i < 4; i++) {
        threads[i] = xrtThreadCreate(WorkerThread, &ids[i], 0);
    }
    
    // 等待线程：需要使用系统 API
    for (int i = 0; i < 4; i++) {
        #ifdef _WIN32
            WaitForSingleObject(threads[i]->Handle, INFINITE);
            CloseHandle(threads[i]->Handle);
        #else
            pthread_join((pthread_t)threads[i]->Handle, NULL);
        #endif
        xrtFree(threads[i]);
    }
}
```

---

## 相关文档

- [Base 基础函数库](api-base.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#thread-线程管理库)

</div>
