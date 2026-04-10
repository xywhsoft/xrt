typedef enum
{
	XSON_PARSE_RESULT_FAIL = -1,
	XSON_PARSE_RESULT_OK = 0,
	XSON_PARSE_RESULT_SKIP = 1,
	XSON_PARSE_RESULT_NONE = 2
} xson_parse_result_t;

typedef enum
{
	XSON_CONTAINER_AUTO = 0,
	XSON_CONTAINER_ARRAY = 1,
	XSON_CONTAINER_LIST = 2,
	XSON_CONTAINER_DICT = 3,
	XSON_CONTAINER_SET = 4
} xson_container_t;

typedef enum
{
	XSON_WRITE_RESULT_FAIL = -1,
	XSON_WRITE_RESULT_OK = 0,
	XSON_WRITE_RESULT_SKIP = 1
} xson_write_result_t;

typedef struct
{
	json_parse_t tJSON;
	uint32 iFlags;
} xson_parse_t;

typedef struct
{
	char* sText;
	size_t iSize;
	size_t iUsed;
	int bFormat;
	uint32 iFlags;
} xson_print_t;


// 内部函数：_xson_is_ident_start
static bool _xson_is_ident_start(char ch)
{
	return (((ch >= 'a') && (ch <= 'z')) || ((ch >= 'A') && (ch <= 'Z')) || (ch == '_'));
}


// 内部函数：_xson_is_ident_char
static bool _xson_is_ident_char(char ch)
{
	return _xson_is_ident_start(ch) || ((ch >= '0') && (ch <= '9'));
}


// 内部函数：查看
static char _xson_peek(xson_parse_t* pParse)
{
	if ( pParse == NULL ) {
		return '\0';
	}
	if ( pParse->tJSON.offset >= pParse->tJSON.size ) {
		return '\0';
	}
	return pParse->tJSON.str[pParse->tJSON.offset];
}


// 内部函数：_xson_skip_blank
static void _xson_skip_blank(xson_parse_t* pParse)
{
	if ( pParse == NULL ) {
		return;
	}
	pParse->tJSON.skip_blank(&pParse->tJSON);
}


// 内部函数：_xson_match_prefix
static bool _xson_match_prefix(xson_parse_t* pParse, const char* sName, char chNext)
{
	size_t iLen;
	size_t iRemain;

	if ( pParse == NULL || sName == NULL ) {
		return FALSE;
	}

	iLen = strlen(__xrt_cstr(sName));
	iRemain = pParse->tJSON.size - pParse->tJSON.offset;
	if ( iRemain <= iLen ) {
		return FALSE;
	}

	if ( memcmp(pParse->tJSON.str + pParse->tJSON.offset, sName, iLen) != 0 ) {
		return FALSE;
	}

	return pParse->tJSON.str[pParse->tJSON.offset + iLen] == chNext;
}


// 内部函数：_xson_trim_slice
static void _xson_trim_slice(const char** ppText, size_t* pSize)
{
	const char* sText;
	size_t iSize;

	if ( ppText == NULL || pSize == NULL ) {
		return;
	}

	sText = *ppText;
	iSize = *pSize;
	while ( iSize > 0 && IS_BLANK((unsigned char)*sText) ) {
		sText++;
		iSize--;
	}
	while ( iSize > 0 && IS_BLANK((unsigned char)sText[iSize - 1]) ) {
		iSize--;
	}

	*ppText = sText;
	*pSize = iSize;
}


// 内部函数：释放 JSON 字符串
static void _xson_free_json_string(json_string_t* pStr)
{
	if ( pStr == NULL ) {
		return;
	}
	if ( pStr->info.alloced && pStr->str ) {
		json_free(pStr->str);
	}
	memset(pStr, 0, sizeof(*pStr));
}


// 内部函数：跳过字符串
static int _xson_skip_string(xson_parse_t* pParse, char chEnd)
{
	char* sText = NULL;
	json_strinfo_t tInfo = { 0 };

	if ( pParse == NULL ) {
		return -1;
	}

	if ( pParse->tJSON.parse_string(&pParse->tJSON, chEnd, &sText, &tInfo, FALSE) < 0 ) {
		return -1;
	}
	if ( tInfo.alloced && sText ) {
		json_free(sText);
	}
	if ( _xson_peek(pParse) != chEnd ) {
		return -1;
	}
	pParse->tJSON.offset++;
	return 0;
}


// 内部函数：跳过组
// SON 组跳过算法：递归跳过匹配的括号组，正确处理嵌套和字符串内容
static int _xson_skip_group(xson_parse_t* pParse)
{
	char ch;
	char chEnd;

	if ( pParse == NULL ) {
		return -1;
	}

	// 根据开始括号确定对应的结束括号
	ch = _xson_peek(pParse);
	if ( ch == '[' ) {
		chEnd = ']';
	} else if ( ch == '{' ) {
		chEnd = '}';
	} else if ( ch == '(' ) {
		chEnd = ')';
	} else {
		return -1;
	}

	// 跳过开始括号
	pParse->tJSON.offset++;

	// 逐字符扫描，寻找匹配的结束括号
	while ( pParse->tJSON.offset < pParse->tJSON.size ) {
		ch = _xson_peek(pParse);

		// 遇到结束括号则成功返回
		if ( ch == chEnd ) {
			pParse->tJSON.offset++;
			return 0;
		}

		// 遇到双引号字符串，跳过字符串内容
		if ( ch == '\"' ) {
			pParse->tJSON.offset++;
			if ( _xson_skip_string(pParse, '\"') < 0 ) {
				return -1;
			}
			continue;
		}

		// 遇到单引号字符串（特殊引号模式），跳过字符串内容
		#if JSON_PARSE_SPECIAL_QUOTES
		if ( ch == '\'' ) {
			pParse->tJSON.offset++;
			if ( _xson_skip_string(pParse, '\'') < 0 ) {
				return -1;
			}
			continue;
		}
		#endif

		// 遇到嵌套括号组，递归跳过
		if ( ch == '[' || ch == '{' || ch == '(' ) {
			if ( _xson_skip_group(pParse) < 0 ) {
				return -1;
			}
			continue;
		}

		// 普通字符，直接跳过
		pParse->tJSON.offset++;
	}

	// 未找到匹配的结束括号
	return -1;
}

// [R12] 探测当前位置是否为列表格式（整数索引:值）
static bool _xson_probe_list(xson_parse_t* pParse)
{
	size_t iSaved;
	char ch;

	if ( pParse == NULL ) {
		return FALSE;
	}

	// 保存当前位置以便探测后恢复
	iSaved = pParse->tJSON.offset;
	_xson_skip_blank(pParse);

	// 跳过可选的正负号
	ch = _xson_peek(pParse);
	if ( ch == '+' || ch == '-' ) {
		pParse->tJSON.offset++;
	}

	// 必须以数字开头
	ch = _xson_peek(pParse);
	if ( !IS_DIGIT((unsigned char)ch) ) {
		pParse->tJSON.offset = iSaved;
		return FALSE;
	}

	// 跳过数字部分
	while ( IS_DIGIT((unsigned char)_xson_peek(pParse)) ) {
		pParse->tJSON.offset++;
	}

	// 数字后面必须紧跟冒号，才是列表格式
	_xson_skip_blank(pParse);
	ch = _xson_peek(pParse);
	pParse->tJSON.offset = iSaved;
	return ch == ':';
}

