#include "../test.h"



/* 验证安全随机 boundary 格式和 FormData 一步编码。 */
int main(void)
{
	xformdata* pForm = xrtFormDataCreate(NULL);
	xmultipartboundary Boundary;
	xhttpbody* pBody;
	uint8 BoundaryStorage[sizeof(xmultipartboundary) + 2u];
	char ContentType[128];
	size_t iSize;

	testRequire((pForm != NULL) && xrtFormDataAppendText(
		pForm, XRT_STR_LITERAL("name"), XRT_STR_LITERAL("value")
	), "random FormData setup failed");
	memset(BoundaryStorage, 0xA5, sizeof(BoundaryStorage));
	pBody = xrtFormDataBodyRandom(
		pForm,
		(xmultipartboundary*)(void*)(BoundaryStorage + 1u)
	);
	memcpy(&Boundary, BoundaryStorage + 1u, sizeof(Boundary));
	testRequire((pBody != NULL) &&
		(BoundaryStorage[0] == 0xA5) &&
		(BoundaryStorage[sizeof(BoundaryStorage) - 1u] == 0xA5) &&
		(Boundary.Size == 45) &&
		(memcmp(Boundary.Data, "----xrt-form-", 13) == 0) &&
		xrtMultipartContentTypeWrite(
			&Boundary,
			ContentType,
			sizeof(ContentType),
			&iSize
		) && (iSize > Boundary.Size),
		"random FormData encoding mismatch");
	xrtHttpBodyDestroy(pBody);
	testRequire(xrtFormDataBodyRandom(
		pForm,
		(xmultipartboundary*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == NULL, "random FormData accepted a wrapping output");
	xrtClearError();
	xrtFormDataDestroy(pForm);
	printf("[PASS] form_data_random\n");
	return 0;
}
