#include <xrt/acme_store.h>

#if defined(XACME_FEATURE_ACME_STORE)

#include <xrt/charset.h>
#include <xrt/crypto.h>
#include <xrt/file.h>
#include <xrt/pem.h>
#include <xrt/x509.h>

#include <stdio.h>
#include <string.h>

static void xacmeStoreError(xerrkind Kind, xacmestoreerror Code, cstr s)
{
	xrtSetErrorInfo(Kind, "xrt.acme.store", (int32)Code, s);
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
	return xrtFileWriteAtomic(
		sPath, (xbytesview){ (const uint8*)sText, strlen(sText) });
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
	if(!xacmeStoreWriteAtomicText(sPath, sAccountPem))
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
		xacmeStoreError(
			XERR_NOT_FOUND, XACME_STORE_ERROR_NOT_FOUND,
			"acme store cert not found");
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
