#define XRT_IMPLEMENTATION
#include "../../singlehead/xrt.h"

#define XSPIDER_HTML_IMPLEMENTATION
#include "src/xspider_html.h"

#define XSPIDER_IMPLEMENTATION
#include "src/xspider.h"

/*
	XSpider Demo
	XSpider 演示程序

	目标 / Goal:
		1. 展示 xrt 的线程、协程、HTTP、URL、字典、文件等能力
		2. 证明可以用 xrt 快速开发一个结构清晰的并发爬虫

	用法 / Usage:
		xspider <root_url> [max_depth] [max_pages] [threads] [coroutines_per_thread] [options]
*/

typedef struct {
	xspider* pSpider;
	xmutex pFileLock;
	char sOutputDir[XSPIDER_OUTPUT_CAP];
	char sPagesDir[XSPIDER_OUTPUT_CAP];
	char sManifestPath[XSPIDER_OUTPUT_CAP];
} xspider_demo_state;


static void procDemoPrintHeader(void)
{
	printf("========================================\n");
	printf("XSpider - XRT Concurrent Website Crawler\n");
	printf("XSpider - 基于 XRT 的并发网站爬虫\n");
	printf("========================================\n\n");
}


static void procDemoPrintUsage(const char* sExe)
{
	printf("Usage: %s <root_url> [max_depth] [max_pages] [threads] [coroutines_per_thread] [options]\n", sExe);
	printf("用法:  %s <root_url> [max_depth] [max_pages] [threads] [coroutines_per_thread] [options]\n", sExe);
	printf("\n");
	printf("Options / 选项:\n");
	printf("  --insecure         Disable TLS certificate verification / 关闭 TLS 证书校验\n");
	printf("  --strict-tls       Enable TLS certificate verification / 启用 TLS 证书校验\n");
	printf("  --delay-ms <n>     Global politeness delay in ms / 全局礼貌延迟毫秒数\n");
	printf("  --subdomains       Include subdomains / 包含子域名\n");
	printf("  --no-subdomains    Exclude subdomains / 不包含子域名\n");
	printf("\n");
	printf("Example: %s https://xxrpa.com 4 512 4 12 --insecure --delay-ms 0\n", sExe);
}


static void procDemoStateUnit(xspider_demo_state* pState)
{
	if ( pState == NULL ) {
		return;
	}

	if ( pState->pFileLock != NULL ) {
		xrtMutexDestroy(pState->pFileLock);
		pState->pFileLock = NULL;
	}
}


static bool procDemoStateInit(xspider_demo_state* pState, const char* sOutputDir)
{
	const char* sBaseDir = sOutputDir != NULL ? sOutputDir : "./output";
	char sHeader[256];

	memset(pState, 0, sizeof(*pState));
	pState->pFileLock = xrtMutexCreate();
	if ( pState->pFileLock == NULL ) {
		return false;
	}

	snprintf(pState->sOutputDir, sizeof(pState->sOutputDir), "%s", sBaseDir);
	snprintf(pState->sPagesDir, sizeof(pState->sPagesDir), "%s/pages", sBaseDir);
	snprintf(pState->sManifestPath, sizeof(pState->sManifestPath), "%s/manifest.tsv", sBaseDir);

	if ( xrtDirExists(pState->sOutputDir) ) {
		xrtDirDelete(pState->sOutputDir);
	}
	xrtDirCreateAll(pState->sPagesDir);

	snprintf(sHeader, sizeof(sHeader), "url\tdepth\tstatus\tlinks\tfile\n");
	xrtFileWriteAll(pState->sManifestPath, sHeader, strlen(sHeader), XRT_CP_UTF8);
	return true;
}


static const char* procDemoFileExtension(const xhttpresponse* pResp)
{
	const char* sType;

	if ( pResp == NULL ) {
		return "txt";
	}

	sType = xrtHttpResponseHeader(pResp, "Content-Type");
	if ( sType == NULL ) {
		return "txt";
	}

	if ( procXSpiderContainsNoCase(sType, "text/html") || procXSpiderContainsNoCase(sType, "application/xhtml+xml") ) {
		return "html";
	}
	if ( procXSpiderContainsNoCase(sType, "application/json") ) {
		return "json";
	}
	if ( procXSpiderContainsNoCase(sType, "text/plain") ) {
		return "txt";
	}

	return "bin";
}


