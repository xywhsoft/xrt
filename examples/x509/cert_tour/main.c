/*
 * 范例：x509/cert_tour —— 证书扩展/名称/匹配/验证全接口
 * ----------------------------------------------------------------
 * 演示 API：
 *   【扩展游标】  xrtX509ExtensionInit / Read（证书侧遍历）
 *                 xrtX509ExtensionListInit（裸 Extensions DER）
 *                 xrtX509ExtensionFind / ListFind（按 OID 查找）
 *   【策略扩展】  xrtX509KeyUsage / BasicConstraints /
 *                 ExtendedKeyUsage（OidInit + OidRead 逐项）
 *   【标识扩展】  xrtX509SubjectKeyId / SubjectKeyIdParse /
 *                 AuthorityKeyId / AuthorityKeyIdParse
 *   【名称扩展】  xrtX509SubjectAltName（SAN 四形态遍历）/
 *                 IssuerAltName（缺失 → DONE）/
 *                 NameConstraints（缺失 → DONE）/
 *                 FreshestCrl / CrlPoints（URI 分发点）/
 *                 DistributionPointParse / IssuingPointParse
 *   【标识扩展】  xrtX509SubjectKeyId / SubjectKeyIdParse /
 *   【名称匹配】  xrtX509NameFind / NameWithin /
 *                 GeneralNameWithin / IssuerMatch / MatchHost
 *   【时间与验证】xrtX509TimeParse / ValidAt /
 *                 SignatureVerify / CertificateVerifyKey
 * 模块宏：XRT_MODULE_X509
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/x509/cert_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   cert: extensions iterated (>=7) + oid find ok
 *   cert: keyusage/basicconstraints/eku ok
 *   cert: keyusage/basicconstraints/eku + ski~aki match ok
 *   cert: san 4 forms + altname/constraints absent ok
 *   cert: distribution points (crl + freshest) ok
 *   cert: name find/within/general-within/issuer-match ok
 *   cert: match-host localhost ok
 *   cert: timeparse/validat + signature verified ok
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>
#include "fixture.h"

/* keyUsage OID：2.5.29.15。 */
static const uint8 arrOidKeyUsage[3] = { 0x55u, 0x1Du, 0x0Fu };

/* 单元素 EKU：SEQUENCE { OID 1.3.6.1.5.5.7.3.1 (serverAuth) }。 */
static const uint8 arrEkuDer[12] = {
	0x30u, 0x0Au, 0x06u, 0x08u, 0x2Bu, 0x06u, 0x01u, 0x05u,
	0x05u, 0x07u, 0x03u, 0x01u
};

