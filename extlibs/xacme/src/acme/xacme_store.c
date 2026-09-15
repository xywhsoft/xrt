#include <xrt/acme_store.h>

#if defined(XACME_FEATURE_ACME_STORE)

#include <xrt/charset.h>
#include <xrt/crypto.h>
#include <xrt/file.h>
#include <xrt/pem.h>
#include <xrt/x509.h>

#include <stdio.h>
#include <string.h>

#if !defined(_WIN32)
	#include <sys/stat.h>
#endif

static void xacmeStoreError(xerrkind Kind, xacmestoreerror Code, cstr s)
{
	xrtSetErrorInfo(Kind, "xrt.acme.store", (int32)Code, s);
}

/*
	私钥文件收紧为仅属主可读写（POSIX 0600；Windows 无对应位，跳过）。
	chmod 失败不阻断保存——返回值供诊断，但缺省权限已由 umask 保证
	不宽于 0666，此处是收紧而非放开。
*/
static void xacmeStoreKeyMode(cstr sPath)
{
#if !defined(_WIN32)
	(void)chmod(sPath, 0600);
#else
	(void)sPath;
#endif
}

/* directory URL → 16 字符十六进制目录名。 */
static bool xacmeStoreCaDir(
	cstr sDirectoryUrl, char* sOut /* >= 17 */)
{
	uint8 Digest[XRT_SHA256_SIZE];
	size_t i;
	if(!xrtSha256(sDirectoryUrl, strlen(sDirectoryUrl), Digest))
	{
		return false;
	}
	for(i = 0; i < 8u; i++)
	{
		sprintf(sOut + i * 2u, "%02x", Digest[i]);
	}
	sOut[16] = '\0';
	return true;
}

static bool xacmeStoreWriteAtomicText(cstr sPath, cstr sText)
{
	if(!xrtFileWriteAtomic(
			sPath, (xbytesview){ (const uint8*)sText, strlen(sText) }))
	{
		return false;
	}
	return true;
}

/* 私钥 PEM 原子写 + 权限收紧。 */
static bool xacmeStoreWriteAtomicKey(cstr sPath, cstr sText)
{
	if(!xacmeStoreWriteAtomicText(sPath, sText))
	{
		return false;
	}
	xacmeStoreKeyMode(sPath);
	return true;
}

bool xrtAcmeStoreSaveAccount(
	cstr sRoot, cstr sDirectoryUrl, cstr sAccountPem)
{
	char sCa[17];
	char sPath[1024];
	if((sRoot == NULL) || (sDirectoryUrl == NULL) || (sAccountPem == NULL))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store save account requires root, url and pem");
		return false;
	}
	if(!xacmeStoreCaDir(sDirectoryUrl, sCa))
	{
		return false;
	}
	snprintf(
		sPath, sizeof(sPath), "%s/accounts/%s", sRoot, sCa);
	if(!xrtDirCreateAll(sPath))
	{
		xacmeStoreError(
			XERR_IO, XACME_STORE_ERROR_IO, "acme store mkdir failed");
		return false;
	}
	snprintf(sPath, sizeof(sPath), "%s/accounts/%s/account.pem", sRoot, sCa);
	if(!xacmeStoreWriteAtomicKey(sPath, sAccountPem))
	{
		xacmeStoreError(
			XERR_IO, XACME_STORE_ERROR_IO,
			"acme store write account failed");
		return false;
	}
	return true;
}

str xrtAcmeStoreLoadAccount(cstr sRoot, cstr sDirectoryUrl)
{
	char sCa[17];
	char sPath[1024];
	size_t iSize = 0u;
	bytes pBytes;
	str sPem;
	if((sRoot == NULL) || (sDirectoryUrl == NULL))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store load account requires root and url");
		return NULL;
	}
	if(!xacmeStoreCaDir(sDirectoryUrl, sCa))
	{
		return NULL;
	}
	snprintf(
		sPath, sizeof(sPath), "%s/accounts/%s/account.pem", sRoot, sCa);
	pBytes = xrtFileReadAll(sPath, &iSize);
	if(pBytes == NULL)
	{
		xacmeStoreError(
			XERR_NOT_FOUND, XACME_STORE_ERROR_NOT_FOUND,
			"acme store account not found");
		return NULL;
	}
	sPem = (str)xrtMalloc(iSize + 1u);
	if(sPem != NULL)
	{
		memcpy(sPem, pBytes, iSize);
		sPem[iSize] = '\0';
	}
	xrtFree(pBytes);
	return sPem;
}