static void procDemoAppendManifest(
	xspider_demo_state* pState,
	const xspider_page* pPage,
	const char* sSavedFile
)
{
	FILE* pFile;

	if ( pState == NULL || pPage == NULL || sSavedFile == NULL ) {
		return;
	}

	xrtMutexLock(pState->pFileLock);
	pFile = fopen(pState->sManifestPath, "ab");
	if ( pFile != NULL ) {
		fprintf(
			pFile,
			"%s\t%d\t%u\t%d\t%s\n",
			pPage->sUrl,
			pPage->iDepth,
			pPage->pResponse ? pPage->pResponse->iStatusCode : 0u,
			pPage->iDiscoveredLinkCount,
			sSavedFile
		);
		fclose(pFile);
	}
	xrtMutexUnlock(pState->pFileLock);
}


static bool procDemoShouldVisit(const char* sUrl, const char* sReferer, int iDepth, ptr pUserData)
{
	(void)sReferer;
	(void)pUserData;

	if ( strstr(sUrl, "?download=") != NULL ) {
		return false;
	}

	return iDepth >= 0;
}


static void procDemoOnDiscover(const char* sUrl, const char* sReferer, int iDepth, ptr pUserData)
{
	(void)pUserData;

	printf(
		"[discover] depth=%d url=%s referer=%s\n",
		iDepth,
		sUrl,
		sReferer != NULL ? sReferer : "(seed)"
	);
}


static void procDemoOnPage(const xspider_page* pPage, ptr pUserData)
{
	xspider_demo_state* pState = (xspider_demo_state*)pUserData;
	char sSavedFile[XSPIDER_OUTPUT_CAP];
	uint64 iHash;
	const char* sExt;

	if ( pPage == NULL || pPage->pResponse == NULL || pState == NULL ) {
		return;
	}

	printf(
		"[page] status=%u depth=%d links=%d url=%s\n",
		pPage->pResponse->iStatusCode,
		pPage->iDepth,
		pPage->iDiscoveredLinkCount,
		pPage->sUrl
	);

	if ( pPage->pResponse->pBody == NULL || pPage->pResponse->iBodyLen == 0 ) {
		return;
	}

	sExt = procDemoFileExtension(pPage->pResponse);
	iHash = xrtHash64((ptr)pPage->sUrl, strlen(pPage->sUrl));
	snprintf(
		sSavedFile,
		sizeof(sSavedFile),
		"%s/%016" PRIx64 ".%s",
		pState->sPagesDir,
		iHash,
		sExt
	);

	xrtFileWriteAll(
		sSavedFile,
		pPage->pResponse->pBody,
		pPage->pResponse->iBodyLen,
		XRT_CP_UTF8
	);
	procDemoAppendManifest(pState, pPage, sSavedFile);
	xspiderNotifyPageSaved(pState->pSpider);
}


static void procDemoOnError(
	const char* sUrl,
	const char* sReferer,
	int iDepth,
	xnet_result iStatus,
	const char* sStage,
	const char* sErrorText,
	ptr pUserData
)
{
	(void)pUserData;

	if ( sErrorText != NULL && sErrorText[0] != '\0' ) {
		printf(
			"[error] stage=%s status=%d depth=%d url=%s referer=%s error=%s\n",
			sStage != NULL ? sStage : "(unknown)",
			iStatus,
			iDepth,
			sUrl != NULL ? sUrl : "(null)",
			sReferer != NULL ? sReferer : "(none)",
			sErrorText
		);
	} else {
		printf(
			"[error] stage=%s status=%d depth=%d url=%s referer=%s\n",
			sStage != NULL ? sStage : "(unknown)",
			iStatus,
			iDepth,
			sUrl != NULL ? sUrl : "(null)",
			sReferer != NULL ? sReferer : "(none)"
		);
	}
}


static void procDemoPrintSummary(xspider* pSpider, const xspider_demo_state* pState)
{
	xspider_stats tStats;

	tStats = xspiderSnapshotStats(pSpider);

	printf("\n");
	printf("------------- Summary / 汇总 -------------\n");
	printf("Discovered URLs / 发现链接: %u\n", tStats.iDiscoveredUrlCount);
	printf("Visited pages / 已访问页面: %u\n", tStats.iVisitedPageCount);
	printf("Saved pages / 已保存页面: %u\n", tStats.iSavedPageCount);
	printf("Redirects / 跟随重定向: %u\n", tStats.iRedirectCount);
	printf("Filtered URLs / 已过滤链接: %u\n", tStats.iFilteredUrlCount);
	printf("Errors / 错误数: %u\n", tStats.iErrorCount);
	printf("Manifest / 清单文件: %s\n", pState->sManifestPath);
	printf("Pages dir / 页面输出目录: %s\n", pState->sPagesDir);
	printf("------------------------------------------\n");
}


