#include "../internal/xrt_http.h"



#if defined(XRT_FEATURE_HTTP_TARGET)

/* 清空失败输出并发布统一的 request-target 值错误。 */
static bool __xrtHttpTargetValueFail(xhttptarget* pTarget)
{
	const xhttptarget Target = { 0 };

	memcpy(pTarget, &Target, sizeof(Target));
	__xrtErrorSetValue();
	return false;
}



/* 返回 ASCII 十六进制数字值，非法字节返回负数。 */
static int __xrtHttpTargetHex(unsigned char iByte)
{
	if ( (iByte >= (unsigned char)'0') &&
		(iByte <= (unsigned char)'9') ) {
		return (int)(iByte - (unsigned char)'0');
	}
	if ( (iByte >= (unsigned char)'a') &&
		(iByte <= (unsigned char)'f') ) {
		return 10 + (int)(iByte - (unsigned char)'a');
	}
	if ( (iByte >= (unsigned char)'A') &&
		(iByte <= (unsigned char)'F') ) {
		return 10 + (int)(iByte - (unsigned char)'A');
	}
	return -1;
}



/* 判断 scheme 首字节允许的 ASCII 字母。 */
static bool __xrtHttpTargetAlpha(unsigned char iByte)
{
	return ((iByte >= (unsigned char)'A') &&
		(iByte <= (unsigned char)'Z')) ||
		((iByte >= (unsigned char)'a') &&
		 (iByte <= (unsigned char)'z'));
}



/* 验证 absolute-form 使用的 RFC 3986 scheme。 */
static bool __xrtHttpTargetScheme(xstrview Scheme)
{
	if ( (Scheme.Size == 0) ||
		!__xrtHttpTargetAlpha((unsigned char)Scheme.Data[0]) ) {
		return false;
	}
	for ( size_t i = 1; i < Scheme.Size; i++ ) {
		unsigned char iByte = (unsigned char)Scheme.Data[i];

		if ( !__xrtHttpTargetAlpha(iByte) &&
			!((iByte >= (unsigned char)'0') &&
			  (iByte <= (unsigned char)'9')) &&
			(iByte != (unsigned char)'+') &&
			(iByte != (unsigned char)'-') &&
			(iByte != (unsigned char)'.') ) {
			return false;
		}
	}
	return true;
}



/* 验证 path 或 query 中可直接出现的 URI 字节和百分号转义。 */
static bool __xrtHttpTargetComponent(
	xstrview Text,
	bool bQuestion
)
{
	for ( size_t i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];
		bool bAlpha = __xrtHttpTargetAlpha(iByte);
		bool bDigit = (iByte >= (unsigned char)'0') &&
			(iByte <= (unsigned char)'9');
		bool bAllowed = bAlpha || bDigit ||
			(iByte == (unsigned char)'-') ||
			(iByte == (unsigned char)'.') ||
			(iByte == (unsigned char)'_') ||
			(iByte == (unsigned char)'~') ||
			(iByte == (unsigned char)'!') ||
			(iByte == (unsigned char)'$') ||
			(iByte == (unsigned char)'&') ||
			(iByte == (unsigned char)'\'') ||
			(iByte == (unsigned char)'(') ||
			(iByte == (unsigned char)')') ||
			(iByte == (unsigned char)'*') ||
			(iByte == (unsigned char)'+') ||
			(iByte == (unsigned char)',') ||
			(iByte == (unsigned char)';') ||
			(iByte == (unsigned char)'=') ||
			(iByte == (unsigned char)':') ||
			(iByte == (unsigned char)'@') ||
			(iByte == (unsigned char)'/') ||
			(bQuestion && (iByte == (unsigned char)'?'));

		if ( bAllowed ) {
			continue;
		}
		if ( (iByte != (unsigned char)'%') ||
			((i + 2u) >= Text.Size) ||
			(__xrtHttpTargetHex(
				(unsigned char)Text.Data[i + 1u]
			) < 0) || (__xrtHttpTargetHex(
				(unsigned char)Text.Data[i + 2u]
			) < 0) ) {
			return false;
		}
		i += 2u;
	}
	return true;
}