int main(void)
{
	xx509cert Leaf;
	xx509cert Ca;
	xx509extcursor ExtCursor;
	xx509ext Ext;
	xx509basicconstraints Constraints;
	xx509oidcursor OidCursor;
	xx509gencursor GenCursor;
	xx509genname GenName;
	xx509pubkey CaKey;
	xx509signature Scheme;
	xbytesview KeyId;
	xtime ParsedTime;
	uint16 iUsage = 0;
	size_t iExtCount = 0;
	size_t iEku = 0;
	size_t iSan = 0;
	int iDns = 0;
	int iIp = 0;
	int iEmail = 0;
	int iUri = 0;
	int iResult = 1;

	if ( !xrtX509Parse(exampleLeafDer, sizeof(exampleLeafDer), &Leaf) ||
		!xrtX509Parse(exampleCaDer, sizeof(exampleCaDer), &Ca) ) {
		goto Cleanup;
	}

	/* ---- 扩展游标：证书侧遍历 + 裸 DER 初始化 + OID 查找。 ---- */
	if ( !xrtX509ExtensionInit(&Leaf, &ExtCursor) ) {
		goto Cleanup;
	}
	while ( xrtX509ExtensionRead(&ExtCursor, &Ext) == X509_VALUE ) {
		++iExtCount;
	}
	if ( (iExtCount < 7u) ||
		!xrtX509ExtensionListInit(Leaf.Extensions, &ExtCursor) ||
		!xrtX509ExtensionFind(&Leaf, arrOidKeyUsage,
			sizeof(arrOidKeyUsage), &Ext) ||
		(!Ext.Critical) ||
		!xrtX509ExtensionListFind(Leaf.Extensions, arrOidKeyUsage,
			sizeof(arrOidKeyUsage), &Ext) ) {
		goto Cleanup;
	}
	printf("cert: extensions iterated (>=7) + oid find ok\n");

	/* ---- 策略扩展：KU 位、BC 非锚、EKU 两个 OID。 ---- */
	if ( (xrtX509KeyUsage(&Leaf, &iUsage) != X509_VALUE) ||
		((iUsage & X509_USAGE_DIGITAL_SIGNATURE) == 0u) ||
		((iUsage & X509_USAGE_KEY_ENCIPHERMENT) == 0u) ||
		(xrtX509BasicConstraints(&Leaf, &Constraints) != X509_VALUE) ||
		(Constraints.CA) ||
		(xrtX509BasicConstraints(&Ca, &Constraints) != X509_VALUE) ||
		(!Constraints.CA) ) {
		goto Cleanup;
	}
	if ( xrtX509ExtendedKeyUsage(&Leaf, &OidCursor) != X509_VALUE ) {
		goto Cleanup;
	}
	while ( xrtX509OidRead(&OidCursor, &KeyId) == X509_VALUE ) {
		++iEku;
	}
	if ( (iEku != 2u) ||
		/* OidInit：独立 SEQUENCE OF OID DER 初始化游标。 */
		!xrtX509OidInit((xbytesview) { arrEkuDer, 12u },
			&OidCursor) ||
		(xrtX509OidRead(&OidCursor, &KeyId) != X509_VALUE) ||
		(KeyId.Size != 8u) ||
		(memcmp(KeyId.Data, arrEkuDer + 4, 8u) != 0) ||
		(xrtX509OidRead(&OidCursor, &KeyId) != X509_DONE) ) {
		goto Cleanup;
	}
	printf("cert: keyusage/basicconstraints/eku ok\n");

	/* ---- 标识扩展：叶 SKI == CA SKI（经 AKI keyid 关联）。 ---- */
	{
		/* subjectKeyIdentifier OID：2.5.29.14。 */
		static const uint8 arrOidSki[3] = { 0x55u, 0x1Du, 0x0Eu };
		xx509authoritykeyid AuthorityId;
		xbytesview LeafSki;
		xbytesview ParsedSki;

		if ( (xrtX509SubjectKeyId(&Leaf, &LeafSki) != X509_VALUE) ||
			/* 独立 DER 形态：SKI 扩展值是 OCTET STRING 包裹。 */
			!xrtX509ExtensionFind(&Leaf, arrOidSki,
				sizeof(arrOidSki), &Ext) ||
			!xrtX509SubjectKeyIdParse(Ext.Value, &ParsedSki) ||
			(ParsedSki.Size != LeafSki.Size) ||
			(memcmp(ParsedSki.Data, LeafSki.Data,
				ParsedSki.Size) != 0) ||
			(xrtX509AuthorityKeyId(&Leaf, &AuthorityId) !=
				X509_VALUE) ||
			(!AuthorityId.HasKeyId) ||
			(AuthorityId.KeyId.Size == 0u) ) {
			goto Cleanup;
		}
		/* CA 的 SKI 与叶 AKI keyid 相同（签发关系）。 */
		if ( (xrtX509SubjectKeyId(&Ca, &KeyId) != X509_VALUE) ||
			(KeyId.Size != AuthorityId.KeyId.Size) ||
			(memcmp(KeyId.Data, AuthorityId.KeyId.Data,
				KeyId.Size) != 0) ) {
			goto Cleanup;
		}
		/* AKI 独立 DER 形态：SEQUENCE 直接解析。 */
		{
			static const uint8 arrOidAki[3] =
				{ 0x55u, 0x1Du, 0x23u };
			xx509authoritykeyid ParsedAki;

			if ( !xrtX509ExtensionFind(&Leaf, arrOidAki,
					sizeof(arrOidAki), &Ext) ||
				!xrtX509AuthorityKeyIdParse(Ext.Value,
					&ParsedAki) ||
				(!ParsedAki.HasKeyId) ||
				(ParsedAki.KeyId.Size !=
					AuthorityId.KeyId.Size) ) {
				goto Cleanup;
			}
		}
	}
	printf("cert: keyusage/basicconstraints/eku + ski~aki match ok\n");

	/* ---- SAN 四形态 + 两个可能缺失的扩展 → DONE。 ---- */
	if ( xrtX509SubjectAltName(&Leaf, &GenCursor) != X509_VALUE ) {
		goto Cleanup;
	}
	while ( xrtX509GeneralNameRead(&GenCursor, &GenName) ==
		X509_VALUE ) {
		switch ( GenName.Type ) {
			case X509_NAME_DNS:
				++iDns;
				break;
			case X509_NAME_IP:
				++iIp;
				break;
			case X509_NAME_EMAIL:
				++iEmail;
				break;
			case X509_NAME_URI:
				++iUri;
				break;
			default:
				break;
		}
		++iSan;
	}
	{
		xx509nameconstraints NoConstraints;

		if ( (iSan != 4u) || (iDns != 1) || (iIp != 1) ||
			(iEmail != 1) || (iUri != 1) ||
			(xrtX509IssuerAltName(&Leaf, &GenCursor) !=
				X509_DONE) ||
			(xrtX509NameConstraints(&Leaf, &NoConstraints) !=
				X509_DONE) ) {
			goto Cleanup;
		}
		/* NameConstraintsCheck：手构 permitted/excluded 树——
		 * GeneralSubtrees 隐式内容 = GeneralSubtree TLV 连接，
		 * 每棵树是 SEQUENCE { base GeneralName }。
		 * 无任何树的约束对象是参数错误（两标志全假）。 */
		{
			/* permitted: DNS "localhost"；excluded: DNS "evil.example"。 */
			static const uint8 arrPermitted[13] = {
				0x30u, 0x0Bu, 0x82u, 0x09u,
				'l', 'o', 'c', 'a', 'l', 'h', 'o', 's', 't'
			};
			static const uint8 arrExcluded[16] = {
				0x30u, 0x0Eu, 0x82u, 0x0Cu,
				'e', 'v', 'i', 'l', '.', 'e', 'x', 'a',
				'm', 'p', 'l', 'e'
			};

			memset(&NoConstraints, 0, sizeof(NoConstraints));
			NoConstraints.HasPermitted = true;
			NoConstraints.Permitted.Items.Data = arrPermitted;
			NoConstraints.Permitted.Items.Size = 13u;
			NoConstraints.HasExcluded = true;
			NoConstraints.Excluded.Items.Data = arrExcluded;
			NoConstraints.Excluded.Items.Size = 16u;
			/* DNS localhost 在 permitted 内、不在 excluded 内。 */
			if ( !xrtX509NameConstraintsCheck(&NoConstraints,
					&Leaf) ) {
				goto Cleanup;
			}
			/* 把 permitted 换成别的域 → SAN 的 localhost 被拒。 */
			NoConstraints.Permitted.Items.Data = arrExcluded;
			NoConstraints.Permitted.Items.Size = 16u;
			if ( xrtX509NameConstraintsCheck(&NoConstraints,
					&Leaf) ) {
				goto Cleanup;
			}
		}
	}
	printf("cert: san 4 forms + altname/constraints absent ok\n");

	/* ---- 分发点：CRLDP 与 FreshestCRL 各一个 URI 全名。 ---- */
	{
		xx509distributioncursor DistCursor;
		xx509distributionpoint Point;
		xbytesview Oid;

		/* 找到 CRLDistributionPoints 扩展后用裸 DER 游标遍历。 */
		if ( (xrtX509CrlPoints(&Leaf, &DistCursor) != X509_VALUE) ||
			(xrtX509DistributionRead(&DistCursor, &Point) !=
				X509_VALUE) ||
			/* 独立 DER 形态：单个 DistributionPoint TLV。 */
			!xrtX509DistributionPointParse(Point.Raw, &Point) ||
			(xrtX509FreshestCrl(&Leaf, &DistCursor) !=
				X509_VALUE) ||
			(xrtX509DistributionRead(&DistCursor, &Point) !=
				X509_VALUE) ) {
				goto Cleanup;
			}
			/* 独立 DER 形态：最小合法 IDP——
			 * SEQUENCE { [0] EXPLICIT { fullName [0] IMPLICIT
			 * [URI] } }；空序列被拒（至少需要一个字段）。 */
			{
				static const uint8 arrIdp[9] = {
					0x30u, 0x07u, 0xA0u, 0x05u, 0xA0u, 0x03u,
					0x86u, 0x01u, 0x61u
				};
				xx509issuingpoint Issuing;

				if ( !xrtX509IssuingPointParse(
						(xbytesview) { arrIdp, 9u },
						&Issuing) ||
					(!Issuing.HasDistributionPoint) ) {
					goto Cleanup;
				}
			}
			(void)Oid;
		}
		printf("cert: distribution points (crl + freshest) ok\n");

	/* ---- 名称匹配：CN 属性 + 子树包含 + 发行者匹配。 ---- */
	{
		static const uint8 arrOidCn[3] = { 0x55u, 0x04u, 0x03u };
		xx509namecursor NameCursor;
		xx509nameattr Attr;

		if ( !xrtX509NameFind(Leaf.Subject, arrOidCn,
				sizeof(arrOidCn), &Attr) ||
			(Attr.Value.Size != 9u) ||
			(memcmp(Attr.Value.Data, "localhost", 9u) != 0) ||
			(xrtX509NameWithin(Ca.Subject, Ca.Subject) !=
				X509_VALUE) ||
			(xrtX509NameWithin(Leaf.Subject, Ca.Subject) ==
				X509_ERROR) ||
			(xrtX509GeneralNameWithin(
				&(xx509genname) {
					X509_NAME_DNS,
					{ (const uint8*)"localhost", 9u },
					{ (const uint8*)"localhost", 9u }
				},
				&(xx509genname) {
					X509_NAME_DNS,
					{ (const uint8*)"localhost", 9u },
					{ (const uint8*)"localhost", 9u }
				}) != X509_VALUE) ) {
			goto Cleanup;
		}
		(void)NameCursor;
		/* IssuerMatch：叶的发行者名称与 CA 匹配（不做签名）。 */
		if ( xrtX509IssuerMatch(&Leaf, &Ca) !=
			X509_VALUE ) {
			goto Cleanup;
		}
	}
	printf("cert: name find/within/general-within/issuer-match ok\n");

	/* ---- MatchHost：SAN 的 DNS-ID 命中 localhost（pName 可空）。 ---- */
	if ( xrtX509MatchHost(&Leaf,
			(xstrview) { "localhost", 9u }, NULL) !=
		X509_VALUE ) {
		goto Cleanup;
	}
	printf("cert: match-host localhost ok\n");

	/* ---- 时间与签名：TimeParse 独立 DER、ValidAt、验签。 ---- */
	{
		/* GeneralizedTime 2026-09-01T00:00:00Z。 */
		static const uint8 arrTimeDer[17] = {
			0x18u, 0x0Fu, '2', '0', '2', '6', '0', '9', '0', '1',
			'0', '0', '0', '0', '0', '0', 'Z'
		};

		if ( !xrtX509TimeParse((xbytesview) { arrTimeDer, 17u },
				&ParsedTime) ||
			!xrtX509ValidAt(&Leaf, xrtNow()) ||
			xrtX509ValidAt(&Leaf, ParsedTime) ||
			!xrtX509PublicKey(&Ca, &CaKey) ||
			(xrtX509SignatureParse(&Leaf.SignatureAlgorithm,
				&Scheme) != X509_VALUE) ||
			!xrtX509CertificateVerifyKey(&Leaf, &CaKey) ||
			!xrtX509SignatureVerify(&Scheme, Leaf.Tbs,
				Leaf.Signature, &CaKey) ) {
			goto Cleanup;
		}
	}
	printf("cert: timeparse/validat + signature verified ok\n");
	iResult = 0;

Cleanup:
	return iResult;
}
