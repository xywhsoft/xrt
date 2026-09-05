#include <stdio.h>

#include <xrt.h>



/*
 * 范例：file/lock —— 文件锁：非阻塞排他锁
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtFileLock / Unlock   区域加解锁（本例整文件）
 *   XFILE_LOCK_EXCLUSIVE   排他模式（另有共享 SHARED）
 *   第三参数 false          非阻塞：拿不到立即失败
 * 模块宏：XRT_MODULE_FILE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/file/lock/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   locked
 *
 * 跨平台收口：Windows LockFileEx / POSIX fcntl——语义统一为
 *   "进程存活期间持有，退出自动释放"。单实例守护进程、
 *   任务队列文件的互斥就用它；阻塞版传 true 等待持锁者。
 */


/* 展示对整个文件使用非阻塞排他锁。 */
int main(void)
{
	static const char sPath[] = "xrt-file-lock-example.tmp";
	xfile File = xrtOpen(sPath, XFILE_READ | XFILE_WRITE |
		XFILE_CREATE);

	if ( File == NULL ) {
		return 1;
	}
	if ( !xrtFileLock(File, XFILE_LOCK_EXCLUSIVE, false) ) {
		(void)xrtClose(File);
		return 1;
	}
	printf("locked\n");
	if ( !xrtFileUnlock(File) || !xrtClose(File) ) {
		return 1;
	}
	return xrtFileDelete(sPath) ? 0 : 1;
}