// [R12] 探测当前位置是否为字典格式（键:值）
static bool _xson_probe_dict(xson_parse_t* pParse)
{
	size_t iSaved;
	char ch;

	if ( pParse == NULL ) {
		return FALSE;
	}

	// 保存当前位置以便探测后恢复
	iSaved = pParse->tJSON.offset;
	_xson_skip_blank(pParse);
	ch = _xson_peek(pParse);

	// 双引号键：跳过字符串后检查是否紧跟冒号
	if ( ch == '\"' ) {
		pParse->tJSON.offset++;
		if ( _xson_skip_string(pParse, '\"') < 0 ) {
			pParse->tJSON.offset = iSaved;
			return FALSE;
		}
		_xson_skip_blank(pParse);
		ch = _xson_peek(pParse);
		pParse->tJSON.offset = iSaved;
		return ch == ':';
	}

	#if JSON_PARSE_SPECIAL_QUOTES
	// 单引号键：跳过字符串后检查是否紧跟冒号
	if ( ch == '\'' ) {
		pParse->tJSON.offset++;
		if ( _xson_skip_string(pParse, '\'') < 0 ) {
			pParse->tJSON.offset = iSaved;
			return FALSE;
		}
		_xson_skip_blank(pParse);
		ch = _xson_peek(pParse);
		pParse->tJSON.offset = iSaved;
		return ch == ':';
	}
	// 空键：直接以冒号开头
	if ( ch == ':' ) {
		pParse->tJSON.offset = iSaved;
		return JSON_PARSE_EMPTY_KEY ? TRUE : FALSE;
	}
	// 裸键（无引号）：扫描到分隔符后检查是否紧跟冒号
	if ( ch != '\0' ) {
		while ( pParse->tJSON.offset < pParse->tJSON.size ) {
			ch = _xson_peek(pParse);
			if ( ch == ':' || ch == ',' || ch == '}' || IS_BLANK((unsigned char)ch) ) {
				break;
			}
			pParse->tJSON.offset++;
		}
		_xson_skip_blank(pParse);
		ch = _xson_peek(pParse);
		pParse->tJSON.offset = iSaved;
		return ch == ':';
	}
	#endif

	// 无法识别的格式，恢复位置
	pParse->tJSON.offset = iSaved;
	return FALSE;
}


// 内部函数：_xson_parse_list_index
// [R12] 解析列表索引，格式为 "整数:" ，支持正负号和溢出检测
static int _xson_parse_list_index(xson_parse_t* pParse, int64* pIndex)
{
	size_t iSaved;
	uint64 iLimit;
	uint64 iValue;
	bool bNegative;
	char ch;

	if ( pParse == NULL || pIndex == NULL ) {
		return -1;
	}

	// 保存位置以便失败时回退
	iSaved = pParse->tJSON.offset;
	_xson_skip_blank(pParse);

	// 处理可选的正负号
	bNegative = FALSE;
	ch = _xson_peek(pParse);
	if ( ch == '+' || ch == '-' ) {
		bNegative = (ch == '-');
		pParse->tJSON.offset++;
	}

	// 必须以数字开头
	if ( !IS_DIGIT((unsigned char)_xson_peek(pParse)) ) {
		pParse->tJSON.offset = iSaved;
		return -1;
	}

	// 逐位解析数字，进行溢出检测
	iLimit = bNegative ? ((uint64)INT64_MAX + 1ULL) : (uint64)INT64_MAX;
	iValue = 0;
	while ( IS_DIGIT((unsigned char)_xson_peek(pParse)) ) {
		uint64 iDigit = (uint64)(_xson_peek(pParse) - '0');
		if ( iValue > ((iLimit - iDigit) / 10ULL) ) {
			pParse->tJSON.offset = iSaved;
			return -1;
		}
		iValue = (iValue * 10ULL) + iDigit;
		pParse->tJSON.offset++;
	}

	// 数字后面必须紧跟冒号
	_xson_skip_blank(pParse);
	if ( _xson_peek(pParse) != ':' ) {
		pParse->tJSON.offset = iSaved;
		return -1;
	}
	pParse->tJSON.offset++;

	// 根据符号位转换为最终结果，特殊处理 INT64_MIN
	if ( bNegative ) {
		if ( iValue == ((uint64)INT64_MAX + 1ULL) ) {
			*pIndex = INT64_MIN;
		} else {
			*pIndex = -(int64)iValue;
		}
	} else {
		*pIndex = (int64)iValue;
	}

	return 0;
}


// 内部函数：_xson_parse_time_text
// 日期时间文本解析算法：从 "YYYY-MM-DD HH:MM:SS" 格式文本中提取日期时间分量并序列化为 xtime
static bool _xson_parse_time_text(const char* sText, size_t iSize, xtime* pTime)
{
	size_t iPos = 0;
	int64 iYear = 0;
	int iMonth;
	int iDay;
	int iHour;
	int iMinute;
	int iSecond;
	bool bNegative = FALSE;

	if ( sText == NULL || pTime == NULL ) {
		return FALSE;
	}

	// 去除首尾空白字符
	_xson_trim_slice(&sText, &iSize);
	if ( iSize < 19 ) {
		return FALSE;
	}

	// 处理可选的负号（表示公元前年份）
	if ( sText[iPos] == '-' ) {
		bNegative = TRUE;
		iPos++;
	}

	// 解析年份部分（一位或多位数字）
	if ( iPos >= iSize || !IS_DIGIT((unsigned char)sText[iPos]) ) {
		return FALSE;
	}
	while ( iPos < iSize && IS_DIGIT((unsigned char)sText[iPos]) ) {
		iYear = (iYear * 10) + (sText[iPos] - '0');
		iPos++;
	}
	if ( bNegative ) {
		iYear = -iYear;
	}

	// 验证剩余部分固定长度为15（"-MM-DD HH:MM:SS"）
	if ( (iSize - iPos) != 15 ) {
		return FALSE;
	}

	// 验证分隔符位置：pos处为'-'，pos+3处为'-'，pos+6处为' '，pos+9处为':'，pos+12处为':'
	if ( sText[iPos] != '-' || sText[iPos + 3] != '-' || sText[iPos + 6] != ' ' ||
		sText[iPos + 9] != ':' || sText[iPos + 12] != ':' ) {
		return FALSE;
	}

	// 验证所有数字位均为有效数字
	if ( !IS_DIGIT((unsigned char)sText[iPos + 1]) || !IS_DIGIT((unsigned char)sText[iPos + 2]) ||
		!IS_DIGIT((unsigned char)sText[iPos + 4]) || !IS_DIGIT((unsigned char)sText[iPos + 5]) ||
		!IS_DIGIT((unsigned char)sText[iPos + 7]) || !IS_DIGIT((unsigned char)sText[iPos + 8]) ||
		!IS_DIGIT((unsigned char)sText[iPos + 10]) || !IS_DIGIT((unsigned char)sText[iPos + 11]) ||
		!IS_DIGIT((unsigned char)sText[iPos + 13]) || !IS_DIGIT((unsigned char)sText[iPos + 14]) ) {
		return FALSE;
	}

	// 提取月、日、时、分、秒各分量
	iMonth = ((sText[iPos + 1] - '0') * 10) + (sText[iPos + 2] - '0');
	iDay = ((sText[iPos + 4] - '0') * 10) + (sText[iPos + 5] - '0');
	iHour = ((sText[iPos + 7] - '0') * 10) + (sText[iPos + 8] - '0');
	iMinute = ((sText[iPos + 10] - '0') * 10) + (sText[iPos + 11] - '0');
	iSecond = ((sText[iPos + 13] - '0') * 10) + (sText[iPos + 14] - '0');

	// 将各分量序列化为 xtime 值
	*pTime = xrtDateTimeSerial(iYear, iMonth, iDay, iHour, iMinute, iSecond);
	return TRUE;
}