bool xrtAcmeStoreSaveCert(
	cstr sRoot, cstr sPrimaryDomain, cstr sChainPem, cstr sDirectoryUrl)
{
	char sPath[1024];
	char sMeta[1200];
	if((sRoot == NULL) || (sPrimaryDomain == NULL) || (sChainPem == NULL))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store save cert requires root, domain and chain");
		return false;
	}
	snprintf(sPath, sizeof(sPath), "%s/certs/%s", sRoot, sPrimaryDomain);
	if(!xrtDirCreateAll(sPath))
	{
		xacmeStoreError(
			XERR_IO, XACME_STORE_ERROR_IO, "acme store mkdir failed");
		return false;
	}
	snprintf(
		sPath, sizeof(sPath), "%s/certs/%s/fullchain.pem", sRoot,
		sPrimaryDomain);
	if(!xacmeStoreWriteAtomicText(sPath, sChainPem))
	{
		xacmeStoreError(
			XERR_IO, XACME_STORE_ERROR_IO,
			"acme store write chain failed");
		return false;
	}
	snprintf(
		sMeta, sizeof(sMeta), "directory=%s\n",
		(sDirectoryUrl != NULL) ? sDirectoryUrl : "");
	snprintf(
		sPath, sizeof(sPath), "%s/certs/%s/meta.txt", sRoot, sPrimaryDomain);
	if(!xacmeStoreWriteAtomicText(sPath, sMeta))
	{
		xacmeStoreError(
			XERR_IO, XACME_STORE_ERROR_IO, "acme store write meta failed");
		return false;
	}
	return true;
}

str xrtAcmeStoreLoadCert(cstr sRoot, cstr sPrimaryDomain)
{
	char sPath[1024];
	size_t iSize = 0u;
	bytes pBytes;
	str sPem;
	if((sRoot == NULL) || (sPrimaryDomain == NULL))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store load cert requires root and domain");
		return NULL;
	}
	snprintf(
		sPath, sizeof(sPath), "%s/certs/%s/fullchain.pem", sRoot,
		sPrimaryDomain);
	pBytes = xrtFileReadAll(sPath, &iSize);
	if(pBytes == NULL)
	{
		/* 只有文件确实缺失才归类 NOT_FOUND；
		   读取/分配失败保留底层根因（IO/MEMORY），
		   续签判定不得把读故障吞成"缺证书"。 */
		if(!xrtFileExists(sPath))
		{
			xacmeStoreError(
				XERR_NOT_FOUND, XACME_STORE_ERROR_NOT_FOUND,
				"acme store cert not found");
		}
		return NULL;
	}
	sPem = (str)xrtMalloc(iSize + 1u);
	if(sPem != NULL)
	{
		memcpy(sPem, pBytes, iSize);
		sPem[iSize] = '\0';
	}
	xrtFree(pBytes);
	return sPem;
}

str xrtAcmeStoreLoadCertCa(cstr sRoot, cstr sPrimaryDomain)
{
	char sPath[1024];
	size_t iSize = 0u;
	bytes pBytes;
	str sCa = NULL;
	if((sRoot == NULL) || (sPrimaryDomain == NULL))
	{
		return NULL;
	}
	snprintf(
		sPath, sizeof(sPath), "%s/certs/%s/meta.txt", sRoot, sPrimaryDomain);
	pBytes = xrtFileReadAll(sPath, &iSize);
	if((pBytes == NULL) || (iSize <= 11u) ||
		(memcmp(pBytes, "directory=", 10u) != 0))
	{
		xrtFree(pBytes);
		return NULL;
	}
	/* 去掉末尾换行。 */
	while((iSize > 0u) &&
		((pBytes[iSize - 1u] == '\n') || (pBytes[iSize - 1u] == '\r')))
	{
		iSize--;
	}
	sCa = (str)xrtMalloc(iSize - 10u + 1u);
	if(sCa != NULL)
	{
		memcpy(sCa, pBytes + 10u, iSize - 10u);
		sCa[iSize - 10u] = '\0';
	}
	xrtFree(pBytes);
	return sCa;
}

bool xrtAcmeStoreSaveGrant(
	cstr sRoot, cstr sPrimaryDomain, const xacmeissuegrant* pGrant,
	cstr sDirectoryUrl)
{
	char sPath[1024];
	if((sRoot == NULL) || (sPrimaryDomain == NULL) || (pGrant == NULL) ||
		(pGrant->sFullchainPem == NULL) || (pGrant->sKeyPem == NULL))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store save grant requires root, domain, chain and key");
		return false;
	}
	/* 链与 meta 先落（含建目录），再写 key.pem。 */
	if(!xrtAcmeStoreSaveCert(
			sRoot, sPrimaryDomain, pGrant->sFullchainPem, sDirectoryUrl))
	{
		return false;
	}
	snprintf(
		sPath, sizeof(sPath), "%s/certs/%s/key.pem", sRoot, sPrimaryDomain);
	if(!xacmeStoreWriteAtomicKey(sPath, pGrant->sKeyPem))
	{
		xacmeStoreError(
			XERR_IO, XACME_STORE_ERROR_IO,
			"acme store write key failed");
		return false;
	}
	return true;
}

