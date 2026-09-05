#include <xrt.h>

#include "../../../tests/fixtures/x509_path_legacy.h"

#include <stdio.h>



/*
 * 范例：x509/store —— 拥有式信任库与建链源
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtX509StoreCreate/Add/Free   信任库生命周期
 *   xrtX509StoreSource            库 → xx509pathsource（可叠加外部中间证书）
 * 模块宏：XRT_MODULE_X509
 * 编译（单头形态，Windows，仓库根目录）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -I . -include xrt.h impl.c ${BS}
 *       examples/x509/store/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   trust store contains 1 anchor(s)
 *
 * Store 的定位：拥有式证书容器（解析 + 索引），
 *   Source 把它适配成 PathBuild 的输入——第二参数还能
 *   叠加"会话中间证书"（握手对端额外提供的链材料），
 *   库内容不受影响。锚在 Add 时即完成解析与规范化。
 */


/* 建立拥有式信任库，并生成可与外部中间证书组合的建链源。 */
int main(void)
{
	xx509store* pStore = xrtX509StoreCreate();
	xx509pathsource Source;

	if ( (pStore == NULL) || (xrtX509StoreAdd(
		pStore, X509_PATH_ROOT, sizeof(X509_PATH_ROOT)
	) != X509_VALUE) || !xrtX509StoreSource(
		pStore, NULL, 0, &Source
	) ) {
		fprintf(stderr, "%s\n", xrtErrorMessage(xrtGetError()));
		xrtX509StoreFree(pStore);
		return 1;
	}
	printf("trust store contains %zu anchor(s)\n", Source.AnchorCount);
	xrtX509StoreFree(pStore);
	return 0;
}