// 内部函数：_xson_base64_decoded_size
static size_t _xson_base64_decoded_size(const char* sText, size_t iSize)
{
	size_t iRet;

	if ( sText == NULL || iSize == 0 || (iSize % 4) != 0 ) {
		return 0;
	}

	iRet = (iSize / 4) * 3;
	if ( sText[iSize - 1] == '=' ) {
		iRet--;
	}
	if ( sText[iSize - 2] == '=' ) {
		iRet--;
	}
	return iRet;
}

static xson_parse_result_t _xson_parse_value(xson_parse_t* pParse, xvalue* ppVal);


// 内部函数：_xson_parse_array_like
// SON 类数组解析算法：解析 [...] 语法，自动检测内容格式决定创建 Array 还是 List
static xson_parse_result_t _xson_parse_array_like(xson_parse_t* pParse, int iKind, xvalue* ppVal)
{
	xvalue pResult;
	char ch;

	if ( pParse == NULL || ppVal == NULL || _xson_peek(pParse) != '[' ) {
		return XSON_PARSE_RESULT_FAIL;
	}

	// 跳过 '[' 并处理空数组/空列表
	pParse->tJSON.offset++;
	_xson_skip_blank(pParse);
	if ( _xson_peek(pParse) == ']' ) {
		pParse->tJSON.offset++;
		if ( iKind == XSON_CONTAINER_LIST ) {
			*ppVal = xvoCreateList();
		} else {
			*ppVal = xvoCreateArray();
		}
		return (*ppVal != NULL) ? XSON_PARSE_RESULT_OK : XSON_PARSE_RESULT_FAIL;
	}

	// 自动模式：通过探测首个元素判断是列表（索引:值）还是普通数组
	if ( iKind == XSON_CONTAINER_AUTO ) {
		iKind = _xson_probe_list(pParse) ? XSON_CONTAINER_LIST : XSON_CONTAINER_ARRAY;
	}

	// 根据容器类型创建对应的结果对象
	if ( iKind == XSON_CONTAINER_LIST ) {
		pResult = xvoCreateList();
	} else {
		pResult = xvoCreateArray();
	}
	if ( pResult == NULL ) {
		return XSON_PARSE_RESULT_FAIL;
	}

	// 循环解析每个元素
	for ( ;; ) {
		xson_parse_result_t iRet;
		xvalue pItem = NULL;
		int64 iIndex = 0;

		// 列表模式需要先解析索引值
		if ( iKind == XSON_CONTAINER_LIST ) {
			if ( _xson_parse_list_index(pParse, &iIndex) < 0 ) {
				xvoUnref(pResult);
				return XSON_PARSE_RESULT_FAIL;
			}
		}

		// 解析元素值
		iRet = _xson_parse_value(pParse, &pItem);
		if ( iRet == XSON_PARSE_RESULT_FAIL ) {
			xvoUnref(pResult);
			return XSON_PARSE_RESULT_FAIL;
		}
		if ( iRet == XSON_PARSE_RESULT_OK ) {
			bool bStored;
			if ( iKind == XSON_CONTAINER_LIST ) {
				// 列表模式：按索引存储
				bStored = xvoListSetValue(pResult, iIndex, pItem, TRUE);
			} else {
				// 数组模式：追加到末尾
				bStored = xvoArrayAppendValue(pResult, pItem, TRUE);
			}
			if ( bStored == FALSE ) {
				xvoUnref(pItem);
				xvoUnref(pResult);
				return XSON_PARSE_RESULT_FAIL;
			}
		}

		// 检查分隔符或结束括号
		_xson_skip_blank(pParse);
		ch = _xson_peek(pParse);
		if ( ch == ',' ) {
			pParse->tJSON.offset++;
			_xson_skip_blank(pParse);
			#if JSON_PARSE_LAST_COMMA
			// 允许最后一个元素后有多余的逗号
			if ( _xson_peek(pParse) == ']' ) {
				break;
			}
			#endif
			continue;
		}
		if ( ch == ']' ) {
			break;
		}

		// 既不是逗号也不是结束括号，格式错误
		xvoUnref(pResult);
		return XSON_PARSE_RESULT_FAIL;
	}

	// 跳过结束括号 ']'，返回结果
	pParse->tJSON.offset++;
	*ppVal = pResult;
	return XSON_PARSE_RESULT_OK;
}


// 内部函数：_xson_parse_object_like
// SON 类对象解析算法：解析 {...} 语法，自动检测内容格式决定创建 Table(字典) 还是 Coll(集合)
static xson_parse_result_t _xson_parse_object_like(xson_parse_t* pParse, int iKind, xvalue* ppVal)
{
	xvalue pResult;
	char ch;

	if ( pParse == NULL || ppVal == NULL || _xson_peek(pParse) != '{' ) {
		return XSON_PARSE_RESULT_FAIL;
	}

	// 跳过 '{' 并处理空对象/空集合
	pParse->tJSON.offset++;
	_xson_skip_blank(pParse);
	if ( _xson_peek(pParse) == '}' ) {
		pParse->tJSON.offset++;
		if ( iKind == XSON_CONTAINER_SET ) {
			*ppVal = xvoCreateColl();
		} else {
			*ppVal = xvoCreateTable();
		}
		return (*ppVal != NULL) ? XSON_PARSE_RESULT_OK : XSON_PARSE_RESULT_FAIL;
	}

	// 自动模式：通过探测首个元素判断是字典（键:值）还是集合（仅值）
	if ( iKind == XSON_CONTAINER_AUTO ) {
		iKind = _xson_probe_dict(pParse) ? XSON_CONTAINER_DICT : XSON_CONTAINER_SET;
	}

	// 根据容器类型创建对应的结果对象
	if ( iKind == XSON_CONTAINER_DICT ) {
		pResult = xvoCreateTable();
	} else {
		pResult = xvoCreateColl();
	}
	if ( pResult == NULL ) {
		return XSON_PARSE_RESULT_FAIL;
	}

	// 循环解析每个键值对或元素
	for ( ;; ) {
		xson_parse_result_t iRet;
		xvalue pItem = NULL;

		if ( iKind == XSON_CONTAINER_DICT ) {
			// 字典模式：先解析键名，再解析值
			json_string_t tKey = { 0 };
			bool bStored;

			if ( _json_parse_key(&pParse->tJSON, &tKey) < 0 ) {
				xvoUnref(pResult);
				return XSON_PARSE_RESULT_FAIL;
			}

			// 解析键对应的值
			iRet = _xson_parse_value(pParse, &pItem);
			if ( iRet == XSON_PARSE_RESULT_FAIL ) {
				_xson_free_json_string(&tKey);
				xvoUnref(pResult);
				return XSON_PARSE_RESULT_FAIL;
			}
			if ( iRet == XSON_PARSE_RESULT_OK ) {
				// 以键值对形式存入字典
				bStored = xvoTableSetValue(pResult, tKey.str, tKey.info.len, pItem, TRUE);
				if ( bStored == FALSE ) {
					_xson_free_json_string(&tKey);
					xvoUnref(pItem);
					xvoUnref(pResult);
					return XSON_PARSE_RESULT_FAIL;
				}
			}

			_xson_free_json_string(&tKey);
		} else {
			// 集合模式：仅解析值，无键名
			bool bStored;

			iRet = _xson_parse_value(pParse, &pItem);
			if ( iRet == XSON_PARSE_RESULT_FAIL ) {
				xvoUnref(pResult);
				return XSON_PARSE_RESULT_FAIL;
			}
			if ( iRet == XSON_PARSE_RESULT_OK ) {
				// 以元素值形式存入集合
				bStored = xvoCollSetValue(pResult, pItem, TRUE);
				if ( bStored == FALSE ) {
					xvoUnref(pItem);
					xvoUnref(pResult);
					return XSON_PARSE_RESULT_FAIL;
				}
			}
		}

		// 检查分隔符或结束括号
		_xson_skip_blank(pParse);
		ch = _xson_peek(pParse);
		if ( ch == ',' ) {
			pParse->tJSON.offset++;
			_xson_skip_blank(pParse);
			#if JSON_PARSE_LAST_COMMA
			// 允许最后一个元素后有多余的逗号
			if ( _xson_peek(pParse) == '}' ) {
				break;
			}
			#endif
			continue;
		}
		if ( ch == '}' ) {
			break;
		}

		// 既不是逗号也不是结束括号，格式错误
		xvoUnref(pResult);
		return XSON_PARSE_RESULT_FAIL;
	}

	// 跳过结束括号 '}'，返回结果
	pParse->tJSON.offset++;
	*ppVal = pResult;
	return XSON_PARSE_RESULT_OK;
}

