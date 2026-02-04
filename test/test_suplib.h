

// SupLib 库测试
void Test_SupLib(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n SupLib 库测试 :\n\n");

	// SUPLIB-001: UTF-16空字符串
	printf("SupLib test subject 1 : UTF-16空字符串\n\n");
	uint16_t u16Empty[] = { 0 };
	size_t len1 = u16len(u16Empty);
	printf("u16len(\"\") = %zu\t\t\t\t=> 0\n", len1);
	if (len1 == 0) printf("pass! √\n");
	else printf("fail! ×\n");

	printf("\n");

	// SUPLIB-002: UTF-16 ASCII
	printf("SupLib test subject 2 : UTF-16 ASCII\n\n");
	uint16_t u16Hello[] = { 'H', 'e', 'l', 'l', 'o', 0 };
	size_t len2 = u16len(u16Hello);
	printf("u16len(\"Hello\") = %zu\t\t\t\t=> 5\n", len2);
	if (len2 == 5) printf("pass! √\n");
	else printf("fail! ×\n");

	printf("\n");

	// SUPLIB-003: UTF-16中文字符
	printf("SupLib test subject 3 : UTF-16中文字符\n\n");
	uint16_t u16Chinese[] = { 0x4F60, 0x597D, 0 };
	size_t len3 = u16len(u16Chinese);
	printf("u16len(\"你好\") = %zu\t\t\t\t=> 2\n", len3);
	if (len3 == 2) printf("pass! √\n");
	else printf("fail! ×\n");

	printf("\n");

	// SUPLIB-004: UTF-16混合字符
	printf("SupLib test subject 4 : UTF-16混合字符\n\n");
	uint16_t u16Mixed[] = { 'H', 'e', 'l', 'l', 'o', 0x4F60, 0x597D, 0 };
	size_t len4 = u16len(u16Mixed);
	printf("u16len(\"Hello你好\") = %zu\t\t\t=> 7\n", len4);
	if (len4 == 7) printf("pass! √\n");
	else printf("fail! ×\n");

	printf("\n");

	// SUPLIB-005: UTF-32空字符串
	printf("SupLib test subject 5 : UTF-32空字符串\n\n");
	uint32_t u32Empty[] = { 0 };
	size_t len5 = u32len(u32Empty);
	printf("u32len(\"\") = %zu\t\t\t\t=> 0\n", len5);
	if (len5 == 0) printf("pass! √\n");
	else printf("fail! ×\n");

	printf("\n");

	// SUPLIB-006: UTF-32 ASCII
	printf("SupLib test subject 6 : UTF-32 ASCII\n\n");
	uint32_t u32Hello[] = { 'H', 'e', 'l', 'l', 'o', 0 };
	size_t len6 = u32len(u32Hello);
	printf("u32len(\"Hello\") = %zu\t\t\t\t=> 5\n", len6);
	if (len6 == 5) printf("pass! √\n");
	else printf("fail! ×\n");

	printf("\n");

	// SUPLIB-007: UTF-32中文字符
	printf("SupLib test subject 7 : UTF-32中文字符\n\n");
	uint32_t u32Chinese[] = { 0x4F60, 0x597D, 0 };
	size_t len7 = u32len(u32Chinese);
	printf("u32len(\"你好\") = %zu\t\t\t\t=> 2\n", len7);
	if (len7 == 2) printf("pass! √\n");
	else printf("fail! ×\n");

	printf("\n");

	// SUPLIB-008: UTF-32混合字符
	printf("SupLib test subject 8 : UTF-32混合字符\n\n");
	uint32_t u32Mixed[] = { 'H', 'e', 'l', 'l', 'o', 0x4F60, 0x597D, 0 };
	size_t len8 = u32len(u32Mixed);
	printf("u32len(\"Hello你好\") = %zu\t\t\t=> 7\n", len8);
	if (len8 == 7) printf("pass! √\n");
	else printf("fail! ×\n");

	printf("\n");

#if defined(_WIN32) || defined(_WIN64)
	// SUPLIB-009: memmem基本查找
	printf("SupLib test subject 9 : memmem基本查找\n\n");
	char haystack[] = "Hello World";
	char needle[] = "World";
	ptr result1 = memmem(haystack, strlen(haystack), needle, strlen(needle));
	printf("memmem(\"Hello World\", \"World\") = %p\t\t=> 非NULL\n", result1);
	if (result1 != NULL) printf("pass! √\n");
	else printf("fail! ×\n");

	printf("\n");

	// SUPLIB-010: memmem未找到
	printf("SupLib test subject 10 : memmem未找到\n\n");
	char haystack2[] = "Hello";
	char needle2[] = "World";
	ptr result2 = memmem(haystack2, strlen(haystack2), needle2, strlen(needle2));
	printf("memmem(\"Hello\", \"World\") = %p\t\t=> NULL\n", result2);
	if (result2 == NULL) printf("pass! √\n");
	else printf("fail! ×\n");

	printf("\n");

	// SUPLIB-011: memmem空模式
	printf("SupLib test subject 11 : memmem空模式\n\n");
	char haystack3[] = "Hello";
	char needle3[] = "";
	ptr result3 = memmem(haystack3, strlen(haystack3), needle3, strlen(needle3));
	printf("memmem(\"Hello\", \"\") = %p\t\t\t=> NULL\n", result3);
	if (result3 == NULL) printf("pass! √\n");
	else printf("fail! ×\n");

	printf("\n");

	// SUPLIB-012: memmem边界
	printf("SupLib test subject 12 : memmem边界\n\n");
	char haystack4[] = "abc";
	char needle4[] = "abc";
	ptr result4 = memmem(haystack4, strlen(haystack4), needle4, strlen(needle4));
	printf("memmem(\"abc\", \"abc\") = %p\t\t\t=> 非NULL\n", result4);
	if (result4 != NULL) printf("pass! √\n");
	else printf("fail! ×\n");

	printf("\n");
#else
	printf("SupLib test subject 9-12 : memmem 功能 (仅Windows平台)\n\n");
	printf("skipped - memmem is Windows only\n\n");
#endif

	printf("\n\n\n");
	#if defined(_WIN32) || defined(_WIN64)
		system("pause");
	#else
		printf("Press Enter to continue...");
		getchar();
	#endif
	#if defined(_WIN32) || defined(_WIN64)
		system("cls");
	#else
		printf("\033[2J\033[H");
	#endif
}
