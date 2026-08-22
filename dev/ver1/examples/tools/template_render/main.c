/*
 * XRT Example - Template Render
 * XRT 范例 - 模板渲染工具
 *
 * Description / 说明:
 *   EN: Demonstrates rendering HTML from a JSON data file and template file.
 *   CN: 演示从 JSON 数据文件和模板文件渲染 HTML。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/tools_template_render.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/tools_template_render -lm -lpthread
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include "../../example_common.h"


int main()
{
	str sTemplatePath = NULL;
	str sDataPath = NULL;
	str sOutputPath = NULL;
	str sTemplateText = NULL;
	xvalue pData = NULL;
	xtetemplate hTemplate = NULL;
	size_t iRetSize = 0;
	str sHTML = NULL;

	xrtInit();

	sTemplatePath = exMakeAppFilePath("template_render_demo.tpl");
	sDataPath = exMakeAppFilePath("template_render_demo.json");
	sOutputPath = exMakeAppFilePath("template_render_demo.html");

	xrtFileWriteAll(sTemplatePath, "<h1>{{title}}</h1><p>{{message}}</p>", 35, XRT_CP_UTF8);
	xrtFileWriteAll(sDataPath, "{\"title\":\"XRT\",\"message\":\"Rendered from JSON\"}", 47, XRT_CP_UTF8);

	sTemplateText = xrtFileReadAll(sTemplatePath, XRT_CP_UTF8, NULL);
	pData = xrtParseJSON_File(sDataPath);
	hTemplate = xteParse(sTemplateText, strlen(sTemplateText), "{{}}");
	sHTML = xteMake(hTemplate, pData, NULL, NULL, &iRetSize);
	xrtFileWriteAll(sOutputPath, sHTML, iRetSize, XRT_CP_UTF8);

	printf("output_file = %s\n", sOutputPath);
	printf("html = %s\n", sHTML);

	xrtFree(sTemplateText);
	xrtFree(sHTML);
	xteParseFree(hTemplate);
	xvoUnref(pData);
	xrtFileDelete(sTemplatePath);
	xrtFileDelete(sDataPath);
	xrtFileDelete(sOutputPath);
	xrtFree(sTemplatePath);
	xrtFree(sDataPath);
	xrtFree(sOutputPath);
	xrtUnit();
	return 0;
}
