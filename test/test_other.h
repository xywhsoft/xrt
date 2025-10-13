


// 解析 JSON 文件到 xte Value
json_sax_ret_t xte_private_ParseJSON_Proc(json_sax_parser_t *parser)
{
	json_string_t *jkey = &parser->array[parser->index];
	/*
	// 新结构数据入栈
	if ( ((jkey->type == JSON_ARRAY) || (jkey->type == JSON_OBJECT)) && (parser->value.vcmd == JSON_SAX_START) ) {
		if ( jkey->type == JSON_ARRAY ) {
			if ( varRoot ) {
				if ( varCur->MainType == XTE_DT_ARRAY ) {
					XTE_Value arrNew = xteValueCreateArray();
					xteArrayAppendValue(varCur, arrNew, TRUE);
					PSSTK_Push(varStack, arrNew);
					varCur = arrNew;
				} else if ( varCur->MainType == XTE_DT_TABLE ) {
					XTE_Value arrNew = xteValueCreateArray();
					char* sKey = malloc(jkey->len + 1);
					memcpy(sKey, jkey->str, jkey->len);
					sKey[jkey->len] = 0;
					xteTableSetValue(varCur, sKey, jkey->len, arrNew, TRUE);
					PSSTK_Push(varStack, arrNew);
					varCur = arrNew;
				}
			} else {
				varRoot = xteValueCreateArray();
				PSSTK_Push(varStack, varRoot);
				varCur = varRoot;
			}
		} else if ( jkey->type == JSON_OBJECT ) {
			if ( varRoot ) {
				if ( varCur->MainType == XTE_DT_ARRAY ) {
					XTE_Value tblNew = xteValueCreateTable();
					xteArrayAppendValue(varCur, tblNew, TRUE);
					PSSTK_Push(varStack, tblNew);
					varCur = tblNew;
				} else if ( varCur->MainType == XTE_DT_TABLE ) {
					XTE_Value tblNew = xteValueCreateTable();
					char* sKey = malloc(jkey->len + 1);
					memcpy(sKey, jkey->str, jkey->len);
					sKey[jkey->len] = 0;
					xteTableSetValue(varCur, sKey, jkey->len, tblNew, TRUE);
					PSSTK_Push(varStack, tblNew);
					varCur = tblNew;
				}
			} else {
				varRoot = xteValueCreateTable();
				PSSTK_Push(varStack, varRoot);
				varCur = varRoot;
			}
		}
    }
    if ( ((jkey->type == JSON_ARRAY) || (jkey->type == JSON_OBJECT)) && (parser->value.vcmd == JSON_SAX_FINISH) ) {
		if ( varStack > 0) {
			PSSTK_Pop(varStack);
			varCur = PSSTK_Top(varStack);
		}
	}
	*/
	if ( jkey->info.type == JSON_NULL ) {
        printf("null\n");
	} else if ( jkey->info.type == JSON_BOOL ) {
        printf("bool   : %d\n", parser->value.vnum.vbool);
	} else if ( jkey->info.type == JSON_INT ) {
        printf("int    : %d\n", parser->value.vnum.vint);
	} else if ( jkey->info.type == JSON_HEX ) {
        printf("hex    : %d\n", parser->value.vnum.vhex);
	} else if ( jkey->info.type == JSON_LINT ) {
        printf("lint   : %lld\n", parser->value.vnum.vlint);
	} else if ( jkey->info.type == JSON_LHEX ) {
        printf("lhex   : %lld\n", parser->value.vnum.vlhex);
	} else if ( jkey->info.type == JSON_DOUBLE ) {
        printf("double : %f\n", parser->value.vnum.vdbl);
	} else if ( jkey->info.type == JSON_STRING ) {
        printf("str    : %.*s\n", parser->value.vstr.info.len, parser->value.vstr.str);
	} else if ( jkey->info.type == JSON_ARRAY ) {
        printf("array\n");
	} else if ( jkey->info.type == JSON_OBJECT ) {
        printf("table\n");
	} else {
		printf("Unknown data type\n");
	}
    return JSON_SAX_PARSE_CONTINUE;
}



// 自定义测试
void Test_Other(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n 自定义测试 :\n\n");
	/*
	printf("xvalue_struct : %d\n", sizeof(xvalue_struct));
	printf("xvalue12_struct : %d\n", sizeof(xvalue12_struct));
	printf("xcustom_struct : %d\n", sizeof(xcustom_struct));
	*/
	
	
	
	str sFile = xrtPathJoin(3, xCore->AppPath, "test", "json.txt");
	char* sText = xrtFileGetAll(sFile);
	int iRet = json_sax_parse_str(sText, xCore->iRet, xte_private_ParseJSON_Proc);
	if ( iRet < 0 ) {
		printf("ok\n");
	}
	
	
	
}