// [R12] 解析 time(...) 格式的时间值，提取括号内文本并解析为 xtime
static xson_parse_result_t _xson_parse_time_value(xson_parse_t* pParse, xvalue* ppVal)
{
	const char* sText;
	size_t iSize;
	xtime tValue;

	if ( pParse == NULL || ppVal == NULL || _xson_peek(pParse) != '(' ) {
		return XSON_PARSE_RESULT_FAIL;
	}

	// 跳过 '(' 并记录文本起始位置
	pParse->tJSON.offset++;
	sText = pParse->tJSON.str + pParse->tJSON.offset;

	// 扫描到 ')' 为止，计算括号内文本长度
	iSize = 0;
	while ( pParse->tJSON.offset < pParse->tJSON.size ) {
		if ( _xson_peek(pParse) == ')' ) {
			break;
		}
		pParse->tJSON.offset++;
		iSize++;
	}

	// 必须找到结束括号
	if ( _xson_peek(pParse) != ')' ) {
		return XSON_PARSE_RESULT_FAIL;
	}
	pParse->tJSON.offset++;

	// 将括号内文本解析为时间值
	if ( _xson_parse_time_text(sText, iSize, &tValue) == FALSE ) {
		return XSON_PARSE_RESULT_FAIL;
	}

	// 创建时间类型的 xvalue 对象
	*ppVal = xvoCreateTime(tValue);
	return (*ppVal != NULL) ? XSON_PARSE_RESULT_OK : XSON_PARSE_RESULT_FAIL;
}

// [R12] 解析 class(...) 格式的二进制类数据，括号内为 Base64 编码的结构体数据
static xson_parse_result_t _xson_parse_class_value(xson_parse_t* pParse, xvalue* ppVal)
{
	const char* sText;
	size_t iSize;
	size_t iDecodedSize;
	ptr pData;
	xvalue pVal;

	if ( pParse == NULL || ppVal == NULL || _xson_peek(pParse) != '(' ) {
		return XSON_PARSE_RESULT_FAIL;
	}

	// 跳过 '(' 并记录文本起始位置
	pParse->tJSON.offset++;
	sText = pParse->tJSON.str + pParse->tJSON.offset;

	// 扫描到 ')' 为止，计算括号内文本长度
	iSize = 0;
	while ( pParse->tJSON.offset < pParse->tJSON.size ) {
		if ( _xson_peek(pParse) == ')' ) {
			break;
		}
		pParse->tJSON.offset++;
		iSize++;
	}

	// 必须找到结束括号
	if ( _xson_peek(pParse) != ')' ) {
		return XSON_PARSE_RESULT_FAIL;
	}
	pParse->tJSON.offset++;

	// 去除首尾空白，计算 Base64 解码后的数据大小
	_xson_trim_slice(&sText, &iSize);
	iDecodedSize = _xson_base64_decoded_size(sText, iSize);
	if ( iDecodedSize == 0 ) {
		return XSON_PARSE_RESULT_FAIL;
	}

	// 执行 Base64 解码
	pData = xrtBase64Decode((str)sText, iSize, NULL);
	if ( pData == NULL || pData == xCore.sNull ) {
		return XSON_PARSE_RESULT_FAIL;
	}

	// 创建 Class 类型的 xvalue 并复制解码数据
	pVal = xvoCreateClass((uint32)iDecodedSize);
	if ( pVal == NULL ) {
		xrtFree(pData);
		return XSON_PARSE_RESULT_FAIL;
	}

	// 将解码后的二进制数据复制到 Class 对象中
	memcpy(pVal->vStruct, pData, iDecodedSize);
	xrtFree(pData);
	*ppVal = pVal;
	return XSON_PARSE_RESULT_OK;
}

// SON 前缀值解析算法：根据标识符前缀分发到对应的类型解析器，支持 array/list/dict/set/time/class 等
static xson_parse_result_t _xson_parse_prefixed_value(xson_parse_t* pParse, xvalue* ppVal)
{
	size_t iNameLen;
	char chNext;

	if ( pParse == NULL || ppVal == NULL ) {
		return XSON_PARSE_RESULT_FAIL;
	}

	// 匹配各已知前缀并分发到对应解析器
	if ( _xson_match_prefix(pParse, "array", '[') ) {
		pParse->tJSON.offset += 5;
		return _xson_parse_array_like(pParse, XSON_CONTAINER_ARRAY, ppVal);
	}
	if ( _xson_match_prefix(pParse, "list", '[') ) {
		pParse->tJSON.offset += 4;
		return _xson_parse_array_like(pParse, XSON_CONTAINER_LIST, ppVal);
	}
	if ( _xson_match_prefix(pParse, "dict", '{') ) {
		pParse->tJSON.offset += 4;
		return _xson_parse_object_like(pParse, XSON_CONTAINER_DICT, ppVal);
	}
	if ( _xson_match_prefix(pParse, "set", '{') ) {
		pParse->tJSON.offset += 3;
		return _xson_parse_object_like(pParse, XSON_CONTAINER_SET, ppVal);
	}
	if ( _xson_match_prefix(pParse, "time", '(') ) {
		pParse->tJSON.offset += 4;
		return _xson_parse_time_value(pParse, ppVal);
	}
	if ( _xson_match_prefix(pParse, "class", '(') ) {
		pParse->tJSON.offset += 5;
		return _xson_parse_class_value(pParse, ppVal);
	}

	// 未匹配到已知前缀，检查是否为带前缀的未知类型
	iNameLen = 0;
	while ( (pParse->tJSON.offset + iNameLen) < pParse->tJSON.size &&
		_xson_is_ident_char(pParse->tJSON.str[pParse->tJSON.offset + iNameLen]) ) {
		iNameLen++;
	}
	if ( (pParse->tJSON.offset + iNameLen) < pParse->tJSON.size ) {
		chNext = pParse->tJSON.str[pParse->tJSON.offset + iNameLen];
	} else {
		chNext = '\0';
	}

	// 未知前缀类型后跟括号：根据标志决定是跳过还是报错
	if ( iNameLen > 0 && (chNext == '[' || chNext == '{' || chNext == '(') ) {
		if ( (pParse->iFlags & XSON_F_IGNORE_UNSUPPORTED_DECODE) == 0 ) {
			return XSON_PARSE_RESULT_FAIL;
		}
		// 忽略不支持类型的解码模式：跳过整个组
		pParse->tJSON.offset += iNameLen;
		if ( _xson_skip_group(pParse) < 0 ) {
			return XSON_PARSE_RESULT_FAIL;
		}
		return XSON_PARSE_RESULT_SKIP;
	}

	// 不是前缀值，交由标量解析器处理
	return XSON_PARSE_RESULT_NONE;
}

