#include <stdio.h>
#include <string.h>

#include <xrt.h>



/*
 * 范例：file/vfs —— 显式命名空间中的便携资源覆盖
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtVfsCreate / Destroy       创建、销毁隔离命名空间
 *   xrtVfsMemoryCreate / Seal    构建不可变内存资源层
 *   xrtVfsMemoryMount / Unmount  按 priority 发布、撤销 overlay
 *   xrtVfsReadAll                通过统一虚拟路径读取
 * 模块宏：XRT_MODULE_VFS_MEMORY
 * 预期输出：
 *   active settings: {"theme":"dark"}
 *   fallback asset: built-in welcome
 *   restored settings: {"theme":"system"}
 *
 * 高优先级层只需要提供被覆盖的文件。资源不存在时返回 MISS，VFS 自动查询下一层；
 * 权限、校验或 I/O 错误会立即终止，不能静默退回内置资源。
 */



static bool exampleRead(xvfs Vfs, cstr sPath,
	cstr sLabel, cstr sExpected)
{
	size_t iSize = 0u;
	bytes pData = xrtVfsReadAll(Vfs, sPath, &iSize);
	size_t iExpected = strlen(sExpected);

	if ( pData == NULL ) return false;
	if ( (iSize != iExpected) ||
		(memcmp(pData, sExpected, iExpected) != 0) ) {
		xrtFree(pData);
		return false;
	}
	printf("%s: %s\n", sLabel, (const char*)pData);
	xrtFree(pData);
	return true;
}



int main(void)
{
	static const char sBuiltinSettings[] = "{\"theme\":\"system\"}";
	static const char sOverrideSettings[] = "{\"theme\":\"dark\"}";
	static const char sBuiltinWelcome[] = "built-in welcome";
	xvfs Vfs = xrtVfsCreate();
	xvfsmemory Builtin = xrtVfsMemoryCreate();
	xvfsmemory Override = xrtVfsMemoryCreate();
	xvfsmount BuiltinMount = NULL;
	xvfsmount OverrideMount = NULL;
	bool bSuccess = false;

	if ( (Vfs == NULL) || (Builtin == NULL) || (Override == NULL) ) goto cleanup;
	if ( !xrtVfsMemoryPutCopy(Builtin, "config/settings.json",
		sBuiltinSettings, sizeof(sBuiltinSettings) - 1u) ||
		!xrtVfsMemoryPutCopy(Builtin, "assets/welcome.txt",
		sBuiltinWelcome, sizeof(sBuiltinWelcome) - 1u) ||
		!xrtVfsMemorySeal(Builtin) ||
		!xrtVfsMemoryPutCopy(Override, "config/settings.json",
		sOverrideSettings, sizeof(sOverrideSettings) - 1u) ||
		!xrtVfsMemorySeal(Override) ) goto cleanup;
	BuiltinMount = xrtVfsMemoryMount(Vfs, "/app", 0,
		XVFS_CASE_SENSITIVE, Builtin, 0u);
	OverrideMount = xrtVfsMemoryMount(Vfs, "/app", 100,
		XVFS_CASE_SENSITIVE, Override, 0u);
	if ( (BuiltinMount == NULL) || (OverrideMount == NULL) ) goto cleanup;
	xrtVfsMemoryDestroy(Builtin);
	Builtin = NULL;
	xrtVfsMemoryDestroy(Override);
	Override = NULL;
	if ( !exampleRead(Vfs, "/app/config/settings.json",
		"active settings", sOverrideSettings) ||
		!exampleRead(Vfs, "/app/assets/welcome.txt",
		"fallback asset", sBuiltinWelcome) ||
		!xrtVfsUnmount(OverrideMount) ||
		!exampleRead(Vfs, "/app/config/settings.json",
		"restored settings", sBuiltinSettings) ) goto cleanup;
	bSuccess = true;

cleanup:
	if ( !bSuccess && (xrtGetError() != NULL) )
		fprintf(stderr, "%s\n", xrtErrorMessage(xrtGetError()));
	if ( OverrideMount != NULL ) {
		(void)xrtVfsUnmount(OverrideMount);
		xrtVfsMountDestroy(OverrideMount);
	}
	if ( BuiltinMount != NULL ) {
		(void)xrtVfsUnmount(BuiltinMount);
		xrtVfsMountDestroy(BuiltinMount);
	}
	xrtVfsMemoryDestroy(Override);
	xrtVfsMemoryDestroy(Builtin);
	xrtVfsDestroy(Vfs);
	return bSuccess ? 0 : 1;
}