int main(int argc, char** argv)
{
	xspider_config tConfig;
	xspider_events tEvents;
	xspider_demo_state tState;
	xspider* pSpider = NULL;
	const char* sRootUrl;
	bool bRunOk = false;
	int i;
	int iPositionalIndex = 0;

	xrtInit();
	procDemoPrintHeader();

	if ( argc < 2 ) {
		procDemoPrintUsage(argv[0]);
		xrtUnit();
		return 1;
	}

	xspiderConfigInit(&tConfig);
	tConfig.bIncludeSubdomains = true;
	for ( i = 2; i < argc; ++i ) {
		const char* sArg = argv[i];

		if ( strcmp(sArg, "--insecure") == 0 ) {
			tConfig.bVerifyPeer = false;
			continue;
		}
		if ( strcmp(sArg, "--strict-tls") == 0 ) {
			tConfig.bVerifyPeer = true;
			continue;
		}
		if ( strcmp(sArg, "--delay-ms") == 0 ) {
			if ( i + 1 >= argc ) {
				printf("missing value for --delay-ms / --delay-ms 缺少参数值\n");
				printf("\n");
				procDemoPrintUsage(argv[0]);
				xrtUnit();
				return 1;
			}
			tConfig.iPolitenessDelayMs = (uint32)atoi(argv[++i]);
			continue;
		}
		if ( strcmp(sArg, "--subdomains") == 0 ) {
			tConfig.bIncludeSubdomains = true;
			continue;
		}
		if ( strcmp(sArg, "--no-subdomains") == 0 ) {
			tConfig.bIncludeSubdomains = false;
			continue;
		}

		iPositionalIndex++;
		if ( iPositionalIndex == 1 ) {
			tConfig.iMaxDepth = atoi(sArg);
		} else if ( iPositionalIndex == 2 ) {
			tConfig.iMaxPages = atoi(sArg);
		} else if ( iPositionalIndex == 3 ) {
			tConfig.iThreadCount = atoi(sArg);
		} else if ( iPositionalIndex == 4 ) {
			tConfig.iCoroutinePerThread = atoi(sArg);
		} else {
			printf("unknown argument / 未知参数: %s\n", sArg);
			printf("\n");
			procDemoPrintUsage(argv[0]);
			xrtUnit();
			return 1;
		}
	}
	if ( tConfig.iThreadCount < 1 ) tConfig.iThreadCount = 1;
	if ( tConfig.iCoroutinePerThread < 1 ) tConfig.iCoroutinePerThread = 1;
	tConfig.iEngineWorkerCount = tConfig.iThreadCount * 2;
	snprintf(tConfig.sOutputDir, sizeof(tConfig.sOutputDir), "./output");

	sRootUrl = argv[1];

	if ( !procDemoStateInit(&tState, tConfig.sOutputDir) ) {
		printf("failed to initialize demo output state\n");
		xrtUnit();
		return 2;
	}

	memset(&tEvents, 0, sizeof(tEvents));
	tEvents.procShouldVisit = procDemoShouldVisit;
	tEvents.procOnDiscover = procDemoOnDiscover;
	tEvents.procOnPage = procDemoOnPage;
	tEvents.procOnError = procDemoOnError;

	pSpider = xspiderCreate(&tConfig, &tEvents, &tState);
	if ( pSpider == NULL ) {
		printf("failed to create xspider\n");
		procDemoStateUnit(&tState);
		xrtUnit();
		return 3;
	}

	tState.pSpider = pSpider;

	if ( !xspiderAddSeed(pSpider, sRootUrl) ) {
		printf("failed to add seed url: %s\n", sRootUrl);
		xspiderDestroy(pSpider);
		procDemoStateUnit(&tState);
		xrtUnit();
		return 4;
	}

	printf("Root URL / 根地址: %s\n", sRootUrl);
	printf("Max depth / 最大深度: %d\n", tConfig.iMaxDepth);
	printf("Max pages / 最大页面数: %d\n", tConfig.iMaxPages);
	printf("Threads / 线程数: %d\n", tConfig.iThreadCount);
	printf("Coroutines per thread / 每线程协程数: %d\n", tConfig.iCoroutinePerThread);
	printf("Engine workers / 网络引擎 worker 数: %d\n", tConfig.iEngineWorkerCount);
	printf("Include subdomains / 包含子域名: %s\n", tConfig.bIncludeSubdomains ? "yes" : "no");
	printf("Verify TLS peer / 校验 TLS 证书: %s\n", tConfig.bVerifyPeer ? "yes" : "no");
	printf("Politeness delay / 礼貌延迟: %u ms\n", tConfig.iPolitenessDelayMs);
	printf("\n");

	bRunOk = xspiderRun(pSpider);
	if ( !bRunOk ) {
		printf("crawler run failed\n");
	}

	procDemoPrintSummary(pSpider, &tState);

	xspiderDestroy(pSpider);
	procDemoStateUnit(&tState);
	xrtUnit();
	return bRunOk ? 0 : 5;
}
