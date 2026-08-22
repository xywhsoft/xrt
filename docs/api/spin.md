# Spin

`spin` 提供不记录所有者、不可递归的短临界区锁。它先使用处理器暂停提示自旋，持续竞争时主动让出时间片；等待时间不可预测或临界区可能阻塞时，应改用 `mutex`。

## 裁剪

启用 `XRT_FEATURE_SPIN`，模块依赖 `atomic`。未启用时不声明类型和函数，也不编译实现。

## 生命周期

栈上对象使用 `xrtSpinInit()` 和 `xrtSpinUnit()`，静态对象使用 `XRT_SPIN_INIT`，动态对象使用 `xrtSpinCreate()` 和 `xrtSpinDestroy()`。释放仍被持有的锁会失败并设置 `XERR_STATE`。

```c
xspinlock Lock;

if ( !xrtSpinInit(&Lock) ) {
	return false;
}
if ( !xrtSpinLock(&Lock) ) {
	return false;
}
/* 短小且不阻塞的共享状态更新。 */
(void)xrtSpinUnlock(&Lock);
(void)xrtSpinUnit(&Lock);
```

## 进入与退出

`xrtSpinLock()` 等待直到获得锁。`xrtSpinTryLock()` 只尝试一次，锁繁忙时返回 `false` 且不设置错误。`xrtSpinUnlock()` 使用 Release 顺序发布临界区内的写入。

锁不记录线程所有者，调用方必须保证只有持有者执行解锁；同一执行流重复进入会永久等待。协程、任务或网络 Worker 不应在持锁期间执行阻塞、等待或用户回调。

完整示例位于 `examples/concurrency/spin`，竞争回归位于 `tests/concurrency/test_spin_threads.c`。
