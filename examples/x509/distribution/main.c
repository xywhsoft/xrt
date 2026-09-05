#include <stdio.h>

#include <xrt.h>



/*
 * 范例：x509/distribution —— 分发点扩展：CRL 下载 URI 提取
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtX509DistributionInit/Read   分发点游标
 *   xrtX509GeneralNameRead         点内通用名（URI/DNS/...）
 * 模块宏：XRT_MODULE_X509
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/x509/distribution/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   CRL URI: http://crl
 *
 * CRL 获取链路：证书的分发点扩展说明"CRL 在哪下载"——
 *   HTTP URI 是最常见形态。TLS 服务端软失败 / OCSP 缺席时，
 *   按 URI 拉取 CRL 再走 crl_policy 范例的查询路径，
 *   即完整的离线吊销检查。
 */


/* 展示零分配遍历证书分发点正文中的 URI。 */
int main(void)
{
	static const uint8 PointsDer[] = {
		0x30, 0x12, 0x30, 0x10, 0xA0, 0x0E, 0xA0, 0x0C, 0x86, 0x0A,
		0x68, 0x74, 0x74, 0x70, 0x3A, 0x2F, 0x2F, 0x63, 0x72, 0x6C
	};
	xx509distributioncursor Points;
	xx509distributionpoint Point;
	xx509genname Name;

	if ( !xrtX509DistributionInit(
		(xbytesview) { PointsDer, sizeof(PointsDer) }, &Points
	) || (xrtX509DistributionRead(&Points, &Point) != X509_VALUE) ||
		!Point.HasName ||
		(xrtX509GeneralNameRead(
			&Point.Name.FullNames, &Name
		) != X509_VALUE) || (Name.Type != X509_NAME_URI) ) {
		return 1;
	}
	printf("CRL URI: %.*s\n", (int)Name.Value.Size, (cstr)Name.Value.Data);
	return 0;
}
