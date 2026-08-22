#include "../test.h"



/* 借用固定正文视图并验证内容。 */
static bool testFormDataBodyEqual(
	xhttpbody* pBody,
	cstr sExpected,
	size_t iSize
)
{
	xbytesview View;

	return xrtHttpBodyView(pBody, &View) &&
		(View.Size == iSize) &&
		((iSize == 0) ||
		 (memcmp(View.Data, sExpected, iSize) == 0));
}



/* 验证配置、借用描述符和结果结构支持未对齐存储并拒绝回绕范围。 */
static void testFormDataMemoryContracts(void)
{
	uint8 ConfigStorage[sizeof(xformdataconfig) + 2u];
	uint8 FilenameStorage[sizeof(xstrview) + 2u];
	uint8 IndexStorage[sizeof(size_t) + 2u];
	uint8 PartStorage[sizeof(xformdatapart) + 2u];
	xformdataconfig Config;
	xstrview Filename = XRT_STR_LITERAL("name.txt");
	xformdatapart Part;
	xformdata* pForm;
	size_t iIndex = 0;

	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	xrtFormDataConfigInit(
		(xformdataconfig*)(void*)(ConfigStorage + 1u)
	);
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	testRequire((ConfigStorage[0] == 0xA5) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == 0xA5) &&
		(Config.InitialParts == 8u) && (Config.MaxParts == 1024u),
		"FormData config init did not support unaligned storage");
	Config.InitialParts = 0u;
	Config.MaxParts = 4u;
	memcpy(ConfigStorage + 1u, &Config, sizeof(Config));
	pForm = xrtFormDataCreate(
		(const xformdataconfig*)(const void*)(ConfigStorage + 1u)
	);
	testRequire(pForm != NULL,
		"FormData create did not snapshot unaligned config");

	memcpy(FilenameStorage + 1u, &Filename, sizeof(Filename));
	testRequire(xrtFormDataAppendBytes(
		pForm,
		XRT_STR_LITERAL("file"),
		(xbytesview){ (cbytes)"x", 1u },
		(const xstrview*)(const void*)(FilenameStorage + 1u),
		XRT_STR_LITERAL("text/plain")
	), "FormData did not snapshot an unaligned filename descriptor");
	testRequire(!xrtFormDataAppendText(
		pForm,
		(xstrview){
			(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
		},
		XRT_STR_LITERAL("bad")
	) && (xrtFormDataCount(pForm) == 1u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"FormData accepted a wrapping name or changed state");
	xrtClearError();
	testRequire(!xrtFormDataAppendBytes(
		pForm,
		XRT_STR_LITERAL("bad"),
		(xbytesview){ (cbytes)"x", 1u },
		(const xstrview*)(uintptr_t)(UINTPTR_MAX - 1u),
		(xstrview){ NULL, 0 }
	) && (xrtFormDataCount(pForm) == 1u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"FormData accepted a wrapping filename descriptor");
	xrtClearError();

	testRequire(xrtFormDataAt(
		pForm,
		0u,
		(xformdatapart*)(void*)(PartStorage + 1u)
	), "FormData At did not support unaligned output");
	memcpy(&Part, PartStorage + 1u, sizeof(Part));
	testRequire((Part.Name.Size == 4u) &&
		(memcmp(Part.Name.Data, "file", 4u) == 0),
		"FormData At published wrong unaligned output");
	memcpy(IndexStorage + 1u, &iIndex, sizeof(iIndex));
	testRequire(xrtFormDataFind(
		pForm,
		XRT_STR_LITERAL("file"),
		(size_t*)(void*)(IndexStorage + 1u),
		(xformdatapart*)(void*)(PartStorage + 1u)
	) == XHTTP_NEXT_ITEM,
		"FormData Find did not support unaligned outputs");
	memcpy(&iIndex, IndexStorage + 1u, sizeof(iIndex));
	testRequire(iIndex == 1u,
		"FormData Find published wrong unaligned index");
	testRequire(!xrtFormDataAt(
		pForm,
		0u,
		(xformdatapart*)(uintptr_t)(UINTPTR_MAX - 1u)
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"FormData At accepted a wrapping output range");
	xrtClearError();
	xrtFormDataDestroy(pForm);

	xrtFormDataConfigInit((xformdataconfig*)(uintptr_t)(
		UINTPTR_MAX - 1u
	));
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"FormData config init accepted a wrapping range");
	xrtClearError();
	testRequire(xrtFormDataCreate(
		(const xformdataconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == NULL && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"FormData create accepted a wrapping config range");
	xrtClearError();
}



/* 验证有序重复项、文件元数据、查找和 Set 的位置语义。 */
static void testFormDataCollection(void)
{
	xformdata* pForm = xrtFormDataCreate(NULL);
	xstrview Filename = XRT_STR_LITERAL("a.txt");
	xformdatapart Part;
	size_t iIndex = 0;

	testRequire(pForm != NULL, "FormData create failed");
	testRequire(xrtFormDataAppendText(
		pForm, XRT_STR_LITERAL("tag"), XRT_STR_LITERAL("one")
	) && xrtFormDataAppendText(
		pForm, XRT_STR_LITERAL("other"), XRT_STR_LITERAL("x")
	) && xrtFormDataAppendText(
		pForm, XRT_STR_LITERAL("tag"), XRT_STR_LITERAL("two")
	) && xrtFormDataAppendBytes(
		pForm,
		XRT_STR_LITERAL("file"),
		(xbytesview){ (const uint8*)"data", 4 },
		&Filename,
		XRT_STR_LITERAL("text/plain")
	), "FormData append failed");
	testRequire((xrtFormDataCount(pForm) == 4) &&
		(xrtFormDataCountName(
			pForm, XRT_STR_LITERAL("tag")
		) == 2) && xrtFormDataHas(
			pForm, XRT_STR_LITERAL("file")
		), "FormData collection counts mismatch");
	testRequire((xrtFormDataFind(
		pForm,
		XRT_STR_LITERAL("tag"),
		&iIndex,
		&Part
	) == XHTTP_NEXT_ITEM) && (iIndex == 1) &&
		testFormDataBodyEqual(Part.Body, "one", 3),
		"FormData first duplicate lookup mismatch");
	testRequire((xrtFormDataFind(
		pForm,
		XRT_STR_LITERAL("tag"),
		&iIndex,
		&Part
	) == XHTTP_NEXT_ITEM) && (iIndex == 3) &&
		testFormDataBodyEqual(Part.Body, "two", 3),
		"FormData second duplicate lookup mismatch");
	testRequire(xrtFormDataSetText(
		pForm, XRT_STR_LITERAL("tag"), XRT_STR_LITERAL("final")
	), "FormData Set failed");
	testRequire((xrtFormDataCount(pForm) == 3) &&
		xrtFormDataAt(pForm, 0, &Part) &&
		(Part.Name.Size == 3) &&
		(memcmp(Part.Name.Data, "tag", 3) == 0) &&
		testFormDataBodyEqual(Part.Body, "final", 5) &&
		xrtFormDataAt(pForm, 1, &Part) &&
		(memcmp(Part.Name.Data, "other", 5) == 0),
		"FormData Set did not preserve first position");
	testRequire(xrtFormDataAt(pForm, 2, &Part) &&
		((Part.Flags & XFORM_DATA_PART_FILENAME) != 0) &&
		((Part.Flags & XFORM_DATA_PART_CONTENT_TYPE) != 0) &&
		(Part.Filename.Size == 5) &&
		(memcmp(Part.Filename.Data, "a.txt", 5) == 0) &&
		(Part.ContentType.Size == 10) &&
		(memcmp(Part.ContentType.Data, "text/plain", 10) == 0),
		"FormData file metadata mismatch");
	testRequire(xrtFormDataGet(
		pForm, XRT_STR_LITERAL("file"), &Part
	) && ((Part.Flags & XFORM_DATA_PART_FILENAME) != 0) &&
		(Part.Length == 4),
		"FormData common Get mismatch");
	memset(&Part, 0xA5, sizeof(Part));
	testRequire(!xrtFormDataGet(
		pForm, XRT_STR_LITERAL("missing"), &Part
	) && (Part.Name.Data == NULL) && (Part.Body == NULL),
		"FormData missing Get did not clear output");
	xrtFormDataDestroy(pForm);
}



/* 验证借用容器视图和同一正文执行 Set 时不会被原地修改破坏。 */
static void testFormDataAliasAndClone(void)
{
	xformdata* pForm = xrtFormDataCreate(NULL);
	xformdata* pClone;
	xstrview Filename = XRT_STR_LITERAL("alias.bin");
	xformdatapart Part;
	xformdatapart ClonePart;

	testRequire((pForm != NULL) && xrtFormDataAppendBytes(
		pForm,
		XRT_STR_LITERAL("item"),
		(xbytesview){ (const uint8*)"bytes", 5 },
		&Filename,
		XRT_STR_LITERAL("application/octet-stream")
	) && xrtFormDataAt(pForm, 0, &Part),
		"FormData alias setup failed");
	testRequire(xrtFormDataSetBody(
		pForm,
		Part.Name,
		Part.Body,
		&Part.Filename,
		Part.ContentType
	), "FormData alias Set failed");
	testRequire(xrtFormDataAt(pForm, 0, &Part) &&
		(Part.Filename.Size == 9) &&
		(memcmp(Part.Filename.Data, "alias.bin", 9) == 0) &&
		testFormDataBodyEqual(Part.Body, "bytes", 5),
		"FormData alias Set corrupted metadata");
	pClone = xrtFormDataClone(pForm);
	testRequire((pClone != NULL) &&
		xrtFormDataAt(pClone, 0, &ClonePart) &&
		(ClonePart.Name.Data != Part.Name.Data) &&
		(ClonePart.Body == Part.Body),
		"FormData clone ownership mismatch");
	testRequire((xrtFormDataRemove(
		pForm, XRT_STR_LITERAL("item")
	) == 1) && (xrtFormDataCount(pForm) == 0) &&
		testFormDataBodyEqual(ClonePart.Body, "bytes", 5),
		"FormData clone did not retain body");
	xrtFormDataDestroy(pClone);
	xrtFormDataDestroy(pForm);
}



/* 正文释放计数验证容器引用边界。 */
static void testFormDataRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	size_t* pCalls = (size_t*)pContext;

	(void)pData;
	(void)iSize;
	(*pCalls)++;
}



/* 验证正文引用和零值配置限制。 */
static void testFormDataLimitsAndOwnership(void)
{
	xformdataconfig Config;
	xformdata* pForm;
	xhttpbody* pBody;
	size_t iReleases = 0;

	xrtFormDataConfigInit(&Config);
	Config.InitialParts = 0;
	Config.MaxParts = 1;
	Config.MaxMetadata = 1;
	Config.MaxName = 1;
	Config.MaxPartBytes = 2;
	Config.MaxBodyBytes = 2;
	pForm = xrtFormDataCreate(&Config);
	testRequire(pForm != NULL, "limited FormData create failed");
	testRequire(xrtFormDataAppendText(
		pForm, XRT_STR_LITERAL("a"), XRT_STR_LITERAL("12")
	), "limited FormData rejected valid Part");
	testRequire(!xrtFormDataAppendText(
		pForm, XRT_STR_LITERAL("b"), XRT_STR_LITERAL("x")
	), "limited FormData accepted excess Part");
	xrtClearError();
	testRequire(!xrtFormDataSetText(
		pForm, XRT_STR_LITERAL("a"), XRT_STR_LITERAL("123")
	) && xrtFormDataAt(pForm, 0, &(xformdatapart){ 0 }),
		"limited FormData changed state after failed Set");
	xrtClearError();
	xrtFormDataDestroy(pForm);

	pForm = xrtFormDataCreate(NULL);
	pBody = xrtHttpBodyReference(
		(xbytesview){ (cbytes)"leased", 6 },
		testFormDataRelease,
		&iReleases
	);
	testRequire((pForm != NULL) && (pBody != NULL) &&
		xrtFormDataAppendBody(
			pForm,
			XRT_STR_LITERAL("body"),
			pBody,
			NULL,
			(xstrview){ NULL, 0 }
		), "FormData body reference setup failed");
	xrtHttpBodyDestroy(pBody);
	testRequire(iReleases == 0,
		"FormData released retained body early");
	xrtFormDataDestroy(pForm);
	testRequire(iReleases == 1,
		"FormData did not release retained body");
}



/* 运行拥有型 FormData 容器测试。 */
int main(void)
{
	testFormDataMemoryContracts();
	testFormDataCollection();
	testFormDataAliasAndClone();
	testFormDataLimitsAndOwnership();
	printf("[PASS] form_data\n");
	return 0;
}

