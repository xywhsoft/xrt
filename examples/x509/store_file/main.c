#include <xrt.h>

#include <stdio.h>



/*
 * 范例：x509/store_file —— 从 DER/PEM 文件装载信任库
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtX509StoreCreateFile   文件一步建库（格式自动识别）
 * 模块宏：XRT_MODULE_X509（依赖 FILE/PEM）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/x509/store_file/main.c -lws2_32 -liphlpapi
 * 用法：
 *   x509-store-file <ca-file>
 * 预期输出（无参数时）：
 *   usage: x509-store-file <ca-file>
 *
 * 文件形态自适应：二进制 DER / Base64 PEM（含多证书块）
 *   都能装载——企业自建 CA 常给的就是 PEM 包。
 *   产物直接就是 Store，接 store 范例的 Source 流程
 *   即可服务 TLS 验证。
 */


/* 从命令行指定的 DER 或 PEM 文件创建可直接用于建链的信任库。 */
int main(int iArgc, char** ppArgs)
{
	xx509store* pStore;
	size_t iAdded;

	if ( iArgc != 2 ) {
		fprintf(stderr, "usage: x509-store-file <ca-file>\n");
		return 0;
	}
	pStore = xrtX509StoreCreate();
	if ( (pStore == NULL) || !xrtX509StoreAddFile(
		pStore, ppArgs[1], &iAdded
	) ) {
		xrtX509StoreFree(pStore);
		return 2;
	}
	printf("anchors=%llu added=%llu\n",
		(unsigned long long)xrtX509StoreCount(pStore),
		(unsigned long long)iAdded);
	xrtX509StoreFree(pStore);
	return 0;
}
