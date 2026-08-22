#ifndef XRT_INTERNAL_FILE_TEMP_H
#define XRT_INTERNAL_FILE_TEMP_H

#include "xrt_file.h"



#if defined(XRT_FEATURE_FILE_TEMP)

/* 临时对象名称冲突时的最大重试次数。 */
#define XRT_TEMP_ATTEMPTS 128u



/* 临时名称生成器统一管理默认值、系统目录和拥有内存。 */
typedef struct __xrttempname {
	cstr Directory;
	cstr Prefix;
	cstr Suffix;
	str OwnedDirectory;
} __xrttempname;



/* 初始化临时名称生成器；目录为空时使用系统临时目录。 */
bool __xrtTempNameInit(__xrttempname* pName,
	cstr sDirectory, cstr sPrefix, cstr sSuffix,
	cstr sDefaultPrefix, cstr sDefaultSuffix);



/* 生成下一条带安全随机部分的拥有路径。 */
str __xrtTempNameNext(const __xrttempname* pName);



/* 释放临时名称生成器拥有的系统目录。 */
void __xrtTempNameFree(__xrttempname* pName);



/* 按指定创建模式在目录中排他创建临时文件。 */
xfile __xrtFileTempCreate(cstr sDirectory, cstr sPrefix,
	cstr sSuffix, uint32 iMode, str* pPath);

#endif

#endif
