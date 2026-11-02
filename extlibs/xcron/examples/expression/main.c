#include <xcron.h>
#include <inttypes.h>
#include <stdio.h>

int main(void)
{
	const xstrview Text = XRT_STR_LITERAL("*/15 9-17 * JAN-MAR MON-FRI");
	xcronexpression Expression; xcronexpressionissue Issue;
	if (!xcronExpressionCompile(Text, XCRON_UNIX5, &Expression, &Issue)) {
		fprintf(stderr, "cron error %d at byte %zu\n", (int)Issue.Code, Issue.Offset); return 1;
	}
	printf("expression: %.*s\n", (int)Text.Size, Text.Data);
	for (size_t Field = 0; Field < XCRON_FIELD_COUNT; ++Field)
		printf("field %zu mask: 0x%016" PRIx64 "\n", Field, Expression.Fields[Field]);
	printf("day star flags: %u\n", (unsigned)Expression.Flags);
	return 0;
}
