/*
 * XRT Example - Plugin System
 * XRT 范例 - 插件系统
 *
 * Description / 说明:
 *   EN: Demonstrates dynamic plugin loading system with plugin interface,
 *       registry, and lifecycle management. Shows how to load/unload plugins
 *       at runtime, register plugin services, and handle plugin dependencies.
 *       Implements cross-platform plugin architecture.
 *   CN: 演示动态插件加载系统，包括插件接口、注册表和生命周期管理。
 *       展示如何在运行时加载/卸载插件、注册插件服务和处理插件依赖关系。
 *       实现跨平台插件架构。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/advanced_plugin_system.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/advanced_plugin_system -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Dynamic library loading
 *   - Plugin interface standardization
 *   - Service registry pattern
 *   - Plugin lifecycle management
 *   - Cross-platform compatibility
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <time.h>

#ifdef _WIN32
	#include <windows.h>
	typedef HMODULE PluginHandle;
	#define PLUGIN_EXTENSION ".dll"
#else
	#include <dlfcn.h>
	typedef void* PluginHandle;
	#define PLUGIN_EXTENSION ".so"
#endif

// Plugin interface version
// 插件接口版本
#define PLUGIN_INTERFACE_VERSION 1

// Plugin information structure
// 插件信息结构
typedef struct {
	int iVersion;           // Interface version / 接口版本
	str sName;             // Plugin name / 插件名称
	str sDescription;      // Plugin description / 插件描述
	str sAuthor;           // Plugin author / 插件作者
	str sVersion;          // Plugin version / 插件版本
} PluginInfo;

// Plugin interface functions
// 插件接口函数
typedef PluginInfo* (*PluginGetInfoFunc)();
typedef int (*PluginInitializeFunc)(void* pUserData);
typedef void (*PluginShutdownFunc)();
typedef void (*PluginExecuteFunc)(void* pParams);

// Plugin structure
// 插件结构
typedef struct Plugin {
	str sName;                      // Plugin name / 插件名称
	str sFilePath;                  // Plugin file path / 插件文件路径
	PluginHandle hLibrary;          // Library handle / 库句柄
	
	// Plugin functions
	// 插件函数
	PluginGetInfoFunc pfnGetInfo;
	PluginInitializeFunc pfnInitialize;
	PluginShutdownFunc pfnShutdown;
	PluginExecuteFunc pfnExecute;
	
	// Plugin info
	// 插件信息
	PluginInfo* pInfo;
	int bLoaded;                    // Load status / 加载状态
	int bInitialized;               // Initialization status / 初始化状态
	void* pUserData;                // User data / 用户数据
	
	struct Plugin* pNext;           // Next plugin in list / 列表中的下一个插件
} Plugin;

// Plugin registry structure
// 插件注册表结构
typedef struct {
	Plugin* pPlugins;               // Loaded plugins list / 已加载插件列表
	size_t iPluginCount;            // Number of loaded plugins / 已加载插件数量
	xbuffer pBufferSearchPaths;     // Search paths buffer / 搜索路径缓冲区
} PluginRegistry;

// Create plugin registry
// 创建插件注册表
PluginRegistry* PluginRegistryCreate()
{
	PluginRegistry* pRegistry = xrtMalloc(sizeof(PluginRegistry));
	pRegistry->pPlugins = NULL;
	pRegistry->iPluginCount = 0;
	pRegistry->pBufferSearchPaths = xrtBufferCreate(0);
	return pRegistry;
}

// Destroy plugin registry
// 销毁插件注册表
void PluginRegistryDestroy(PluginRegistry* pRegistry)
{
	if ( pRegistry ) {
		// Unload all plugins
		// 卸载所有插件
		Plugin* pCurrent = pRegistry->pPlugins;
		while ( pCurrent ) {
			Plugin* pNext = pCurrent->pNext;
			if ( pCurrent->bInitialized && pCurrent->pfnShutdown ) {
				pCurrent->pfnShutdown();
			}
			if ( pCurrent->hLibrary ) {
#ifdef _WIN32
				FreeLibrary(pCurrent->hLibrary);
#else
				dlclose(pCurrent->hLibrary);
#endif
			}
			if ( pCurrent->sName ) xrtFree(pCurrent->sName);
			if ( pCurrent->sFilePath ) xrtFree(pCurrent->sFilePath);
			if ( pCurrent->pInfo ) xrtFree(pCurrent->pInfo);
			xrtFree(pCurrent);
			pCurrent = pNext;
		}
		
		xrtBufferDestroy(pRegistry->pBufferSearchPaths);
		xrtFree(pRegistry);
	}
}

// Add search path
// 添加搜索路径
void PluginRegistryAddSearchPath(PluginRegistry* pRegistry, str sPath)
{
	size_t iLen = strlen(sPath);
	if ( iLen > 0 ) {
		xrtBufferAppend(pRegistry->pBufferSearchPaths, sPath, iLen, XBUF_ANSI);
		xrtBufferAppend(pRegistry->pBufferSearchPaths, ";", 1, XBUF_ANSI);
	}
}

// Load plugin library
// 加载插件库
PluginHandle LoadPluginLibrary(str sFilePath)
{
#ifdef _WIN32
	return LoadLibraryA(sFilePath);
#else
	return dlopen(sFilePath, RTLD_LAZY);
#endif
}

// Get function from plugin
// 从插件获取函数
void* GetPluginFunction(PluginHandle hLibrary, str sFunctionName)
{
#ifdef _WIN32
	return (void*)GetProcAddress(hLibrary, sFunctionName);
#else
	return dlsym(hLibrary, sFunctionName);
#endif
}

// Load plugin
// 加载插件
Plugin* PluginRegistryLoadPlugin(PluginRegistry* pRegistry, str sPluginName)
{
	// Create plugin structure
	// 创建插件结构
	Plugin* pPlugin = xrtMalloc(sizeof(Plugin));
	pPlugin->sName = xrtCopyStr(sPluginName, strlen(sPluginName) + 1);
	pPlugin->sFilePath = NULL;
	pPlugin->hLibrary = NULL;
	pPlugin->pfnGetInfo = NULL;
	pPlugin->pfnInitialize = NULL;
	pPlugin->pfnShutdown = NULL;
	pPlugin->pfnExecute = NULL;
	pPlugin->pInfo = NULL;
	pPlugin->bLoaded = 0;
	pPlugin->bInitialized = 0;
	pPlugin->pUserData = NULL;
	pPlugin->pNext = pRegistry->pPlugins;
	
	// Try to find plugin file
	// 尝试查找插件文件
	char sSearchBuffer[1024];
	char* pPaths = (char*)pRegistry->pBufferSearchPaths->Buffer;
	char* pPath = strtok(pPaths, ";");
	
	while ( pPath ) {
		snprintf(sSearchBuffer, sizeof(sSearchBuffer), "%s/%s%s", 
		         pPath, sPluginName, PLUGIN_EXTENSION);
		
		PluginHandle hLib = LoadPluginLibrary(sSearchBuffer);
		if ( hLib ) {
			pPlugin->hLibrary = hLib;
			pPlugin->sFilePath = xrtCopyStr(sSearchBuffer, strlen(sSearchBuffer) + 1);
			break;
		}
		
		pPath = strtok(NULL, ";");
	}
	
	if ( !pPlugin->hLibrary ) {
		printf("Failed to load plugin: %s\n", sPluginName);
		xrtFree(pPlugin->sName);
		xrtFree(pPlugin);
		return NULL;
	}
	
	// Get plugin functions
	// 获取插件函数
	pPlugin->pfnGetInfo = (PluginGetInfoFunc)GetPluginFunction(pPlugin->hLibrary, "PluginGetInfo");
	pPlugin->pfnInitialize = (PluginInitializeFunc)GetPluginFunction(pPlugin->hLibrary, "PluginInitialize");
	pPlugin->pfnShutdown = (PluginShutdownFunc)GetPluginFunction(pPlugin->hLibrary, "PluginShutdown");
	pPlugin->pfnExecute = (PluginExecuteFunc)GetPluginFunction(pPlugin->hLibrary, "PluginExecute");
	
	if ( !pPlugin->pfnGetInfo ) {
		printf("Plugin %s missing required function: PluginGetInfo\n", sPluginName);
#ifdef _WIN32
		FreeLibrary(pPlugin->hLibrary);
#else
		dlclose(pPlugin->hLibrary);
#endif
		xrtFree(pPlugin->sName);
		xrtFree(pPlugin->sFilePath);
		xrtFree(pPlugin);
		return NULL;
	}
	
	// Get plugin info
	// 获取插件信息
	pPlugin->pInfo = pPlugin->pfnGetInfo();
	if ( !pPlugin->pInfo ) {
		printf("Plugin %s returned NULL info\n", sPluginName);
#ifdef _WIN32
		FreeLibrary(pPlugin->hLibrary);
#else
		dlclose(pPlugin->hLibrary);
#endif
		xrtFree(pPlugin->sName);
		xrtFree(pPlugin->sFilePath);
		xrtFree(pPlugin);
		return NULL;
	}
	
	// Add to registry
	// 添加到注册表
	pRegistry->pPlugins = pPlugin;
	pRegistry->iPluginCount++;
	pPlugin->bLoaded = 1;
	
	printf("Loaded plugin: %s (%s)\n", pPlugin->pInfo->sName, pPlugin->pInfo->sVersion);
	
	return pPlugin;
}

// Initialize plugin
// 初始化插件
int PluginInitialize(Plugin* pPlugin, void* pUserData)
{
	if ( !pPlugin->bLoaded || pPlugin->bInitialized ) {
		return -1;
	}
	
	if ( pPlugin->pfnInitialize ) {
		int iResult = pPlugin->pfnInitialize(pUserData);
		if ( iResult == 0 ) {
			pPlugin->bInitialized = 1;
			pPlugin->pUserData = pUserData;
			printf("Initialized plugin: %s\n", pPlugin->pInfo->sName);
			return 0;
		} else {
			printf("Failed to initialize plugin: %s\n", pPlugin->pInfo->sName);
			return iResult;
		}
	}
	
	// No initialize function, consider initialized
	// 无初始化函数，视为已初始化
	pPlugin->bInitialized = 1;
	return 0;
}

// Execute plugin
// 执行插件
void PluginExecute(Plugin* pPlugin, void* pParams)
{
	if ( pPlugin->bInitialized && pPlugin->pfnExecute ) {
		pPlugin->pfnExecute(pParams);
	}
}

// Shutdown plugin
// 关闭插件
void PluginShutdown(Plugin* pPlugin)
{
	if ( pPlugin->bInitialized && pPlugin->pfnShutdown ) {
		pPlugin->pfnShutdown();
		pPlugin->bInitialized = 0;
		pPlugin->pUserData = NULL;
		printf("Shutdown plugin: %s\n", pPlugin->pInfo->sName);
	}
}

// Find plugin by name
// 按名称查找插件
Plugin* PluginRegistryFindPlugin(PluginRegistry* pRegistry, str sName)
{
	Plugin* pCurrent = pRegistry->pPlugins;
	while ( pCurrent ) {
		if ( strcmp(pCurrent->pInfo->sName, sName) == 0 ) {
			return pCurrent;
		}
		pCurrent = pCurrent->pNext;
	}
	return NULL;
}

// List all plugins
// 列出所有插件
void PluginRegistryListPlugins(PluginRegistry* pRegistry)
{
	printf("Loaded Plugins (%zu):\n", pRegistry->iPluginCount);
	
	Plugin* pCurrent = pRegistry->pPlugins;
	int iIndex = 1;
	while ( pCurrent ) {
		printf("  %d. %s v%s - %s\n", iIndex++, 
		       pCurrent->pInfo->sName,
		       pCurrent->pInfo->sVersion,
		       pCurrent->pInfo->sDescription);
		pCurrent = pCurrent->pNext;
	}
}

// Mock plugin functions for demonstration
// 演示用的模拟插件函数
PluginInfo* MockPluginGetInfo()
{
	PluginInfo* pInfo = xrtMalloc(sizeof(PluginInfo));
	pInfo->iVersion = PLUGIN_INTERFACE_VERSION;
	pInfo->sName = "MockPlugin";
	pInfo->sDescription = "Demonstration plugin for testing";
	pInfo->sAuthor = "XRT Team";
	pInfo->sVersion = "1.0.0";
	return pInfo;
}

int MockPluginInitialize(void* pUserData)
{
	printf("Mock plugin initialized with user data: %p\n", pUserData);
	return 0;
}

void MockPluginShutdown()
{
	printf("Mock plugin shutdown\n");
}

void MockPluginExecute(void* pParams)
{
	printf("Mock plugin executed with params: %p\n", pParams);
}

// Test plugin system
// 测试插件系统
void TestPluginSystem()
{
	printf("=== Plugin System Test ===\n");
	printf("=== 插件系统测试 ===\n");
	
	PluginRegistry* pRegistry = PluginRegistryCreate();
	
	// Add search paths
	// 添加搜索路径
	PluginRegistryAddSearchPath(pRegistry, ".");
	PluginRegistryAddSearchPath(pRegistry, "./plugins");
	
	// Since we can't load real plugins in this demo, we'll simulate
	// 由于在此演示中无法加载真实插件，我们将进行模拟
	
	// Create mock plugin manually
	// 手动创建模拟插件
	Plugin* pMockPlugin = xrtMalloc(sizeof(Plugin));
	pMockPlugin->sName = xrtCopyStr("MockPlugin", 10);
	pMockPlugin->sFilePath = xrtCopyStr("mock_plugin" PLUGIN_EXTENSION, 
	                                   strlen("mock_plugin" PLUGIN_EXTENSION) + 1);
	pMockPlugin->hLibrary = NULL;  // Simulated / 模拟的
	pMockPlugin->pfnGetInfo = MockPluginGetInfo;
	pMockPlugin->pfnInitialize = MockPluginInitialize;
	pMockPlugin->pfnShutdown = MockPluginShutdown;
	pMockPlugin->pfnExecute = MockPluginExecute;
	pMockPlugin->pInfo = MockPluginGetInfo();
	pMockPlugin->bLoaded = 1;
	pMockPlugin->bInitialized = 0;
	pMockPlugin->pUserData = NULL;
	pMockPlugin->pNext = pRegistry->pPlugins;
	
	pRegistry->pPlugins = pMockPlugin;
	pRegistry->iPluginCount = 1;
	
	// List plugins
	// 列出插件
	PluginRegistryListPlugins(pRegistry);
	
	// Initialize plugin
	// 初始化插件
	int iUserData = 42;
	PluginInitialize(pMockPlugin, &iUserData);
	
	// Execute plugin
	// 执行插件
	str sTestParam = "Hello from main program";
	PluginExecute(pMockPlugin, sTestParam);
	
	// Shutdown plugin
	// 关闭插件
	PluginShutdown(pMockPlugin);
	
	// Cleanup
	// 清理
	PluginRegistryDestroy(pRegistry);
}

// Test plugin registry operations
// 测试插件注册表操作
void TestPluginRegistry()
{
	printf("\n=== Plugin Registry Test ===\n");
	printf("=== 插件注册表测试 ===\n");
	
	PluginRegistry* pRegistry = PluginRegistryCreate();
	
	// Add multiple search paths
	// 添加多个搜索路径
	PluginRegistryAddSearchPath(pRegistry, "/usr/lib/plugins");
	PluginRegistryAddSearchPath(pRegistry, "/opt/myapp/plugins");
	PluginRegistryAddSearchPath(pRegistry, "./local_plugins");
	
	printf("Search paths added\n");
	
	// Try to load non-existent plugin
	// 尝试加载不存在的插件
	Plugin* pPlugin = PluginRegistryLoadPlugin(pRegistry, "nonexistent_plugin");
	if ( !pPlugin ) {
		printf("Correctly failed to load nonexistent plugin\n");
	}
	
	PluginRegistryDestroy(pRegistry);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Plugin System Demo\n");
	printf("XRT 插件系统演示\n");
	printf("====================\n\n");
	
	// Run tests
	// 运行测试
	TestPluginSystem();
	TestPluginRegistry();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}