#include "../test.h"
#include "../fixtures/x509_path_legacy.h"



/* 初始化旧版 TLS 测试留下的真实 RSA 证书链。 */
static void testX509PathBuildRsaFixture(
	xx509cert* pLeaf,
	xx509cert* pIntermediate,
	xx509cert* pRoot,
	xx509anchor* pAnchor,
	xx509pathconfig* pConfig
)
{
	static const uint8 ServerAuthOid[] = {
		0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01
	};

	testRequire(xrtX509Parse(
		X509_PATH_LEAF, sizeof(X509_PATH_LEAF), pLeaf
	) && xrtX509Parse(
		X509_PATH_INTERMEDIATE, sizeof(X509_PATH_INTERMEDIATE), pIntermediate
	) && xrtX509Parse(
		X509_PATH_ROOT, sizeof(X509_PATH_ROOT), pRoot
	) && xrtX509Anchor(pRoot, pAnchor),
		"X.509 RSA path builder fixture initialization failed");
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Time = pLeaf->NotBefore;
	pConfig->Flags = X509_PATH_REQUIRE_KEY_USAGE |
		X509_PATH_REQUIRE_PURPOSE;
	pConfig->KeyUsage = X509_USAGE_DIGITAL_SIGNATURE;
	pConfig->Purpose = (xbytesview) {
		ServerAuthOid, sizeof(ServerAuthOid)
	};
}



/* 验证打乱候选顺序且混入根证书时仍返回不含信任锚的有效路径。 */
static void testX509PathBuildRsaChain(void)
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

	testX509PathBuildRsaFixture(
		&Leaf, &Intermediate, &Root, &Anchor, &Config
	);
	Issuers[0] = &Root;
	Issuers[1] = &Intermediate;
	Source.Issuers = Issuers;
	Source.IssuerCount = 2u;
	Source.Anchors = &Anchor;
	Source.AnchorCount = 1u;
	testRequire(xrtX509PathBuild(
		&Leaf, &Source, &Config, Path, 3u, &Result
	) && (Result.Count == 2u) && (Result.Anchor == &Anchor) &&
		(Path[0] == &Leaf) && (Path[1] == &Intermediate),
		"X.509 RSA builder included the presented root in the path");
}



/* 验证同名错误公钥分支失败后会继续搜索后续有效发行者。 */
static void testX509PathBuildRsaBacktrack(void)
{
	static const uint8 WrongRaw = 1;
	xx509cert Leaf;
	xx509cert Intermediate;
	xx509cert Root;
	xx509cert Wrong;
	xx509anchor Anchor;
	xx509pathsource Source;
	xx509pathconfig Config;
	xx509pathresult Result;
	const xx509cert* Issuers[2];
	const xx509cert* Path[3];

	testX509PathBuildRsaFixture(
		&Leaf, &Intermediate, &Root, &Anchor, &Config
	);
	Leaf.Extensions = (xbytesview) { NULL, 0 };
	Config.Flags = 0;
	Config.KeyUsage = 0;
	Config.Purpose = (xbytesview) { NULL, 0 };
	Wrong = Intermediate;
	Wrong.Raw = (xbytesview) { &WrongRaw, 1u };
	Wrong.SubjectPublicKeyInfo = Root.SubjectPublicKeyInfo;
	Issuers[0] = &Wrong;
	Issuers[1] = &Intermediate;
	Source.Issuers = Issuers;
	Source.IssuerCount = 2u;
	Source.Anchors = &Anchor;
	Source.AnchorCount = 1u;
	testRequire(xrtX509PathBuild(
		&Leaf, &Source, &Config, Path, 3u, &Result
	) && (Result.Count == 2u) && (Result.Anchor == &Anchor) &&
		(Path[1] == &Intermediate),
		"X.509 RSA builder did not backtrack from a wrong public key");
}



/* 验证真实链在输出容量不足时给出稳定的建链范围错误。 */
static void testX509PathBuildRsaCapacity(void)
{
	xx509cert Leaf;
	xx509cert Intermediate;
	xx509cert Root;
	xx509anchor Anchor;
	xx509pathsource Source;
	xx509pathconfig Config;
	xx509pathresult Result;
	const xx509cert* Issuers[1];
	const xx509cert* Path[1];

	testX509PathBuildRsaFixture(
		&Leaf, &Intermediate, &Root, &Anchor, &Config
	);
	Issuers[0] = &Intermediate;
	Source.Issuers = Issuers;
	Source.IssuerCount = 1u;
	Source.Anchors = &Anchor;
	Source.AnchorCount = 1u;
	testRequire(!xrtX509PathBuild(
		&Leaf, &Source, &Config, Path, 1u, &Result
	) && (xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_PATH_BUILD),
		"X.509 RSA builder accepted an undersized path buffer");
}



/* 执行真实 RSA 自动建链、回溯和容量边界测试。 */
int main(void)
{
	testX509PathBuildRsaChain();
	testX509PathBuildRsaBacktrack();
	testX509PathBuildRsaCapacity();
	printf("[PASS] x509_path_build_rsa\n");
	return 0;
}
