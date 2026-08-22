#include <stdio.h>
#include <xruntime.h>



/* 构造一个带动态字段的语言对象属性表并读取字段。 */
int main(void)
{
	xrtdynamicfields* pFields = xrtDynamicFieldsCreate();
	const xvalue* pValue;
	int64 iAnswer;

	if ( (pFields == NULL) ||
		 !xrtDynamicFieldsSetNew(
			pFields, XRT_STR_LITERAL("answer"), xrtValueInt(42)
		) ) {
		xrtDynamicFieldsUnref(pFields);
		return 1;
	}
	pValue = xrtDynamicFieldsGet(
		pFields, XRT_STR_LITERAL("answer")
	);
	if ( (pValue == NULL) || !xrtValueGetInt(pValue, &iAnswer) ) {
		xrtDynamicFieldsUnref(pFields);
		return 1;
	}
	printf("%lld\n", (long long)iAnswer);
	xrtDynamicFieldsUnref(pFields);
	return 0;
}
