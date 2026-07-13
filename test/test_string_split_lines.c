#include "xrt.h"

#include <stdio.h>
#include <string.h>

static int check_lines(const char* text, size_t expectedCount, const char* const* expected)
{
	size_t count = 0;
	str* lines = xrtStrSplitLines((str)text, 0, &count);
	size_t i;

	if ( lines == NULL || count != expectedCount ) {
		fprintf(stderr, "splitLines count mismatch: text='%s', actual=%zu, expected=%zu\n",
			text != NULL ? text : "(null)", count, expectedCount);
		xrtFree(lines);
		return 0;
	}
	for ( i = 0; i < count; ++i ) {
		if ( lines[i] == NULL || strcmp((const char*)lines[i], expected[i]) != 0 ) {
			fprintf(stderr, "splitLines item mismatch at %zu\n", i);
			xrtFree(lines);
			return 0;
		}
	}
	if ( lines[count] != NULL ) {
		fprintf(stderr, "splitLines result is not null terminated\n");
		xrtFree(lines);
		return 0;
	}
	xrtFree(lines);
	return 1;
}

int main(void)
{
	static const char* abc[] = {"a", "b", "c"};
	static const char* ab[] = {"a", "b"};
	static const char* oneEmpty[] = {""};
	static const char* aEmpty[] = {"a", ""};

	xrtInit();
	if ( !check_lines("", 0, NULL) ||
		 !check_lines("a\nb\nc", 3, abc) ||
		 !check_lines("a\nb\n", 2, ab) ||
		 !check_lines("a\r\nb\r\n", 2, ab) ||
		 !check_lines("a\rb\r", 2, ab) ||
		 !check_lines("\n", 1, oneEmpty) ||
		 !check_lines("a\n\n", 2, aEmpty) ) {
		xrtUnit();
		return 1;
	}
	xrtUnit();
	puts("test_string_split_lines: PASS");
	return 0;
}
