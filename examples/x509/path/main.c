#include <xrt.h>

#include "../../../tests/fixtures/x509_path_legacy.h"

#include <stdio.h>
#include <string.h>



/*
 * 范例：x509/path —— 路径验证：有序链 + 信任锚 → 有效/无效
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtX509Parse / xrtX509Anchor   解析证书 / 从根证书建信任锚
 *   xrtX509PathValidate            完整路径验证（RFC 5280）
 *   xx509pathconfig                验证策略（时间/SHA-1/用途等）
 * 模块宏：XRT_MODULE_X509（依赖 CRYPTO）
 * 编译（单头形态，Windows，仓库根目录；fixtures 取自 tests）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -I . -include xrt.h impl.c ${BS}
 *       examples/x509/path/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   certificate path is valid
 *
 * 验证矩阵：逐级签名、有效期窗口（Time 取自叶证书 NotBefore，
 *   刻意避开边界歧义）、基本约束与路径长度、密钥用途、
 *   名称约束收窄、签名算法策略（默认拒绝 SHA-1）。
 *   路径方向：目标在前、中间 CA 向后；信任锚不属于路径数组。
 */


/* 验证一条从目标证书经过中间 CA 到独立信任锚的有序路径。 */
int main(void)
{
	xx509cert Leaf;
	xx509cert Intermediate;
	xx509cert Root;
	xx509anchor Anchor;
	xx509pathconfig Config;
	const xx509cert* Path[2];

	if ( !xrtX509Parse(
		X509_PATH_LEAF, sizeof(X509_PATH_LEAF), &Leaf
	) || !xrtX509Parse(
		X509_PATH_INTERMEDIATE, sizeof(X509_PATH_INTERMEDIATE), &Intermediate
	) || !xrtX509Parse(
		X509_PATH_ROOT, sizeof(X509_PATH_ROOT), &Root
	) || !xrtX509Anchor(&Root, &Anchor) ) {
		fprintf(stderr, "%s\n", xrtErrorMessage(xrtGetError()));
		return 1;
	}

	memset(&Config, 0, sizeof(Config));
	Config.Time = Leaf.NotBefore;
	Path[0] = &Leaf;
	Path[1] = &Intermediate;
	if ( !xrtX509PathValidate(Path, 2u, &Anchor, &Config) ) {
		fprintf(stderr, "%s\n", xrtErrorMessage(xrtGetError()));
		return 1;
	}
	printf("certificate path is valid\n");
	return 0;
}
