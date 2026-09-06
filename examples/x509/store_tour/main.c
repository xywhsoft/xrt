/*
 * 范例：x509/store_tour —— 信任库导入与吊销归并
 * ----------------------------------------------------------------
 * 演示 API：
 *   【信任库】    xrtX509StoreAddPem（PEM 文本事务式导入）
 *                 xrtX509StoreAddSystem（平台系统信任锚）
 *                 xrtX509StoreCertificate / StoreAnchor（按索引借用）
 *   【路径策略】  xrtX509PathConfigInit（当前时间严格默认）
 *   【吊销归并】  xrtX509RevocationInit / RevocationUpdate /
 *                 RevocationResult（多份 CRL 查询结果归并状态机，
 *                 Update 收 CrlCheck 的查询结果）
 * 模块宏：XRT_MODULE_X509
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/x509/store_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   store: pem import 1 + system anchors >= 10
 *   store: certificate + anchor borrowed by index ok
 *   store: path config defaults ok
 *   store: revocation merge: updated -> revoked
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>
#include "fixture.h"

/* 把 PEM 行数组拼回完整文本（含结尾换行）。 */
static size_t examplePemJoin(char* sText, size_t iCapacity)
{
	size_t iOffset = 0;

	for ( size_t i = 0; i < sizeof(exampleCaPem) /
			sizeof(exampleCaPem[0]); i++ ) {
		size_t iLen = strlen(exampleCaPem[i]);

		if ( iOffset + iLen + 1u >= iCapacity ) {
			return 0u;
		}
		memcpy(sText + iOffset, exampleCaPem[i], iLen);
		iOffset += iLen;
		sText[iOffset++] = '\n';
	}
	sText[iOffset] = '\0';
	return iOffset;
}

int main(void)
{
	char arrPem[2048];
	xx509store* pStore = NULL;
	xx509cert Ca;
	xx509cert Leaf;
	xx509crl Crl;
	xx509crlconfig CrlConfig;
	xx509crlvalid Valid;
	xx509pathconfig PathConfig;
	xx509revocationcheck Check;
	xx509revocation Revocation;
	size_t iAdded = 0;
	size_t iPemSize;
	size_t iSystemAnchors;
	int iResult = 1;

	if ( !xrtX509Parse(exampleCaDer, sizeof(exampleCaDer), &Ca) ||
		!xrtX509Parse(exampleLeafDer, sizeof(exampleLeafDer),
			&Leaf) ||
		!xrtX509CrlParse(exampleCrlDer, sizeof(exampleCrlDer),
			&Crl) ) {
		goto Cleanup;
	}

	/* ---- 信任库导入：PEM 文本（+1 锚）与系统信任锚。 ---- */
	iPemSize = examplePemJoin(arrPem, sizeof(arrPem));
	if ( (iPemSize == 0u) ||
		!xrtX509StoreAddPem(pStore = xrtX509StoreCreate(),
			arrPem, iPemSize, &iAdded) ||
		(iAdded != 1u) ) {
		goto Cleanup;
	}
	if ( !xrtX509StoreAddSystem(pStore, &iAdded) ) {
		goto Cleanup;
	}
	iSystemAnchors = iAdded;
	if ( iSystemAnchors < 10u ) {
		goto Cleanup;
	}
	printf("store: pem import 1 + system anchors >= 10\n");

	/* ---- 按索引借用：第一份是 PEM 导入的 CA，锚名称可读。 ---- */
	{
		const xx509cert* pCert = xrtX509StoreCertificate(pStore,
			0u);
		const xx509anchor* pAnchor = xrtX509StoreAnchor(pStore, 0u);

		if ( (pCert == NULL) || (pAnchor == NULL) ||
			(pAnchor->Certificate.Size != pCert->Raw.Size) ||
			(memcmp(pAnchor->Certificate.Data,
				pCert->Raw.Data, pCert->Raw.Size) != 0) ||
			(xrtX509StoreCertificate(pStore, 100000u) !=
				NULL) ) {
			goto Cleanup;
		}
	}
	printf("store: certificate + anchor borrowed by index ok\n");

	/* ---- 路径策略默认值：初始化即可用于 PathBuild/Validate。 ---- */
	xrtX509PathConfigInit(&PathConfig);
	if ( (PathConfig.Time == 0) ) {
		goto Cleanup;
	}
	printf("store: path config defaults ok\n");

	/* ---- 吊销归并：喂入验证过的 CRL → 立即得到撤销终态。 ---- */
	xrtX509CrlConfigInit(&CrlConfig);
	if ( !xrtX509CrlValidate(&Crl, &Ca, &CrlConfig, &Valid) ) {
		goto Cleanup;
	}
	xrtX509RevocationInit(&Check);
	if ( (xrtX509CrlCheck(&Valid, &Leaf, &Revocation) !=
			X509_VALUE) ||
		(Revocation.State != X509_REVOCATION_REVOKED) ||
		(xrtX509RevocationUpdate(&Check, &Revocation) !=
			X509_VALUE) ||
		(xrtX509RevocationResult(&Check, &Revocation) !=
			X509_VALUE) ||
		(Revocation.State != X509_REVOCATION_REVOKED) ) {
		goto Cleanup;
	}
	printf("store: revocation merge: updated -> revoked\n");
	iResult = 0;

Cleanup:
	if ( pStore != NULL ) {
		xrtX509StoreFree(pStore);
	}
	return iResult;
}
