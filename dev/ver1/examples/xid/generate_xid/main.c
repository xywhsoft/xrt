/*
 * XRT Example - Generate XID
 * XRT 范例 - 生成 XID
 *
 * Description / 说明:
 *   EN: Demonstrates XID generation, encoding, decoding and comparison.
 *   CN: 演示 XID 的生成、编码、解码与比较。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/xid_generate_xid.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/xid_generate_xid -lm -lpthread
 *
 * Note / 注意:
 *   - XID objects and strings returned by XRT must be freed with xrtFree.
 *   - This example also checks simple uniqueness across multiple generated IDs.
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"


int main()
{
	int i;
	xid pFirst = NULL;
	xid pDecoded = NULL;
	str sEncoded = NULL;
	bool bUnique = TRUE;
	str arrIDs[5] = { 0 };

	xrtInit();

	printf("=== Generate XID ===\n");
	printf("=== 生成 XID ===\n");

	pFirst = xrtMakeXID();
	sEncoded = xrtEncodeXID(pFirst);
	pDecoded = xrtDecodeXID(sEncoded);

	printf("Encoded XID: %s\n", sEncoded);
	printf("Decoded equals original: %s\n", xrtCompXID(pFirst, pDecoded) ? "TRUE" : "FALSE");

	for ( i = 0; i < 5; i++ ) {
		arrIDs[i] = xrtMakeXIDS();
		printf("Batch[%d] = %s\n", i, arrIDs[i]);
		if ( (i > 0) && (strcmp(arrIDs[i - 1], arrIDs[i]) == 0) ) {
			bUnique = FALSE;
		}
	}

	printf("Batch unique check: %s\n", bUnique ? "TRUE" : "FALSE");

	xrtFree(pFirst);
	xrtFree(pDecoded);
	xrtFree(sEncoded);

	for ( i = 0; i < 5; i++ ) {
		xrtFree(arrIDs[i]);
	}

	xrtUnit();
	return 0;
}
