#include <xrt.h>

#include "../../../tests/fixtures/x509_path_legacy.h"

#include <stdio.h>
#include <string.h>



/*
 * 范例：x509/path_build —— 建链：从无序候选自动构造有效路径
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtX509PathBuild   逆向建链 + 正向验证一步完成
 *   xx509pathsource    候选源：可用签发者 + 信任锚
 *   xx509pathresult    结果：路径数组 + 数量
 * 模块宏：XRT_MODULE_X509
 * 编译（单头形态，Windows，仓库根目录）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -I . -include xrt.h impl.c ${BS}
 *       examples/x509/path_build/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   certificate path contains 2 certificate(s)
 *
 * 与 path 范例的分工：Path 要求调用方自己排好链；
 *   PathBuild 拿"一堆证书 + 锚点"自动找链——TLS 握手拿到
 *   对端链（顺序不保证）后的标准处理。输出 2 张 =
 *   叶 + 中间 CA（锚不计入）。候选池大时有深度/宽度约束。
 */


/* 从无序候选证书中构建并验证一条到独立信任锚的认证路径。 */
int main(void)
{
	xx509cert Leaf;
	xx509cert Intermediate;
	xx509cert Root;
	xx509anchor Anchor;
	xx509pathsource Source;
	xx509pathconfig Config;
	xx509pathresult Result;
	const xx509cert* Issuers[2];
	const xx509cert* Path[3];

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
	Issuers[0] = &Root;
	Issuers[1] = &Intermediate;
	Source.Issuers = Issuers;
	Source.IssuerCount = 2u;
	Source.Anchors = &Anchor;
	Source.AnchorCount = 1u;
	if ( !xrtX509PathBuild(
		&Leaf, &Source, &Config, Path, 3u, &Result
	) ) {
		fprintf(stderr, "%s\n", xrtErrorMessage(xrtGetError()));
		return 1;
	}
	printf("certificate path contains %zu certificate(s)\n", Result.Count);
	return 0;
}
