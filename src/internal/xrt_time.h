#ifndef XRT_INTERNAL_TIME_H
#define XRT_INTERNAL_TIME_H

#include "xrt_internal.h"



#if defined(XRT_FEATURE_TIME)

/* 内部日期构造结果用于让解析器延迟到公共边界再报告一次错误。 */
typedef enum __xrt_time_make_status {
	__XRT_TIME_MAKE_OK = 0,
	__XRT_TIME_MAKE_OFFSET,
	__XRT_TIME_MAKE_COMPONENT,
	__XRT_TIME_MAKE_OVERFLOW
} __xrt_time_make_status;



/* 无错误副作用地按显式 UTC 偏移构造绝对时间。 */
__xrt_time_make_status __xrtTimeMakeValue(
	const xdatetime* pDateTime, xtime* pTime);

/* 设置时间模块的结构化错误。 */
void __xrtTimeSetError(xerrkind Kind, xtimeerror Code,
	cstr sOperation, cstr sMessage, int iSystemCode);



/* 检查 int64 加法，失败时不修改输出。 */
bool __xrtTimeAddChecked(int64 iLeft, int64 iRight, int64* pResult);



/* 检查 int64 减法，失败时不修改输出。 */
bool __xrtTimeSubChecked(int64 iLeft, int64 iRight, int64* pResult);



/* 检查 int64 乘法，失败时不修改输出。 */
bool __xrtTimeMulChecked(int64 iLeft, int64 iRight, int64* pResult);



/* 执行向负无穷取整的有符号除法，除数必须为正数。 */
int64 __xrtTimeFloorDiv(int64 iValue, int64 iDivisor);



/* 把 Unix 微秒拆成天数和当日微秒。 */
void __xrtTimeSplitDay(xtime iTime, int64* pDays, int64* pDayTime);



/* 把 Unix Epoch 天数转换为 Gregorian 日期。 */
void __xrtTimeCivilFromDays(int64 iDays, int64* pYear, int* pMonth, int* pDay);



/* 把 Gregorian 日期转换为 Unix Epoch 天数。 */
bool __xrtTimeDaysFromCivil(int64 iYear, int iMonth, int iDay, int64* pDays);



/* 无错误副作用地按系统本地时区分解时间。 */
bool __xrtTimeLocalParts(xtime iTime, xdatetime* pDateTime, int* pSystemCode);

#endif



#endif
