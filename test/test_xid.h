


// XID 库测试
void Test_XID(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n XID 库测试 :\n\n");
	xid xida = xrtMakeXID(xrtRand32());
	printf("xrtMakeXID - Time : %lld\n", xida->Time);
	printf("xrtMakeXID - Tick : %d\n", xida->Tick);
	printf("xrtMakeXID - Addr : %x\n", xida->Addr);
	printf("xrtMakeXID - Rand : %lld\n", xida->Rand);
	str sXID = xrtEncodeXID(xida);
	printf("xrtEncodeXID : %s\n", sXID);
	xida = xrtDecodeXID(sXID);
	printf("xrtDecodeXID - Time : %lld\n", xida->Time);
	printf("xrtDecodeXID - Tick : %d\n", xida->Tick);
	printf("xrtDecodeXID - Addr : %x\n", xida->Addr);
	printf("xrtDecodeXID - Rand : %lld\n", xida->Rand);
}


