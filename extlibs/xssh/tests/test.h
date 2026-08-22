#ifndef XSSH_TEST_H
#define XSSH_TEST_H

#include "../../../tests/test.h"
#include <xssh.h>



/* 比较两个字节视图。 */
static inline bool testSshBytesEqual(xbytesview Left, xbytesview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0u) || (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 比较两个文本视图。 */
static inline bool testSshTextEqual(xstrview Left, xstrview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0u) || (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}

#endif
