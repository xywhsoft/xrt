#define XCRON_MODULE_XCRON
#include <xcron.h>
#include <string.h>

int main(void)
{
	xcronexpression Expression; xcronexpressionissue Issue;
	if (!xcronExpressionCompile(XRT_STR_LITERAL("0,30 9-17 * JAN-MAR MON-FRI"), XCRON_UNIX5, &Expression, &Issue)) return 1;
	if (Expression.Fields[XCRON_FIELD_MINUTE] != ((uint64)1u | ((uint64)1u << 30u)) ||
		Expression.Fields[XCRON_FIELD_WEEKDAY] != 0x3eu || Expression.Flags != XCRON_DAY_MONTH_STAR) return 2;
	if (xcronExpressionCompile(XRT_STR_LITERAL("60 * * * *"), XCRON_UNIX5, &Expression, &Issue)) return 3;
	if (Issue.Code != XCRON_EXPRESSION_RANGE || xrtErrorKind(xrtGetError()) != XERR_RANGE ||
		strcmp(xrtErrorDomain(xrtGetError()), "xrt.cron") != 0) return 4;
	xrtClearError();
	return 0;
}