// JSON 标量值解析算法：利用底层 JSON 解析器解析标量值（null/bool/int/hex/float/string），转换为对应 xvalue
static xson_parse_result_t _xson_parse_json_scalar(xson_parse_t* pParse, xvalue* ppVal)
{
	json_strinfo_t tKeyInfo = { 0 };
	json_number_t tNumber = { 0 };
	json_strinfo_t tStrInfo = { 0 };
	char* sText = NULL;
	char* sPtr;
	xvalue pVal = NULL;
	int iType;

	if ( pParse == NULL || ppVal == NULL ) {
		return XSON_PARSE_RESULT_FAIL;
	}

	// 调用底层 JSON 解析器解析单个值
	sPtr = pParse->tJSON.str + pParse->tJSON.offset;
	if ( _json_parse_single_value(&pParse->tJSON, sPtr, &tKeyInfo, &tNumber, &sText, &tStrInfo) < 0 ) {
		return XSON_PARSE_RESULT_FAIL;
	}

	// 根据解析出的类型创建对应的 xvalue 对象
	iType = tKeyInfo.type;
	if ( iType == JSON_NULL ) {
		pVal = xvoCreateNull();
	} else if ( iType == JSON_BOOL ) {
		pVal = xvoCreateBool(tNumber.vbool);
	} else if ( iType == JSON_INT ) {
		pVal = xvoCreateInt(tNumber.vint);
	} else if ( iType == JSON_HEX ) {
		pVal = xvoCreateInt(tNumber.vhex);
	} else if ( iType == JSON_LINT ) {
		pVal = xvoCreateInt(tNumber.vlint);
	} else if ( iType == JSON_LHEX ) {
		pVal = xvoCreateInt((int64)tNumber.vlhex);
	} else if ( iType == JSON_DOUBLE ) {
		pVal = xvoCreateFloat(tNumber.vdbl);
	} else if ( iType == JSON_STRING ) {
		// 字符串类型：区分动态分配和内联两种情况
		if ( tStrInfo.alloced ) {
			pVal = xvoCreateText(sText, tStrInfo.len, TRUE);
			sText = NULL;
		} else {
			pVal = xvoCreateText(sText, tStrInfo.len, FALSE);
		}
	}

	// 释放动态分配的临时字符串
	if ( tStrInfo.alloced && sText ) {
		json_free(sText);
	}

	// 检查是否成功创建了值对象
	if ( pVal == NULL ) {
		return XSON_PARSE_RESULT_FAIL;
	}

	*ppVal = pVal;
	return XSON_PARSE_RESULT_OK;
}

static xson_parse_result_t _xson_parse_value(xson_parse_t* pParse, xvalue* ppVal)
{
	xson_parse_result_t iRet;
	char ch;

	if ( pParse == NULL || ppVal == NULL ) {
		return XSON_PARSE_RESULT_FAIL;
	}

	_xson_skip_blank(pParse);
	ch = _xson_peek(pParse);
	if ( ch == '[' ) {
		return _xson_parse_array_like(pParse, XSON_CONTAINER_AUTO, ppVal);
	}
	if ( ch == '{' ) {
		return _xson_parse_object_like(pParse, XSON_CONTAINER_AUTO, ppVal);
	}
	if ( _xson_is_ident_start(ch) ) {
		iRet = _xson_parse_prefixed_value(pParse, ppVal);
		if ( iRet != XSON_PARSE_RESULT_NONE ) {
			return iRet;
		}
	}
	return _xson_parse_json_scalar(pParse, ppVal);
}

// [R12] SON 文本解析入口：初始化解析器状态，解析整个 SON 文本为 xvalue 树
static xvalue _xson_parse_text(str sText, size_t iSize, uint32 iFlags)
{
	xson_parse_t tParse = { 0 };
	xson_parse_result_t iRet;
	xvalue pRoot = NULL;

	if ( sText == NULL ) {
		return xvoCreateNull();
	}

	// 初始化解析器结构体
	tParse.tJSON.str = (char*)sText;
	tParse.tJSON.size = iSize ? iSize : strlen(__xrt_cstr(sText));
	tParse.tJSON.offset = 0;
	tParse.tJSON.skip_blank = _skip_blank_rapid;
	tParse.tJSON.parse_string = _json_sax_parse_string;
	tParse.iFlags = iFlags;

	// 跳过前导空白，解析根值
	_xson_skip_blank(&tParse);
	iRet = _xson_parse_value(&tParse, &pRoot);
	if ( iRet != XSON_PARSE_RESULT_OK ) {
		if ( pRoot ) {
			xvoUnref(pRoot);
		}
		return xvoCreateNull();
	}

	// 验证解析完成后不应有剩余非空白字符
	_xson_skip_blank(&tParse);
	if ( _xson_peek(&tParse) != '\0' ) {
		xvoUnref(pRoot);
		return xvoCreateNull();
	}

	return pRoot;
}

// [R12] 确保输出缓冲区有足够空间，不足时按倍增策略扩展
static bool _xson_print_reserve(xson_print_t* pPrint, size_t iNeed)
{
	size_t iNewSize;
	char* sNew;

	if ( pPrint == NULL ) {
		return FALSE;
	}

	// 当前空间已足够，直接返回
	if ( (pPrint->iUsed + iNeed + 1) <= pPrint->iSize ) {
		return TRUE;
	}

	// 计算新的缓冲区大小：256 起步，4096 以下倍增，以上每次加 4096
	iNewSize = pPrint->iSize ? pPrint->iSize : 256;
	while ( (pPrint->iUsed + iNeed + 1) > iNewSize ) {
		if ( iNewSize < 4096 ) {
			iNewSize <<= 1;
		} else {
			iNewSize += 4096;
		}
	}

	// 重新分配缓冲区
	sNew = xrtRealloc(pPrint->sText, iNewSize);
	if ( sNew == NULL ) {
		return FALSE;
	}

	pPrint->sText = sNew;
	pPrint->iSize = iNewSize;
	return TRUE;
}

static bool _xson_print_append_raw(xson_print_t* pPrint, const char* sText, size_t iSize)
{
	if ( iSize == 0 ) {
		return TRUE;
	}
	if ( _xson_print_reserve(pPrint, iSize) == FALSE ) {
		return FALSE;
	}
	memcpy(pPrint->sText + pPrint->iUsed, sText, iSize);
	pPrint->iUsed += iSize;
	pPrint->sText[pPrint->iUsed] = '\0';
	return TRUE;
}

static bool _xson_print_append_cstr(xson_print_t* pPrint, const char* sText)
{
	if ( sText == NULL ) {
		return FALSE;
	}
	return _xson_print_append_raw(pPrint, sText, strlen(__xrt_cstr(sText)));
}

static bool _xson_print_append_char(xson_print_t* pPrint, char ch)
{
	if ( _xson_print_reserve(pPrint, 1) == FALSE ) {
		return FALSE;
	}
	pPrint->sText[pPrint->iUsed++] = ch;
	pPrint->sText[pPrint->iUsed] = '\0';
	return TRUE;
}

