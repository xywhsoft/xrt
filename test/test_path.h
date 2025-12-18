


// Path 库测试
void Test_Path(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Path 库测试 :\n\n");
	printf("xrtPathGetNameExt : %s\n", xrtPathGetNameExt("c:\\123\\456\\789\\file.ext", 0));
	printf("xrtPathGetName : %s\n", xrtPathGetName("c:\\123\\456\\789\\file.ext", 0));
	printf("xrtPathGetExt : %s\n", xrtPathGetExt("c:\\123\\456\\789\\file.ext", 0));
	printf("xrtPathGetDir : %s\n", xrtPathGetDir("c:\\123\\456\\789\\file.ext", 0));
	printf("xrtPathGetDir : %s\n", xrtPathGetDir("c:\\123\\456\\789\\", 0));
	printf("xrtPathIsAbs : %d\n", xrtPathIsAbs("c:\\123\\456\\789\\", 0));
	printf("xrtPathRandom : %s\n", xrtPathRandom("c:\\123\\456\\789\\Rand_", 0, ".jpg", 4, 32));
	printf("xrtPathJoin : %s\n", xrtPathJoin(4, "c:\\123", "456", "789", "file.ext"));
}


