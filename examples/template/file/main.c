/*
 * 范例：template/file —— 从模板文件直接编译（最常见入口）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtTemplateCompileFile  读取 .tpl 源文件并编译为不可变模板
 *   xrtTemplateRender       渲染（同 core 范例，此处演示文件来源）
 * 模块宏：XRT_MODULE_TEMPLATE（依赖 VALUE/FILE）
 * 编译（单头形态，Windows，须在仓库根目录运行以找到相对路径）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/template/file/main.c -lws2_32 -liphlpapi
 * 预期输出（page.tpl 内容 "Hello {$name}"）：
 *   Hello Alice
 *
 * CompileFile = 读文件 + Compile 一步完成；
 *   文件不存在/语法错误都返回 NULL 并设置线程错误
 *  （错误信息带偏移，xrtTemplateErrorOffset 可定位）。
 * 生产建议：启动时编译一次并持有模板对象（引用计数），
 *   渲染热路径零解析开销。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	xtemplate* pTemplate = NULL;
	xvalue* pData = NULL;
	str sOutput = NULL;
	int iResult = 1;

	pTemplate = xrtTemplateCompileFile("examples/template/file/page.tpl");
	pData = xrtValueObject();
	if ( (pTemplate == NULL) || (pData == NULL) ||
		 !xrtValueObjectSetNew(
			pData,
			XRT_STR_LITERAL("name"),
			xrtValueString(XRT_STR_LITERAL("Alice"))
		 ) ) {
		goto cleanup;
	}

	sOutput = xrtTemplateRender(pTemplate, pData, NULL);
	if ( sOutput == NULL ) {
		iResult = 2;
		goto cleanup;
	}
	printf("%s\n", sOutput);
	iResult = 0;

cleanup:
	xrtFree(sOutput);
	if ( pData != NULL ) {
		xrtValueRelease(pData);
	}
	if ( pTemplate != NULL ) {
		xrtTemplateRelease(pTemplate);
	}
	return iResult;
}