static bool _xson_print_append_indent(xson_print_t* pPrint, int iDepth)
{
	if ( pPrint->bFormat == FALSE ) {
		return TRUE;
	}
	if ( _xson_print_append_char(pPrint, '\n') == FALSE ) {
		return FALSE;
	}
	for ( int i = 0; i < iDepth; ++i ) {
		if ( _xson_print_append_char(pPrint, '\t') == FALSE ) {
			return FALSE;
		}
	}
	return TRUE;
}

static bool _xson_print_append_i64(xson_print_t* pPrint, int64 iValue)
{
	char sBuff[32];
	int iSize = xrtI64ToStr(iValue, sBuff);

	if ( iSize < 0 ) {
		return FALSE;
	}
	return _xson_print_append_raw(pPrint, sBuff, (size_t)iSize);
}

static bool _xson_print_append_double(xson_print_t* pPrint, double fValue)
{
	char sBuff[64];
	int iSize = xrtNumToStr(fValue, sBuff);

	if ( iSize < 0 ) {
		return FALSE;
	}
	return _xson_print_append_raw(pPrint, sBuff, (size_t)iSize);
}

// JSON 字符串转义输出算法：按 JSON 规范对字符串进行转义处理并输出到缓冲区
static bool _xson_print_append_json_string(xson_print_t* pPrint, const char* sText, size_t iSize)
{
	static const char sHex[] = "0123456789abcdef";
	size_t i;

	// 输出开始双引号
	if ( _xson_print_append_char(pPrint, '\"') == FALSE ) {
		return FALSE;
	}

	// 逐字符扫描，对特殊字符进行转义处理
	for ( i = 0; i < iSize; ++i ) {
		unsigned char ch = (unsigned char)sText[i];
		switch ( ch ) {
		// JSON 标准转义字符
		case '\"':
			if ( _xson_print_append_raw(pPrint, "\\\"", 2) == FALSE ) { return FALSE; }
			break;
		case '\\':
			if ( _xson_print_append_raw(pPrint, "\\\\", 2) == FALSE ) { return FALSE; }
			break;
		case '\b':
			if ( _xson_print_append_raw(pPrint, "\\b", 2) == FALSE ) { return FALSE; }
			break;
		case '\f':
			if ( _xson_print_append_raw(pPrint, "\\f", 2) == FALSE ) { return FALSE; }
			break;
		case '\n':
			if ( _xson_print_append_raw(pPrint, "\\n", 2) == FALSE ) { return FALSE; }
			break;
		case '\r':
			if ( _xson_print_append_raw(pPrint, "\\r", 2) == FALSE ) { return FALSE; }
			break;
		case '\t':
			if ( _xson_print_append_raw(pPrint, "\\t", 2) == FALSE ) { return FALSE; }
			break;
		case '\v':
			if ( _xson_print_append_raw(pPrint, "\\v", 2) == FALSE ) { return FALSE; }
			break;
		default:
			// 控制字符（0x00-0x1F）使用 \u00XX 格式转义
			if ( ch < 0x20 ) {
				char sEsc[6];
				sEsc[0] = '\\';
				sEsc[1] = 'u';
				sEsc[2] = '0';
				sEsc[3] = '0';
				sEsc[4] = sHex[(ch >> 4) & 0x0F];
				sEsc[5] = sHex[ch & 0x0F];
				if ( _xson_print_append_raw(pPrint, sEsc, 6) == FALSE ) { return FALSE; }
			} else {
				// 普通可打印字符直接输出
				if ( _xson_print_append_char(pPrint, (char)ch) == FALSE ) { return FALSE; }
			}
			break;
		}
	}

	// 输出结束双引号
	return _xson_print_append_char(pPrint, '\"');
}

static xson_write_result_t _xson_write_value(xson_print_t* pPrint, xvalue varVal, int iDepth, bool bRoot);

typedef struct
{
	xson_print_t* pPrint;
	int iDepth;
	int iCount;
	bool bError;
} xson_walk_ctx_t;

// [R12] 将数组类型的 xvalue 序列化为 SON 文本
static xson_write_result_t _xson_write_array(xson_print_t* pPrint, xvalue varVal, int iDepth)
{
	int iCount = 0;

	// 输出开始括号
	if ( _xson_print_append_char(pPrint, '[') == FALSE ) {
		return XSON_WRITE_RESULT_FAIL;
	}

	// 遍历数组中每个元素并序列化
	for ( int i = 0; i < (int)varVal->vArray->Count; ++i ) {
		xvalue pItem = xvoArrayGetValue(varVal, (uint32)i);
		size_t iMark = pPrint->iUsed;
		xson_write_result_t iRet;

		// 元素间添加逗号分隔符
		if ( iCount > 0 ) {
			if ( _xson_print_append_char(pPrint, ',') == FALSE ) {
				return XSON_WRITE_RESULT_FAIL;
			}
		}
		// 格式化模式下输出缩进
		if ( pPrint->bFormat ) {
			if ( _xson_print_append_indent(pPrint, iDepth + 1) == FALSE ) {
				return XSON_WRITE_RESULT_FAIL;
			}
		}

		// 递归序列化元素值，跳过不支持的类型
		iRet = _xson_write_value(pPrint, pItem, iDepth + 1, FALSE);
		if ( iRet == XSON_WRITE_RESULT_SKIP ) {
			// 跳过时回退到元素开始前的位置
			pPrint->iUsed = iMark;
			pPrint->sText[pPrint->iUsed] = '\0';
			continue;
		}
		if ( iRet == XSON_WRITE_RESULT_FAIL ) {
			return XSON_WRITE_RESULT_FAIL;
		}

		iCount++;
	}

	// 格式化模式下在结束括号前输出缩进
	if ( pPrint->bFormat && iCount > 0 ) {
		if ( _xson_print_append_indent(pPrint, iDepth) == FALSE ) {
			return XSON_WRITE_RESULT_FAIL;
		}
	}

	// 输出结束括号
	if ( _xson_print_append_char(pPrint, ']') == FALSE ) {
		return XSON_WRITE_RESULT_FAIL;
	}
	return XSON_WRITE_RESULT_OK;
}

// [R12] 列表遍历回调：序列化每个列表元素的索引和值
static bool _xson_write_list_proc(int64 iKey, xvalue* ppVal, xson_walk_ctx_t* pCtx)
{
	size_t iMark;
	xson_write_result_t iRet;

	if ( pCtx == NULL || pCtx->pPrint == NULL || ppVal == NULL ) {
		return TRUE;
	}

	// 记录当前位置，以便跳过时回退
	iMark = pCtx->pPrint->iUsed;

	// 元素间添加逗号分隔符
	if ( pCtx->iCount > 0 ) {
		if ( _xson_print_append_char(pCtx->pPrint, ',') == FALSE ) {
			pCtx->bError = TRUE;
			return TRUE;
		}
	}
	// 格式化模式下输出缩进
	if ( pCtx->pPrint->bFormat ) {
		if ( _xson_print_append_indent(pCtx->pPrint, pCtx->iDepth + 1) == FALSE ) {
			pCtx->bError = TRUE;
			return TRUE;
		}
	}

	// 输出列表索引
	if ( _xson_print_append_i64(pCtx->pPrint, iKey) == FALSE ) {
		pCtx->bError = TRUE;
		return TRUE;
	}

	// 输出索引与值之间的冒号分隔符
	if ( pCtx->pPrint->bFormat ) {
		if ( _xson_print_append_raw(pCtx->pPrint, ":\t", 2) == FALSE ) {
			pCtx->bError = TRUE;
			return TRUE;
		}
	} else {
		if ( _xson_print_append_char(pCtx->pPrint, ':') == FALSE ) {
			pCtx->bError = TRUE;
			return TRUE;
		}
	}

	// 递归序列化元素值，跳过时回退到元素开始前的位置
	iRet = _xson_write_value(pCtx->pPrint, ppVal[0], pCtx->iDepth + 1, FALSE);
	if ( iRet == XSON_WRITE_RESULT_SKIP ) {
		pCtx->pPrint->iUsed = iMark;
		pCtx->pPrint->sText[pCtx->pPrint->iUsed] = '\0';
		return FALSE;
	}
	if ( iRet == XSON_WRITE_RESULT_FAIL ) {
		pCtx->bError = TRUE;
		return TRUE;
	}

	pCtx->iCount++;
	return FALSE;
}

