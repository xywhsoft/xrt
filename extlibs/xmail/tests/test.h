#ifndef XMAIL_TEST_H
#define XMAIL_TEST_H

#include "../../../tests/test.h"
#include <xmail.h>



/* 从显式地址和长度创建测试视图，不依赖 XRT string 模块。 */
static inline xstrview testMailViewN(const char* sText, size_t iSize)
{
	xstrview Text;

	Text.Data = sText;
	Text.Size = iSize;
	return Text;
}



/* 从零结尾文本创建测试视图。 */
static inline xstrview testMailView(const char* sText)
{
	return testMailViewN(sText, strlen(sText));
}



/* 比较测试视图与常量文本。 */
static inline bool testMailViewEqual(xstrview Text, xstrview Expected)
{
	return (Text.Size == Expected.Size) &&
		(memcmp(Text.Data, Expected.Data, Text.Size) == 0);
}



/* 按 ASCII 大小写不敏感规则比较两个测试视图。 */
static inline bool testMailViewCaseEqual(xstrview Left, xstrview Right)
{
	if ( Left.Size != Right.Size ) {
		return false;
	}
	for ( size_t i = 0; i < Left.Size; i++ ) {
		unsigned char iLeft = (unsigned char)Left.Data[i];
		unsigned char iRight = (unsigned char)Right.Data[i];

		if ( (iLeft >= (unsigned char)'A') && (iLeft <= (unsigned char)'Z') ) {
			iLeft = (unsigned char)(iLeft + ('a' - 'A'));
		}
		if ( (iRight >= (unsigned char)'A') && (iRight <= (unsigned char)'Z') ) {
			iRight = (unsigned char)(iRight + ('a' - 'A'));
		}
		if ( iLeft != iRight ) {
			return false;
		}
	}
	return true;
}

#endif