bool xrtAcmeStoreLoadGrant(
	cstr sRoot, cstr sPrimaryDomain, xacmeissuegrant* pOut)
{
	char sPath[1024];
	size_t iSize = 0u;
	bytes pBytes;
	if((sRoot == NULL) || (sPrimaryDomain == NULL) || (pOut == NULL))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store load grant requires root, domain and output");
		return false;
	}
	memset(pOut, 0, sizeof(*pOut));
	pOut->sFullchainPem = xrtAcmeStoreLoadCert(sRoot, sPrimaryDomain);
	if(pOut->sFullchainPem == NULL)
	{
		return false;
	}
	snprintf(
		sPath, sizeof(sPath), "%s/certs/%s/key.pem", sRoot, sPrimaryDomain);
	pBytes = xrtFileReadAll(sPath, &iSize);
	if(pBytes == NULL)
	{
		xacmeStoreError(
			XERR_NOT_FOUND, XACME_STORE_ERROR_NOT_FOUND,
			"acme store key not found");
		xrtAcmeGrantUnit(pOut);
		return false;
	}
	pOut->sKeyPem = (str)xrtMalloc(iSize + 1u);
	if(pOut->sKeyPem == NULL)
	{
		xrtFree(pBytes);
		xrtAcmeGrantUnit(pOut);
		return false;
	}
	memcpy(pOut->sKeyPem, pBytes, iSize);
	pOut->sKeyPem[iSize] = '\0';
	xrtFree(pBytes);
	return true;
}

bool xrtAcmeStoreListDomains(
	cstr sRoot, char (*sOutDomains)[256],
	size_t iCapacity, size_t* pOutCount)
{
	char sPath[1024];
	xdir Dir;
	if((sRoot == NULL) || (sOutDomains == NULL) || (pOutCount == NULL) ||
		(iCapacity == 0u))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store list requires root, output and capacity");
		return false;
	}
	*pOutCount = 0u;
	snprintf(sPath, sizeof(sPath), "%s/certs", sRoot);
	Dir = xrtDirOpen(sPath, 0u);
	if(!Dir)
	{
		/* 目录不存在视为空清单（首次运行前）。 */
		xacmeStoreError(
			XERR_NOT_FOUND, XACME_STORE_ERROR_NOT_FOUND,
			"acme store certs dir not found");
		return false;
	}
	for(;;)
	{
		xdirentry Entry;
		xdirnext eNext = xrtDirNext(Dir, &Entry);
		if(eNext == XDIR_NEXT_END)
		{
			break;
		}
		if(eNext != XDIR_NEXT_ITEM)
		{
			xrtDirClose(Dir);
			xacmeStoreError(
				XERR_IO, XACME_STORE_ERROR_IO,
				"acme store list iterate failed");
			return false;
		}
		if((Entry.Name.Size == 0u) || (Entry.Name.Size >= 256u))
		{
			continue;
		}
		if(*pOutCount >= iCapacity)
		{
			xrtDirClose(Dir);
			xacmeStoreError(
				XERR_RANGE, XACME_STORE_ERROR_ARGUMENT,
				"acme store list capacity exhausted");
			return false;
		}
		memcpy(sOutDomains[*pOutCount], Entry.Name.Data, Entry.Name.Size);
		sOutDomains[*pOutCount][Entry.Name.Size] = '\0';
		(*pOutCount)++;
	}
	xrtDirClose(Dir);
	return true;
}

bool xrtAcmeStoreNeedRenew(
	cstr sRoot, cstr sPrimaryDomain, int iRenewalDays, bool* pbNeed)
{
	str sChain;
	xpemcursor Pem;
	xpemblock Block;
	size_t iDerSize = 0u;
	bytes pDer;
	xx509cert Cert;
	xtime Deadline;
	bool bNeed = true;
	bool bOk = false;

	if((sRoot == NULL) || (sPrimaryDomain == NULL) || (pbNeed == NULL) ||
		(iRenewalDays < 0))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store need renew requires root, domain and days");
		return false;
	}
	*pbNeed = true;

	sChain = xrtAcmeStoreLoadCert(sRoot, sPrimaryDomain);
	if(sChain == NULL)
	{
		if(xrtErrorKind(xrtGetError()) != XERR_NOT_FOUND)
		{
			/* 读故障不是"缺证书"：如实失败，避免误触重签。 */
			return false;
		}
		return true; /* 缺证书即需要签发。 */
	}
	if(!xrtPemInit(&Pem, sChain, strlen(sChain)) ||
		(xrtPemRead(&Pem, &Block) != XPEM_BLOCK))
	{
		xacmeStoreError(
			XERR_PROTOCOL, XACME_STORE_ERROR_PARSE,
			"acme store chain has no pem block");
		goto Done;
	}
	pDer = xrtPemDecodeNew(&Block, &iDerSize);
	if(pDer == NULL)
	{
		goto Done;
	}
	if(!xrtX509Parse(pDer, iDerSize, &Cert))
	{
		xrtFree(pDer);
		xacmeStoreError(
			XERR_PROTOCOL, XACME_STORE_ERROR_PARSE,
			"acme store leaf cert parse failed");
		goto Done;
	}
	xrtFree(pDer);
	if(!xrtTimeAdd(
			xrtNow(), (int64)iRenewalDays, XTIME_UNIT_DAY, &Deadline))
	{
		goto Done;
	}
	/* notAfter 早于续签线 → 需要续。 */
	bNeed = (Cert.NotAfter < Deadline);
	bOk = true;

Done:
	xrtFree(sChain);
	if(bOk)
	{
		*pbNeed = bNeed;
	}
	return bOk;
}

#endif