static xson_write_result_t _xson_write_list(xson_print_t* pPrint, xvalue varVal, int iDepth)
{
	xson_walk_ctx_t tCtx = { pPrint, iDepth, 0, FALSE };

	if ( _xson_print_append_raw(pPrint, "list[", 5) == FALSE ) {
		return XSON_WRITE_RESULT_FAIL;
	}
	xrtListWalk(varVal->vList, (ptr)_xson_write_list_proc, &tCtx);
	if ( tCtx.bError ) {
		return XSON_WRITE_RESULT_FAIL;
	}
	if ( pPrint->bFormat && tCtx.iCount > 0 ) {
		if ( _xson_print_append_indent(pPrint, iDepth) == FALSE ) {
			return XSON_WRITE_RESULT_FAIL;
		}
	}
	if ( _xson_print_append_char(pPrint, ']') == FALSE ) {
		return XSON_WRITE_RESULT_FAIL;
	}
	return XSON_WRITE_RESULT_OK;
}

// [R12] 字典遍历回调：序列化每个字典键值对的键名和值
static bool _xson_write_dict_proc(Dict_Key* pKey, xvalue* ppVal, xson_walk_ctx_t* pCtx)
{
	size_t iMark;
	xson_write_result_t iRet;

	if ( pCtx == NULL || pCtx->pPrint == NULL || pKey == NULL || ppVal == NULL ) {
		return TRUE;
	}

	// 记录当前位置，以便跳过时回退
	iMark = pCtx->pPrint->iUsed;

	// 元素间添加逗号分隔符
	if ( pCtx->iCount > 0 ) {
		if ( _xson_print_append_char(pCtx->pPrint, ',') == FALSE ) {
			pCtx->bError = TRUE;
			return TRUE;
		}
	}
	// 格式化模式下输出缩进
	if ( pCtx->pPrint->bFormat ) {
		if ( _xson_print_append_indent(pCtx->pPrint, pCtx->iDepth + 1) == FALSE ) {
			pCtx->bError = TRUE;
			return TRUE;
		}
	}

	// 输出键名（JSON 字符串格式，带转义）
	if ( _xson_print_append_json_string(pCtx->pPrint, pKey->Key, pKey->KeyLen) == FALSE ) {
		pCtx->bError = TRUE;
		return TRUE;
	}

	// 输出键与值之间的冒号分隔符
	if ( pCtx->pPrint->bFormat ) {
		if ( _xson_print_append_raw(pCtx->pPrint, ":\t", 2) == FALSE ) {
			pCtx->bError = TRUE;
			return TRUE;
		}
	} else {
		if ( _xson_print_append_char(pCtx->pPrint, ':') == FALSE ) {
			pCtx->bError = TRUE;
			return TRUE;
		}
	}

	// 递归序列化值，跳过时回退到元素开始前的位置
	iRet = _xson_write_value(pCtx->pPrint, ppVal[0], pCtx->iDepth + 1, FALSE);
	if ( iRet == XSON_WRITE_RESULT_SKIP ) {
		pCtx->pPrint->iUsed = iMark;
		pCtx->pPrint->sText[pCtx->pPrint->iUsed] = '\0';
		return FALSE;
	}
	if ( iRet == XSON_WRITE_RESULT_FAIL ) {
		pCtx->bError = TRUE;
		return TRUE;
	}

	pCtx->iCount++;
	return FALSE;
}

static xson_write_result_t _xson_write_dict(xson_print_t* pPrint, xvalue varVal, int iDepth)
{
	xson_walk_ctx_t tCtx = { pPrint, iDepth, 0, FALSE };

	if ( _xson_print_append_char(pPrint, '{') == FALSE ) {
		return XSON_WRITE_RESULT_FAIL;
	}
	xrtDictWalk(varVal->vTable, (ptr)_xson_write_dict_proc, &tCtx);
	if ( tCtx.bError ) {
		return XSON_WRITE_RESULT_FAIL;
	}
	if ( pPrint->bFormat && tCtx.iCount > 0 ) {
		if ( _xson_print_append_indent(pPrint, iDepth) == FALSE ) {
			return XSON_WRITE_RESULT_FAIL;
		}
	}
	if ( _xson_print_append_char(pPrint, '}') == FALSE ) {
		return XSON_WRITE_RESULT_FAIL;
	}
	return XSON_WRITE_RESULT_OK;
}

// [R12] 集合遍历回调：序列化集合中的每个元素值
static bool _xson_write_set_proc(Coll_Key* pKey, xson_walk_ctx_t* pCtx)
{
	size_t iMark;
	xson_write_result_t iRet;

	if ( pCtx == NULL || pCtx->pPrint == NULL || pKey == NULL ) {
		return TRUE;
	}

	// 记录当前位置，以便跳过时回退
	iMark = pCtx->pPrint->iUsed;

	// 元素间添加逗号分隔符
	if ( pCtx->iCount > 0 ) {
		if ( _xson_print_append_char(pCtx->pPrint, ',') == FALSE ) {
			pCtx->bError = TRUE;
			return TRUE;
		}
	}
	// 格式化模式下输出缩进
	if ( pCtx->pPrint->bFormat ) {
		if ( _xson_print_append_indent(pCtx->pPrint, pCtx->iDepth + 1) == FALSE ) {
			pCtx->bError = TRUE;
			return TRUE;
		}
	}

	// 递归序列化元素值，跳过时回退到元素开始前的位置
	iRet = _xson_write_value(pCtx->pPrint, pKey->Value, pCtx->iDepth + 1, FALSE);
	if ( iRet == XSON_WRITE_RESULT_SKIP ) {
		pCtx->pPrint->iUsed = iMark;
		pCtx->pPrint->sText[pCtx->pPrint->iUsed] = '\0';
		return FALSE;
	}
	if ( iRet == XSON_WRITE_RESULT_FAIL ) {
		pCtx->bError = TRUE;
		return TRUE;
	}

	pCtx->iCount++;
	return FALSE;
}

static xson_write_result_t _xson_write_set(xson_print_t* pPrint, xvalue varVal, int iDepth)
{
	xson_walk_ctx_t tCtx = { pPrint, iDepth, 0, FALSE };

	if ( _xson_print_append_raw(pPrint, "set{", 4) == FALSE ) {
		return XSON_WRITE_RESULT_FAIL;
	}
	xrtAVLTreeWalk(varVal->vColl, (ptr)_xson_write_set_proc, &tCtx);
	if ( tCtx.bError ) {
		return XSON_WRITE_RESULT_FAIL;
	}
	if ( pPrint->bFormat && tCtx.iCount > 0 ) {
		if ( _xson_print_append_indent(pPrint, iDepth) == FALSE ) {
			return XSON_WRITE_RESULT_FAIL;
		}
	}
	if ( _xson_print_append_char(pPrint, '}') == FALSE ) {
		return XSON_WRITE_RESULT_FAIL;
	}
	return XSON_WRITE_RESULT_OK;
}

