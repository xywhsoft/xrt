#include <stdio.h>

#include <xrt.h>



/* 展示从源码文件直接编译不可变模板的常见入口。 */
int main(void)
{
	xtemplate* pTemplate = NULL;
	xvalue* pData = NULL;
	str sOutput = NULL;
	int iResult = 1;

	/* 编译模板并准备渲染数据。 */
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

	/* 渲染结果由调用方持有。 */
	sOutput = xrtTemplateRender(pTemplate, pData, NULL);
	if ( sOutput == NULL ) {
		iResult = 2;
		goto cleanup;
	}
	printf("%s\n", sOutput);
	iResult = 0;

	/* 所有失败路径统一释放已经取得的资源。 */
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
