#include <stdio.h>
#include <xrt.h>



/* 展示批量生成、拥有型文本、比较和格式错误定位。 */
int main(void)
{
	xid Values[4] = { XID_ZERO };
	xid Parsed = XID_ZERO;
	xid Zero = XID_ZERO;
	str sFormatted = NULL;
	str sGenerated = NULL;
	size_t iOffset = XRT_NPOS;
	bool bValid = false;

	/* 批量入口由调用方提供连续值存储，不为每个标识分配对象。 */
	if ( !xrtXidMakeMany(Values, 4u) || xrtXidIsZero(&Values[0]) ) {
		goto cleanup;
	}

	/* Format 和 MakeString 是需要 xrtFree 的常见路径便利入口。 */
	sFormatted = xrtXidFormat(&Values[0]);
	sGenerated = xrtXidMakeString();
	if ( (sFormatted == NULL) || (sGenerated == NULL) ||
		 !xrtXidParse(
			(xstrview){ sFormatted, XID_TEXT_SIZE }, &Parsed
		 ) || !xrtXidEqual(&Values[0], &Parsed) ||
		 !xrtXidIsZero(&Zero) ) {
		goto cleanup;
	}

	/* 格式错误携带第一个非法位置，调用方无需解析错误消息。 */
	if ( xrtXidParse(XRT_STR_LITERAL("bad"), &Parsed) ||
		 !xrtXidErrorOffset(xrtGetError(), &iOffset) ||
		 (iOffset != 3u) ) {
		goto cleanup;
	}
	xrtClearError();
	bValid = true;

cleanup:
	printf("batch order: %d\n", xrtXidCompare(&Values[0], &Values[1]));
	printf("invalid offset: %zu\n", iOffset);
	xrtFree(sFormatted);
	xrtFree(sGenerated);
	return bValid ? 0 : 1;
}