static bool _xson_is_unsupported_value(xvalue varVal)
{
	if ( varVal == NULL ) {
		return FALSE;
	}
	return (varVal->Type == XVO_DT_POINT) || (varVal->Type == XVO_DT_FUNC) || (varVal->Type == XVO_DT_CUSTOM);
}

// SON 值序列化算法：根据 xvalue 的类型分发到对应的序列化处理器
static xson_write_result_t _xson_write_value(xson_print_t* pPrint, xvalue varVal, int iDepth, bool bRoot)
{
	str sText;
	str sBase64;

	if ( pPrint == NULL || varVal == NULL ) {
		return XSON_WRITE_RESULT_FAIL;
	}

	// 检查是否为不支持的类型（指针、函数、自定义），根据标志决定跳过或失败
	if ( _xson_is_unsupported_value(varVal) ) {
		if ( ((pPrint->iFlags & XSON_F_IGNORE_UNSUPPORTED_ENCODE) != 0) && (bRoot == FALSE) ) {
			return XSON_WRITE_RESULT_SKIP;
		}
		return XSON_WRITE_RESULT_FAIL;
	}

	// 根据值类型分发序列化处理
	switch ( varVal->Type ) {
	case XVO_DT_NULL:
		return _xson_print_append_raw(pPrint, "null", 4) ? XSON_WRITE_RESULT_OK : XSON_WRITE_RESULT_FAIL;
	case XVO_DT_BOOL:
		if ( varVal->vBool ) {
			return _xson_print_append_raw(pPrint, "true", 4) ? XSON_WRITE_RESULT_OK : XSON_WRITE_RESULT_FAIL;
		}
		return _xson_print_append_raw(pPrint, "false", 5) ? XSON_WRITE_RESULT_OK : XSON_WRITE_RESULT_FAIL;
	case XVO_DT_INT:
		return _xson_print_append_i64(pPrint, varVal->vInt) ? XSON_WRITE_RESULT_OK : XSON_WRITE_RESULT_FAIL;
	case XVO_DT_FLOAT:
		return _xson_print_append_double(pPrint, varVal->vFloat) ? XSON_WRITE_RESULT_OK : XSON_WRITE_RESULT_FAIL;
	case XVO_DT_TEXT:
		// 文本类型：输出为 JSON 字符串（带转义）
		sText = varVal->vText ? varVal->vText : xCore.sNull;
		return _xson_print_append_json_string(pPrint, __xrt_cstr(sText), varVal->Size) ? XSON_WRITE_RESULT_OK : XSON_WRITE_RESULT_FAIL;
	case XVO_DT_TIME:
		// 时间类型：输出为 time(时间文本) 格式
		if ( _xson_print_append_raw(pPrint, "time(", 5) == FALSE ) {
			return XSON_WRITE_RESULT_FAIL;
		}
		sText = xvoGetText(varVal);
		if ( _xson_print_append_cstr(pPrint, __xrt_cstr(sText)) == FALSE ) {
			return XSON_WRITE_RESULT_FAIL;
		}
		return _xson_print_append_char(pPrint, ')') ? XSON_WRITE_RESULT_OK : XSON_WRITE_RESULT_FAIL;
	case XVO_DT_ARRAY:
		// 数组类型：递归序列化
		return _xson_write_array(pPrint, varVal, iDepth);
	case XVO_DT_LIST:
		// 列表类型：递归序列化
		return _xson_write_list(pPrint, varVal, iDepth);
	case XVO_DT_COLL:
		// 集合类型：递归序列化
		return _xson_write_set(pPrint, varVal, iDepth);
	case XVO_DT_TABLE:
		// 字典类型：递归序列化
		return _xson_write_dict(pPrint, varVal, iDepth);
	case XVO_DT_CLASS:
		// 类类型：将结构体数据 Base64 编码后输出为 class(Base64) 格式
		if ( varVal->vStruct == NULL || varVal->Size == 0 ) {
			return XSON_WRITE_RESULT_FAIL;
		}
		sBase64 = xrtBase64Encode(varVal->vStruct, varVal->Size, NULL);
		if ( sBase64 == NULL || sBase64 == xCore.sNull ) {
			return XSON_WRITE_RESULT_FAIL;
		}
		if ( _xson_print_append_raw(pPrint, "class(", 6) == FALSE ) {
			xrtFree(sBase64);
			return XSON_WRITE_RESULT_FAIL;
		}
		if ( _xson_print_append_cstr(pPrint, __xrt_cstr(sBase64)) == FALSE ) {
			xrtFree(sBase64);
			return XSON_WRITE_RESULT_FAIL;
		}
		xrtFree(sBase64);
		return _xson_print_append_char(pPrint, ')') ? XSON_WRITE_RESULT_OK : XSON_WRITE_RESULT_FAIL;
	default:
		return XSON_WRITE_RESULT_FAIL;
	}
}

XXAPI xvalue xrtParseXSON(str sText, size_t iSize)
{
	return xrtParseXSONEx(sText, iSize, 0);
}

XXAPI xvalue xrtParseXSONEx(str sText, size_t iSize, uint32 iFlags)
{
	return _xson_parse_text(sText, iSize, iFlags);
}

XXAPI xvalue xrtParseXSON_File(str sFile)
{
	return xrtParseXSON_FileEx(sFile, 0);
}

XXAPI xvalue xrtParseXSON_FileEx(str sFile, uint32 iFlags)
{
	size_t iSize = 0;
	str sText = xrtFileGetAll(sFile, &iSize);
	xvalue pRet;

	if ( sText == NULL ) {
		return xvoCreateNull();
	}

	pRet = _xson_parse_text(sText, iSize, iFlags);
	xrtFree(sText);
	return pRet;
}

XXAPI str xrtStringifyXSON(xvalue varVal, int bFormat, uint32 iFlags, size_t* pRetSize)
{
	xson_print_t tPrint = { 0 };
	xson_write_result_t iRet;

	if ( varVal == NULL ) {
		return NULL;
	}

	// 初始化输出缓冲区
	tPrint.sText = NULL;
	tPrint.iSize = 0;
	tPrint.iUsed = 0;
	tPrint.bFormat = bFormat;
	tPrint.iFlags = iFlags;

	// 预分配初始空间
	if ( _xson_print_reserve(&tPrint, 64) == FALSE ) {
		return NULL;
	}
	tPrint.sText[0] = '\0';

	// 递归序列化根值
	iRet = _xson_write_value(&tPrint, varVal, 0, TRUE);
	if ( iRet != XSON_WRITE_RESULT_OK ) {
		xrtFree(tPrint.sText);
		return NULL;
	}

	// 返回结果字符串及其长度
	if ( pRetSize ) {
		*pRetSize = tPrint.iUsed;
	}
	return (str)tPrint.sText;
}

XXAPI int xrtStringifyXSON_File(str sFile, xvalue varVal, int bFormat, uint32 iFlags)
{
	size_t iSize = 0;
	str sText = xrtStringifyXSON(varVal, bFormat, iFlags, &iSize);
	int iRet;

	if ( sText == NULL ) {
		return FALSE;
	}

	iRet = xrtFilePutAll(sFile, sText, iSize);
	xrtFree(sText);
	return iRet;
}
