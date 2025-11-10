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
XXAPI xthread xrtThreadCreate(ptr (*pFunc)(ptr), ptr pParam);
```

**参数：**
- `pFunc` - 线程函数
- `pParam` - 传递给线程的参数

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
xthread t = xrtThreadCreate(ThreadFunc, &data);
xrtThreadJoin(t);
xrtThreadFree(t);
```

---

### xrtThreadJoin

等待线程结束

**函数原型：**
```c
XXAPI ptr xrtThreadJoin(xthread objThread);
```

**返回值：**
- 线程返回值

---

### xrtThreadFree

释放线程对象

**函数原型：**
```c
XXAPI void xrtThreadFree(xthread objThread);
```

---

### xrtSleep

线程休眠

**函数原型：**
```c
XXAPI void xrtSleep(int iMilliseconds);
```

**参数：**
- `iMilliseconds` - 毫秒数

**示例：**
```c
xrtSleep(1000);  // 休眠1秒
```

---

## 线程同步

### 互斥锁 (Mutex)

```c
typedef struct xmutex_struct* xmutex;

XXAPI xmutex xrtMutexCreate();
XXAPI void xrtMutexLock(xmutex objMutex);
XXAPI void xrtMutexUnlock(xmutex objMutex);
XXAPI void xrtMutexFree(xmutex objMutex);
```

**示例：**
```c
xmutex mutex = xrtMutexCreate();

xrtMutexLock(mutex);
// 临界区
xrtMutexUnlock(mutex);

xrtMutexFree(mutex);
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
        threads[i] = xrtThreadCreate(WorkerThread, &ids[i]);
    }
    
    for (int i = 0; i < 4; i++) {
        xrtThreadJoin(threads[i]);
        xrtThreadFree(threads[i]);
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
