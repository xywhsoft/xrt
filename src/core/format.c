#include <stdio.h>

#include "../internal/xrt_format.h"



#if defined(XRT_FEATURE_ERROR_FORMAT) || defined(XRT_FEATURE_STRING_FORMAT)

#if defined(_MSC_VER)
	#define XRT_FORMAT_VA_COPY(Destination, Source) ((Destination) = (Source))
	#define XRT_FORMAT_VA_END(Args) ((void)(Args))
#else
	#define XRT_FORMAT_VA_COPY(Destination, Source) va_copy((Destination), (Source))
	#define XRT_FORMAT_VA_END(Args) va_end(Args)
#endif



/* 跳过 printf 参数位置中的十进制数字。 */
static cstr __xrtFormatSkipDigits(cstr sText)
{
	while ( (*sText >= '0') && (*sText <= '9') ) {
		sText++;
	}
	return sText;
}



/* 拒绝会写入调用方内存且在双遍处理中产生副作用的 %n。 */
bool __xrtFormatSafe(cstr sFormat)
{
	cstr sText = sFormat;

	while ( *sText != 0 ) {
		if ( *sText++ != '%' ) {
			continue;
		}
		if ( *sText == '%' ) {
			sText++;
			continue;
		}

		/* 解析可选参数位置、标志、宽度和精度。 */
		{
			cstr sPosition = __xrtFormatSkipDigits(sText);

			if ( *sPosition == '$' ) {
				sText = sPosition + 1;
			}
		}
		while ( (*sText == '-') || (*sText == '+') || (*sText == ' ') ||
			(*sText == '#') || (*sText == '0') || (*sText == '\'') ) {
			sText++;
		}
		if ( *sText == '*' ) {
			cstr sPosition;

			sText++;
			sPosition = __xrtFormatSkipDigits(sText);
			if ( *sPosition == '$' ) {
				sText = sPosition + 1;
			}
		} else {
			sText = __xrtFormatSkipDigits(sText);
		}
		if ( *sText == '.' ) {
			sText++;
			if ( *sText == '*' ) {
				cstr sPosition;

				sText++;
				sPosition = __xrtFormatSkipDigits(sText);
				if ( *sPosition == '$' ) {
					sText = sPosition + 1;
				}
			} else {
				sText = __xrtFormatSkipDigits(sText);
			}
		}

		/* 跳过标准长度修饰符以及常见的 MSVC I32/I64 修饰符。 */
		if ( ((*sText == 'h') && (sText[1] == 'h')) ||
			 ((*sText == 'l') && (sText[1] == 'l')) ) {
			sText += 2;
		} else if ( (*sText == 'h') || (*sText == 'l') || (*sText == 'j') ||
			(*sText == 'z') || (*sText == 't') || (*sText == 'L') ||
			(*sText == 'w') ) {
			sText++;
		} else if ( (*sText == 'I') &&
			(((sText[1] == '3') && (sText[2] == '2')) ||
			 ((sText[1] == '6') && (sText[2] == '4'))) ) {
			sText += 3;
		}
		if ( *sText == 'n' ) {
			return false;
		}
		if ( *sText != 0 ) {
			sText++;
		}
	}
	return true;
}



/* 在不消耗调用方参数列表的情况下计算格式化长度。 */
int __xrtFormatMeasure(cstr sFormat, va_list Args)
{
	va_list Copy;
	int iSize;

	XRT_FORMAT_VA_COPY(Copy, Args);
	#if defined(_MSC_VER) || \
		(defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64)))
		iSize = _vscprintf(sFormat, Copy);
	#else
		iSize = vsnprintf(NULL, 0, sFormat, Copy);
	#endif
	XRT_FORMAT_VA_END(Copy);
	return iSize;
}



/* 在不消耗调用方参数列表的情况下写入格式化文本。 */
int __xrtFormatWrite(
	str sOutput,
	size_t iCapacity,
	cstr sFormat,
	va_list Args
)
{
	va_list Copy;
	int iSize;

	XRT_FORMAT_VA_COPY(Copy, Args);
	iSize = vsnprintf(sOutput, iCapacity, sFormat, Copy);
	XRT_FORMAT_VA_END(Copy);
	return iSize;
}

#undef XRT_FORMAT_VA_COPY
#undef XRT_FORMAT_VA_END

#endif