/* 从 target 主体中拆出 path 和可选 query。 */
static bool __xrtHttpTargetPathQuery(
	xstrview Text,
	size_t iStart,
	xhttptarget* pTarget
)
{
	size_t iQuery = XRT_NPOS;

	for ( size_t i = iStart; i < Text.Size; i++ ) {
		if ( Text.Data[i] == '#' ) {
			return false;
		}
		if ( (Text.Data[i] == '?') && (iQuery == XRT_NPOS) ) {
			iQuery = i;
		}
	}
	if ( iQuery == XRT_NPOS ) {
		pTarget->Path = (xstrview){
			Text.Data + iStart, Text.Size - iStart
		};
	} else {
		pTarget->Path = (xstrview){
			Text.Data + iStart, iQuery - iStart
		};
		pTarget->Query = (xstrview){
			Text.Data + iQuery + 1u, Text.Size - iQuery - 1u
		};
		pTarget->Flags |= XHTTP_TARGET_HAS_QUERY;
	}
	return __xrtHttpTargetComponent(pTarget->Path, false) &&
		__xrtHttpTargetComponent(pTarget->Query, true);
}



/* 按方法严格解析 HTTP 的四种 request-target。 */
XRT_API bool xrtHttpTargetParse(
	xstrview Method,
	xstrview Text,
	xhttptarget* pTarget
)
{
	xhttptarget Target = { 0 };
	xhttpmethod MethodCode;

	if ( !__xrtRangeValid(pTarget, sizeof(Target)) ||
		!__xrtHttpViewValid(Method) ||
		!__xrtHttpViewValid(Text) || __xrtRangesOverlap(
			pTarget, sizeof(Target), Method.Data, Method.Size
		) || __xrtRangesOverlap(
			pTarget, sizeof(Target), Text.Data, Text.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	MethodCode = xrtHttpMethodParse(Method);
	if ( (MethodCode == XHTTP_METHOD_INVALID) || (Text.Size == 0) ) {
		return __xrtHttpTargetValueFail(pTarget);
	}
	Target.Method = Method;
	Target.Text = Text;

	if ( MethodCode == XHTTP_METHOD_CONNECT ) {
		if ( !xrtHttpHostParse(Text, &Target.Host) ||
			(Target.Host.Host.Size == 0) ||
			((Target.Host.Flags & XHTTP_AUTHORITY_HAS_PORT) == 0) ||
			((Target.Host.Flags & XHTTP_AUTHORITY_PORT_EMPTY) != 0) ) {
			return __xrtHttpTargetValueFail(pTarget);
		}
		Target.Form = XHTTP_TARGET_AUTHORITY;
		Target.Flags = XHTTP_TARGET_HAS_AUTHORITY;
		Target.Authority = Text;
		memcpy(pTarget, &Target, sizeof(Target));
		return true;
	}

	if ( (Text.Size == 1u) && (Text.Data[0] == '*') ) {
		if ( MethodCode != XHTTP_METHOD_OPTIONS ) {
			return __xrtHttpTargetValueFail(pTarget);
		}
		Target.Form = XHTTP_TARGET_ASTERISK;
		memcpy(pTarget, &Target, sizeof(Target));
		return true;
	}

	if ( Text.Data[0] == '/' ) {
		if ( (Text.Size >= 2u) && (Text.Data[1] == '/') ) {
			return __xrtHttpTargetValueFail(pTarget);
		}
		if ( !__xrtHttpTargetPathQuery(Text, 0, &Target) ) {
			return __xrtHttpTargetValueFail(pTarget);
		}
		Target.Form = XHTTP_TARGET_ORIGIN;
		memcpy(pTarget, &Target, sizeof(Target));
		return true;
	}

	{
		size_t iColon = XRT_NPOS;
		size_t iPathStart;

		for ( size_t i = 0; i < Text.Size; i++ ) {
			if ( Text.Data[i] == ':' ) {
				iColon = i;
				break;
			}
			if ( (Text.Data[i] == '/') || (Text.Data[i] == '?') ||
				(Text.Data[i] == '#') ) {
				break;
			}
		}
		if ( iColon == XRT_NPOS ) {
			return __xrtHttpTargetValueFail(pTarget);
		}
		Target.Scheme = (xstrview){ Text.Data, iColon };
		if ( !__xrtHttpTargetScheme(Target.Scheme) ) {
			return __xrtHttpTargetValueFail(pTarget);
		}
		Target.Flags |= XHTTP_TARGET_HAS_SCHEME;
		iPathStart = iColon + 1u;
		if ( ((Text.Size - iPathStart) >= 2u) &&
			(Text.Data[iPathStart] == '/') &&
			(Text.Data[iPathStart + 1u] == '/') ) {
			size_t iAuthorityStart = iPathStart + 2u;
			size_t iAuthorityEnd = iAuthorityStart;

			while ( (iAuthorityEnd < Text.Size) &&
				(Text.Data[iAuthorityEnd] != '/') &&
				(Text.Data[iAuthorityEnd] != '?') &&
				(Text.Data[iAuthorityEnd] != '#') ) {
				iAuthorityEnd++;
			}
			Target.Authority = (xstrview){
				Text.Data + iAuthorityStart,
				iAuthorityEnd - iAuthorityStart
			};
			if ( !xrtHttpHostParse(Target.Authority, &Target.Host) ||
				(Target.Host.Host.Size == 0) ) {
				return __xrtHttpTargetValueFail(pTarget);
			}
			Target.Flags |= XHTTP_TARGET_HAS_AUTHORITY;
			iPathStart = iAuthorityEnd;
		}
		if ( !__xrtHttpTargetPathQuery(Text, iPathStart, &Target) ) {
			return __xrtHttpTargetValueFail(pTarget);
		}
	}
	Target.Form = XHTTP_TARGET_ABSOLUTE;
	memcpy(pTarget, &Target, sizeof(Target));
	return true;
}



/* 解析请求的有效 authority。 */
XRT_API bool xrtHttpTargetAuthority(
	const xhttptarget* pTarget,
	xstrview Host,
	xhttpauthority* pAuthority
)
{
	xhttptarget Target;
	xhttpauthority Output = { 0 };
	xstrview Authority;

	if ( !__xrtRangeValid(pAuthority, sizeof(Output)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtRangeValid(pTarget, sizeof(Target)) ||
		!__xrtHttpViewValid(Host) ) {
		memcpy(pAuthority, &Output, sizeof(Output));
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Target, pTarget, sizeof(Target));
	if ( __xrtRangesOverlap(
		pAuthority, sizeof(Output), pTarget, sizeof(Target)
	) || __xrtRangesOverlap(
		pAuthority, sizeof(Output), Target.Method.Data, Target.Method.Size
	) || __xrtRangesOverlap(
		pAuthority, sizeof(Output), Target.Text.Data, Target.Text.Size
	) || __xrtRangesOverlap(
		pAuthority, sizeof(Output), Host.Data, Host.Size
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	switch ( Target.Form ) {
		case XHTTP_TARGET_ORIGIN:
		case XHTTP_TARGET_ASTERISK:
			Authority = Host;
			break;

		case XHTTP_TARGET_ABSOLUTE:
		case XHTTP_TARGET_AUTHORITY:
			if ( (Target.Flags & XHTTP_TARGET_HAS_AUTHORITY) == 0 ) {
				memcpy(pAuthority, &Output, sizeof(Output));
				__xrtErrorSetValue();
				return false;
			}
			Authority = Target.Authority;
			break;

		default:
			memcpy(pAuthority, &Output, sizeof(Output));
			__xrtErrorSetInvalidArgument();
			return false;
	}
	if ( !xrtHttpHostParse(Authority, &Output) ) {
		memset(&Output, 0, sizeof(Output));
		memcpy(pAuthority, &Output, sizeof(Output));
		return false;
	}
	memcpy(pAuthority, &Output, sizeof(Output));
	return true;
}



#endif
