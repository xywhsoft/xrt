// XID test
void Test_XID(xrtGlobalData* xCore)
{
	(void)xCore;

	printf("\n\n\n------------------------------------\n\n XID Test:\n\n");

	xid xida = xrtMakeXID();
	assert(xida != NULL);
	printf("xrtMakeXID - Time : %lld\n", xida->Time);
	printf("xrtMakeXID - Tick : %d\n", xida->Tick);
	printf("xrtMakeXID - Addr : %x\n", xida->Addr);
	printf("xrtMakeXID - Rand : %lld\n", xida->Rand);

	str sXID = xrtEncodeXID(xida);
	assert(sXID != NULL && strlen(__xrt_cstr(sXID)) == 32);
	printf("xrtEncodeXID : %s\n", sXID);

	xid decoded = xrtDecodeXID(sXID);
	assert(decoded != NULL);
	assert(xrtCompXID(xida, decoded));
	printf("xrtDecodeXID - Time : %lld\n", decoded->Time);
	printf("xrtDecodeXID - Tick : %d\n", decoded->Tick);
	printf("xrtDecodeXID - Addr : %x\n", decoded->Addr);
	printf("xrtDecodeXID - Rand : %lld\n", decoded->Rand);

	assert(xrtDecodeXID((str)"not-an-xid") == NULL);
	assert(xrtDecodeXID((str)"!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!") == NULL);

	xrtFree(decoded);
	xrtFree(sXID);
	xrtFree(xida);
}
