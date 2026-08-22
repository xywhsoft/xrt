


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
	
	str sSep = xrtPathSep();
	str sNorm = xrtPathNormalize("alpha//beta/./gamma/../delta", 0);
	str sCwd = xrtPathCwd();
	str sAbs = xrtPathAbs(".", 0);
	str sChild = xrtPathJoin(2, sAbs, "child.txt");
	str sRel = xrtPathRel(sAbs, sChild);
	str sHome = xrtPathHome();
	str sTemp = xrtPathTemp();
	str sApp = xrtPathAppDir();
	str sWithExt = xrtPathWithExt("alpha/beta.txt", "md");
	str sWithName = xrtPathWithName("alpha/beta.txt", "readme.md");
	str sWithExtExt = xrtPathGetExt(sWithExt, 0);
	printf("xrtPathSep : %s\n", sSep);
	printf("xrtPathNormalize : %s [%s]\n", sNorm, strstr((char*)sNorm, "delta") ? "PASS" : "FAIL");
	printf("xrtPathCwd : %s [%s]\n", sCwd, xrtPathIsAbs(sCwd, 0) ? "PASS" : "FAIL");
	printf("xrtPathAbs : %s [%s]\n", sAbs, xrtPathIsAbs(sAbs, 0) ? "PASS" : "FAIL");
	printf("xrtPathRel : %s [%s]\n", sRel, strcmp((char*)sRel, "child.txt") == 0 ? "PASS" : "FAIL");
	printf("xrtPathHome : %s [%s]\n", sHome, sHome && sHome[0] ? "PASS" : "FAIL");
	printf("xrtPathTemp : %s [%s]\n", sTemp, sTemp && sTemp[0] ? "PASS" : "FAIL");
	printf("xrtPathAppDir : %s [%s]\n", sApp, sApp && sApp[0] ? "PASS" : "FAIL");
	printf("xrtPathWithExt : %s [%s]\n", sWithExt, strcmp((char*)sWithExtExt, "md") == 0 ? "PASS" : "FAIL");
	printf("xrtPathWithName : %s [%s]\n", sWithName, strstr((char*)sWithName, "readme.md") ? "PASS" : "FAIL");
	xrtFree(sSep);
	xrtFree(sNorm);
	xrtFree(sCwd);
	xrtFree(sAbs);
	xrtFree(sChild);
	xrtFree(sRel);
	xrtFree(sHome);
	xrtFree(sTemp);
	xrtFree(sApp);
	xrtFree(sWithExt);
	xrtFree(sWithExtExt);
	xrtFree(sWithName);
}


