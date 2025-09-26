


// 网络库测试
void Test_Network(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n 网络库测试 :\n\n");
	
	printf("Local IP : %s\n", xrtGetLocalIP());
	printf("Local IP [ int ] : %x\n", xrtGetLocalRawIP());
	printf("Local IP [ int & g ] : %x\n", xCore->LocalAddr);
	printf("Local Name : %s\n", xrtGetLocalName());
	printf("Local Name : %s\n", xrtGetLocalMAC());
}


