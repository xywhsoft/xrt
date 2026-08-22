#include <stdio.h>

#include <xrt.h>



/* 示例记录直接连续存放在数组中。 */
typedef struct examplerecord {
	int ID;
	int Priority;
} examplerecord;



/* 按优先级从小到大排列记录。 */
static int exampleCompareRecord(const void* pLeft, const void* pRight)
{
	const examplerecord* pLeftRecord = (const examplerecord*)pLeft;
	const examplerecord* pRightRecord = (const examplerecord*)pRight;

	return (pLeftRecord->Priority > pRightRecord->Priority) -
		(pLeftRecord->Priority < pRightRecord->Priority);
}



/* 演示常见的一行追加和直接连续遍历。 */
int main(void)
{
	xarray tRecords;
	examplerecord pInput[] = {
		{ 101, 30 },
		{ 102, 10 },
		{ 103, 20 }
	};

	if ( !xrtArrayInit(&tRecords, sizeof(examplerecord)) ) {
		return 1;
	}
	if (
		!xrtArrayAppend(&tRecords, pInput, 3) ||
		!xrtArraySort(&tRecords, exampleCompareRecord)
	) {
		xrtArrayUnit(&tRecords);
		return 2;
	}
	for ( size_t i = 0; i < tRecords.Count; i++ ) {
		examplerecord* pRecord = (examplerecord*)xrtArrayGet(&tRecords, i);

		printf("id=%d priority=%d\n", pRecord->ID, pRecord->Priority);
	}
	xrtArrayUnit(&tRecords);
	return 0;
}
