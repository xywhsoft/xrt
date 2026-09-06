/*
 * 范例：x509/crl_tour —— CRL 档案/条目扩展/验证/状态查询
 * ----------------------------------------------------------------
 * 演示 API：
 *   【档案】      xrtX509CrlConfigInit / CrlNumber /
 *                 CrlAuthorityKeyId / CrlDeltaBase（缺失→DONE）/
 *                 CrlIssuingPoint（缺失→DONE）/ CrlFreshest（缺失→DONE）
 *   【条目】      xrtX509CrlFind（按序列号）/ CrlRevokes（按证书）/
 *                 CrlEntryReason / CrlEntryInvalidityDate /
 *                 CrlEntryIssuer（缺失→DONE）/
 *                 CrlInvalidityDateParse（独立 DER）
 *   【窗口与验证】xrtX509CrlValidAt / CrlVerifyKey（公钥验签）/
 *                 CrlVerify（证书验签）/ CrlValidate（严格档案）
 *   【状态查询】  xrtX509CrlStatus（验 + 查一步到位）/
 *                 CrlSetInit + CrlSetCheck（complete/delta 组合，
 *                 delta 为空时拒绝）
 * 模块宏：XRT_MODULE_X509
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/x509/crl_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   crl: profile number=4098 + aki + absent extensions ok
 *   crl: find/revokes leaf -> entry with reason+invalidity ok
 *   crl: entry issuer absent + independent date parse ok
 *   crl: valid-at now + verify(key/cert) + validate ok
 *   crl: status revoked/good + validate reusable ok
 *   crl: set requires base+delta (null delta rejected) ok
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>
#include "../fixture.h"

int main(void)
{
	xx509cert Ca;
	xx509cert Leaf;
	xx509crl Crl;
	xx509crlconfig Config;
	xx509crlvalid Valid;
	xx509crlset Set;
	xx509crlentry Entry;
	xx509revocation Revocation;
	xx509pubkey CaKey;
	xbytesview CrlNumber;
	xbytesview KeyId;
	int iResult = 1;

	if ( !xrtX509Parse(exampleCaDer, sizeof(exampleCaDer), &Ca) ||
		!xrtX509Parse(exampleLeafDer, sizeof(exampleLeafDer), &Leaf) ||
		!xrtX509CrlParse(exampleCrlDer, sizeof(exampleCrlDer),
			&Crl) ) {
		goto Cleanup;
	}

	/* ---- 档案：CRLNumber 去符号零后为 0x1002、AKI 非空、
	 * 三个 CRL 级扩展在本夹具中缺失 → DONE。 ---- */
	{
		xx509issuingpoint NoPoint;
		xx509distributioncursor NoPoints;
		xx509authoritykeyid AuthorityId;

		if ( (xrtX509CrlNumber(&Crl, &CrlNumber) != X509_VALUE) ||
			(CrlNumber.Size != 2u) ||
			(CrlNumber.Data[0] != 0x10u) ||
			(CrlNumber.Data[1] != 0x02u) ||
			(xrtX509CrlAuthorityKeyId(&Crl, &AuthorityId) !=
				X509_VALUE) ||
			(!AuthorityId.HasKeyId) ||
			(AuthorityId.KeyId.Size == 0u) ||
			(xrtX509CrlDeltaBase(&Crl, &CrlNumber) !=
				X509_DONE) ||
			(xrtX509CrlIssuingPoint(&Crl, &NoPoint) !=
				X509_DONE) ||
			/* FreshestCRL 只允许出现在完整 CRL 中；
			 * 本夹具未携带 → DONE。 */
			(xrtX509CrlFreshest(&Crl, &NoPoints) !=
				X509_DONE) ) {
			goto Cleanup;
		}
		(void)KeyId;
	}
	printf("crl: profile number=4098 + aki + absent extensions ok\n");

	/* ---- 条目查询：序列号命中叶证书，带 reason 与 invalidity。 ---- */
	if ( (xrtX509CrlFind(&Crl, Leaf.Serial, &Entry) != X509_VALUE) ||
		(xrtX509CrlRevokes(&Crl, &Leaf, &Entry) != X509_VALUE) ) {
		goto Cleanup;
	}
	{
		xx509crlreason Reason;
		xtime Invalidity;

		if ( (xrtX509CrlEntryReason(&Entry, &Reason) !=
				X509_VALUE) ||
			(Reason != X509_CRL_REASON_KEY_COMPROMISE) ||
			(xrtX509CrlEntryInvalidityDate(&Entry,
				&Invalidity) != X509_VALUE) ) {
			goto Cleanup;
		}
	}
	printf("crl: find/revokes leaf -> entry with reason+invalidity ok\n");
	{
		/* 条目发行者缺失 + 独立 GeneralizedTime 解析 +
		 * 未吊销证书（CA 自身）→ DONE。 */
		xx509gencursor NoIssuer;
		xx509crlentry NoEntry;
		static const uint8 arrDate[17] = {
			0x18u, 0x0Fu, '2', '0', '2', '6', '0', '9',
			'0', '1', '0', '0', '0', '0', '0', '0', 'Z'
		};
		xtime Parsed;

		if ( (xrtX509CrlEntryIssuer(&Entry, &NoIssuer) !=
				X509_DONE) ||
			!xrtX509CrlInvalidityDateParse(
				(xbytesview) { arrDate, 17u }, &Parsed) ||
			(xrtX509CrlRevokes(&Crl, &Ca, &NoEntry) !=
				X509_DONE) ) {
			goto Cleanup;
		}
	}
	printf("crl: entry issuer absent + independent date parse ok\n");

	/* ---- 窗口与验证：当前时间在发布窗口内、CA 公钥/证书双验签、
	 * 严格档案校验输出可复用的 valid 视图。 ---- */
	if ( !xrtX509CrlValidAt(&Crl, xrtNow()) ||
		!xrtX509PublicKey(&Ca, &CaKey) ||
		!xrtX509CrlVerifyKey(&Crl, &CaKey) ||
		!xrtX509CrlVerify(&Crl, &Ca) ) {
		goto Cleanup;
	}
	xrtX509CrlConfigInit(&Config);
	if ( !xrtX509CrlValidate(&Crl, &Ca, &Config, &Valid) ) {
		goto Cleanup;
	}
	printf("crl: valid-at now + verify(key/cert) + validate ok\n");

	/* ---- 状态查询：叶证书已吊销；CA 未吊销 → VALUE + GOOD
	 *（GOOD 也是确定答案；DONE 表示 CRL 不适用该证书）。 ---- */
	if ( (xrtX509CrlStatus(&Crl, &Ca, &Leaf, &Config, &Revocation) !=
			X509_VALUE) ||
		(Revocation.State != X509_REVOCATION_REVOKED) ||
		(!Revocation.HasReason) ||
		(Revocation.Reason != X509_CRL_REASON_KEY_COMPROMISE) ||
		(!Revocation.HasInvalidityDate) ||
		(xrtX509CrlStatus(&Crl, &Ca, &Ca, &Config, &Revocation) !=
			X509_VALUE) ||
		(Revocation.State != X509_REVOCATION_GOOD) ) {
		goto Cleanup;
	}
	printf("crl: status revoked/good + validate reusable ok\n");

	/* ---- CRL 组合：SetInit 要求 complete + delta 两份已验证 CRL
	 *（NULL delta 是参数错误）；openssl 配置不产出
	 * deltaCRLIndicator，本例独立演示两条负路径：零结构 Check
	 * 被拒绝、缺少 delta 的 Init 被拒绝。Init 成功后才能使用 Set；
	 * 失败不会初始化输出，清零也不等于构造了有效组合。 ---- */
	memset(&Set, 0, sizeof(Set));
	if ( (xrtX509CrlSetCheck(&Set, &Leaf, &Revocation) !=
			X509_ERROR) ||
		xrtX509CrlSetInit(&Set, &Valid, NULL) ) {
		goto Cleanup;
	}
	printf("crl: set requires base+delta (null delta rejected) ok\n");
	iResult = 0;

Cleanup:
	return iResult;
}
