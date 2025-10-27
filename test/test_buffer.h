


// Buffer 库测试
void Test_Buffer(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Buffer 库测试 :\n\n");
	
	
	
	// subject 1 : create object
	printf("Buffer test subject 1 : create object\n\n");
	xbuffer objBuffer = xrtBufferCreate(5, 10);
	if ( objBuffer ) {
		printf("Buffer object : %p\t\t\t\tpass! √\n", objBuffer);
		if ( objBuffer->Buffer ) {
			printf("\tBuffer : %p\t\t\t\tpass! √\n", objBuffer->Buffer);
		} else {
			printf("\tBuffer : %p\t\t\t\tfail! ×\n", objBuffer->Buffer);
		}
		printf("\tLength : %d\t\t\t\t=> 0\n", objBuffer->Length);
		printf("\tAllocLength : %d\t\t\t\t=> 5\n", objBuffer->AllocLength);
		printf("\tAllocStep : %d\t\t\t\t=> 10\n", objBuffer->AllocStep);
	} else {
		printf("Buffer object : %p\t\t\t\t\tfail! ×\n", objBuffer);
	}
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 2 : append string
	printf("Buffer test subject 2 : append string\n\n");
	xrtBufferAppend(objBuffer, "012345", 4, XBUF_UTF8);
	
	printf("\nBuffer state : \n");
	if ( objBuffer->Buffer ) {
		printf("\tBuffer : %p\t\t\t\tpass! √\n", objBuffer->Buffer);
	} else {
		printf("\tBuffer : %p\t\t\t\tfail! ×\n", objBuffer->Buffer);
	}
	printf("\tLength : %d\t\t\t\t=> 4\n", objBuffer->Length);
	printf("\tAllocLength : %d\t\t\t\t=> 5\n", objBuffer->AllocLength);
	printf("\tAllocStep : %d\t\t\t\t=> 10\n", objBuffer->AllocStep);
	
	printf("\nBuffer value : \n");
	printf("\t%s\n", objBuffer->Buffer);
	
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 3 : append string (auto length) & malloc
	printf("Buffer test subject 3 : append string (auto length) & malloc\n\n");
	xrtBufferAppend(objBuffer, "456789", 0, XBUF_UTF8);
	
	printf("\nBuffer state : \n");
	if ( objBuffer->Buffer ) {
		printf("\tBuffer : %p\t\t\t\tpass! √\n", objBuffer->Buffer);
	} else {
		printf("\tBuffer : %p\t\t\t\tfail! ×\n", objBuffer->Buffer);
	}
	printf("\tLength : %d\t\t\t\t=> 10\n", objBuffer->Length);
	printf("\tAllocLength : %d\t\t\t=> 21 (10 + 10 + 1)\n", objBuffer->AllocLength);
	printf("\tAllocStep : %d\t\t\t\t=> 10\n", objBuffer->AllocStep);
	
	printf("\nBuffer value : \n");
	printf("\t%s\n", objBuffer->Buffer);
	
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 4 : insert string (rewrite)
	printf("Buffer test subject 4 : insert string (rewrite)\n\n");
	xrtBufferInsert(objBuffer, 0, "9876543210", 0, XBUF_UTF8);
	
	printf("\nBuffer state : \n");
	if ( objBuffer->Buffer ) {
		printf("\tBuffer : %p\t\t\t\tpass! √\n", objBuffer->Buffer);
	} else {
		printf("\tBuffer : %p\t\t\t\tfail! ×\n", objBuffer->Buffer);
	}
	printf("\tLength : %d\t\t\t\t=> 10\n", objBuffer->Length);
	printf("\tAllocLength : %d\t\t\t=> 21 (10 + 10 + 1)\n", objBuffer->AllocLength);
	printf("\tAllocStep : %d\t\t\t\t=> 10\n", objBuffer->AllocStep);
	
	printf("\nBuffer value : \n");
	printf("\t%s\n", objBuffer->Buffer);
	
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 5 : Generate webpage
	printf("Buffer test subject 5 : Generate webpage\n\n");
	xrtBufferClear(objBuffer);
	xrtBufferAppend(objBuffer, "<!DOCTYPE html>\n", 0, XBUF_UTF8);
	xrtBufferAppend(objBuffer, "<html>\n", 0, XBUF_UTF8);
	xrtBufferAppend(objBuffer, "<head>\n", 0, XBUF_UTF8);
	xrtBufferAppend(objBuffer, "<meta charset=\"utf-8\">\n", 0, XBUF_UTF8);
	xrtBufferAppend(objBuffer, "<title>The page was generated on buffer</title>\n", 0, XBUF_UTF8);
	xrtBufferAppend(objBuffer, "</head>\n", 0, XBUF_UTF8);
	xrtBufferAppend(objBuffer, "<body>\n", 0, XBUF_UTF8);
	xrtBufferAppend(objBuffer, "Hello World!\n", 0, XBUF_UTF8);
	xrtBufferAppend(objBuffer, "</body>\n", 0, XBUF_UTF8);
	xrtBufferAppend(objBuffer, "</html>", 0, XBUF_UTF8);
	
	printf("\nBuffer state : \n");
	if ( objBuffer->Buffer ) {
		printf("\tBuffer : %p\t\t\t\tpass! √\n", objBuffer->Buffer);
	} else {
		printf("\tBuffer : %p\t\t\t\tfail! ×\n", objBuffer->Buffer);
	}
	printf("\tLength : %d\t\t\t\t=> 144\n", objBuffer->Length);
	printf("\tAllocLength : %d\t\t\t=> 155\n", objBuffer->AllocLength);
	printf("\tAllocStep : %d\t\t\t\t=> 10\n", objBuffer->AllocStep);
	
	printf("\nBuffer value : \n");
	printf("\t%s\n", objBuffer->Buffer);
	
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject X : struct unit & destroy
	printf("Buffer test subject X : struct unit & destroy\n\n");
	xrtBufferUnit(objBuffer);
	printf("Buffer object (%p) already unit!\n\n", objBuffer);
	
	printf("Buffer state (object) : \n");
	if ( objBuffer->Buffer ) {
		printf("\tBuffer : %p\t\t\t\tfail! ×\n", objBuffer->Buffer);
	} else {
		printf("\tBuffer : %p\t\t\t\tpass! √\n", objBuffer->Buffer);
	}
	printf("\tLength : %d\t\t\t\t=> 0\n", objBuffer->Length);
	printf("\tAllocLength : %d\t\t\t\t=> 0\n", objBuffer->AllocLength);
	printf("\tAllocStep : %d\t\t\t\t=> 10\n", objBuffer->AllocStep);
	
	xrtBufferDestroy(objBuffer);
	printf("\nBuffer object (%p) already destroyed!\n", objBuffer);
	
	
	
}


