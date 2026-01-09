


// Template 库测试
void Test_Template(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Template 库测试 :\n\n");
	
	// ==================== 测试 1: 词法分析器空格支持 ====================
	printf("--- 测试词法分析器空格支持 ---\n");
	
	// 测试模板：包含空格的标签（使用 .2 格式表示 2 位小数）
	const char* tpl1 = "Hello {$ name }! Price: {%  price  :  .2  }";
	XTE_LiteObject obj1 = xteParse((char*)tpl1, 0, NULL);
	if ( obj1->Success ) {
		printf("✓ 模板解析成功\n");
		
		// 创建测试数据
		xvalue data = xvoCreateTable();
		xvoTableSetText(data, "name", 0, "World", 0, FALSE);
		xvoTableSetFloat(data, "price", 0, 1234.567);
		
		size_t retSize = 0;
		char* result = xteMake(obj1, data, NULL, NULL, &retSize);
		if ( result ) {
			printf("输出: %s\n", result);
			printf("期望: Hello World! Price: 1234.57\n");
			xrtFree(result);
		}
		
		xvoUnref(data);
	} else {
		printf("✗ 模板解析失败: %s\n", obj1->ErrorDesc);
	}
	xteParseFree(obj1);
	
	// 测试时间格式化空格（注意：冒号需要转义）
	const char* tpl2 = "Time: {&  created  :  yyyy-mm-dd hh\\:nn\\:ss  }";
	XTE_LiteObject obj2 = xteParse((char*)tpl2, 0, NULL);
	if ( obj2->Success ) {
		printf("✓ 时间模板解析成功\n");
		
		xvalue data = xvoCreateTable();
		xvoTableSetInt(data, "created", 0, xrtDateTimeSerial(2026, 1, 9, 14, 30, 45));
		
		size_t retSize = 0;
		char* result = xteMake(obj2, data, NULL, NULL, &retSize);
		if ( result ) {
			printf("输出: %s\n", result);
			printf("期望: Time: 2026-01-09 14:30:45\n");
			xrtFree(result);
		}
		
		xvoUnref(data);
	} else {
		printf("✗ 时间模板解析失败: %s\n", obj2->ErrorDesc);
	}
	xteParseFree(obj2);
	
	// ==================== 测试 2: 路径解析器 ====================
	printf("\n--- 测试路径解析器 ---\n");
	
	// 创建嵌套数据结构
	// {
	//   user: {
	//     name: "张三",
	//     profile: {
	//       age: 25,
	//       city: "北京"
	//     }
	//   },
	//   items: [ {title: "第一项"}, {title: "第二项"} ]
	// }
	xvalue data = xvoCreateTable();
	
	xvalue user = xvoCreateTable();
	xvoTableSetText(user, "name", 0, "张三", 0, FALSE);
	
	xvalue profile = xvoCreateTable();
	xvoTableSetInt(profile, "age", 0, 25);
	xvoTableSetText(profile, "city", 0, "北京", 0, FALSE);
	xvoTableSetValue(user, "profile", 0, profile, TRUE);
	
	xvoTableSetValue(data, "user", 0, user, TRUE);
	
	xvalue items = xvoCreateArray();
	xvalue item0 = xvoCreateTable();
	xvoTableSetText(item0, "title", 0, "第一项", 0, FALSE);
	xvoArrayAppendValue(items, item0, TRUE);
	xvalue item1 = xvoCreateTable();
	xvoTableSetText(item1, "title", 0, "第二项", 0, FALSE);
	xvoArrayAppendValue(items, item1, TRUE);
	xvoTableSetValue(data, "items", 0, items, TRUE);
	
	// 测试简单路径
	xvalue v1 = xteResolvePath("user", 0, data, NULL, NULL);
	printf("xteResolvePath(\"user\") = %s\n", (v1 && v1->Type != XVO_DT_NULL) ? "找到 (Table)" : "NULL");
	
	// 测试点号访问
	xvalue v2 = xteResolvePath("user.name", 0, data, NULL, NULL);
	str s2 = xvoGetText(v2);
	printf("xteResolvePath(\"user.name\") = %s\n", s2 ? (char*)s2 : "NULL");
	
	// 测试深层嵌套
	xvalue v3 = xteResolvePath("user.profile.age", 0, data, NULL, NULL);
	printf("xteResolvePath(\"user.profile.age\") = %lld\n", xvoGetInt(v3));
	
	xvalue v4 = xteResolvePath("user.profile.city", 0, data, NULL, NULL);
	str s4 = xvoGetText(v4);
	printf("xteResolvePath(\"user.profile.city\") = %s\n", s4 ? (char*)s4 : "NULL");
	
	// 测试数组索引
	xvalue v5 = xteResolvePath("items[0]", 0, data, NULL, NULL);
	printf("xteResolvePath(\"items[0]\") = %s\n", (v5 && v5->Type != XVO_DT_NULL) ? "找到 (Table)" : "NULL");
	
	// 测试数组索引 + 属性访问
	xvalue v6 = xteResolvePath("items[0].title", 0, data, NULL, NULL);
	str s6 = xvoGetText(v6);
	printf("xteResolvePath(\"items[0].title\") = %s\n", s6 ? (char*)s6 : "NULL");
	
	xvalue v7 = xteResolvePath("items[1].title", 0, data, NULL, NULL);
	str s7 = xvoGetText(v7);
	printf("xteResolvePath(\"items[1].title\") = %s\n", s7 ? (char*)s7 : "NULL");
	
	// 测试不存在的路径
	xvalue v8 = xteResolvePath("user.notexist", 0, data, NULL, NULL);
	printf("xteResolvePath(\"user.notexist\") = %s\n", (!v8 || v8->Type == XVO_DT_NULL) ? "NULL (正确)" : "错误");
	
	xvalue v9 = xteResolvePath("items[99]", 0, data, NULL, NULL);
	printf("xteResolvePath(\"items[99]\") = %s\n", (!v9 || v9->Type == XVO_DT_NULL) ? "NULL (正确)" : "错误");
	
	// 清理
	xvoUnref(data);
	
	// ==================== 测试 3: 综合测试（路径 + 空格） ====================
	printf("\n--- 综合测试 ---\n");
	
	// 注意：当前词法分析器尚未支持路径语法，这里仅测试空格 trim（使用 , 表示千分位）
	const char* tpl3 = "{$ name } - {%  count  :  ,  }";
	XTE_LiteObject obj3 = xteParse((char*)tpl3, 0, NULL);
	if ( obj3->Success ) {
		printf("✓ 综合模板解析成功\n");
		
		xvalue data3 = xvoCreateTable();
		xvoTableSetText(data3, "name", 0, "Test", 0, FALSE);
		xvoTableSetInt(data3, "count", 0, 1234567);
		
		size_t retSize = 0;
		char* result = xteMake(obj3, data3, NULL, NULL, &retSize);
		if ( result ) {
			printf("输出: %s\n", result);
			printf("期望: Test - 1,234,567\n");
			xrtFree(result);
		}
		
		xvoUnref(data3);
	} else {
		printf("✗ 综合模板解析失败: %s\n", obj3->ErrorDesc);
	}
	xteParseFree(obj3);
	
	// ==================== 测试 4: 表达式解析器 ====================
	printf("\n--- 测试表达式解析器 ---\n");
	
	// 创建测试数据
	xvalue exprData = xvoCreateTable();
	xvoTableSetInt(exprData, "age", 0, 25);
	xvoTableSetText(exprData, "name", 0, "Alice", 0, FALSE);
	xvoTableSetBool(exprData, "active", 0, TRUE);
	xvoTableSetFloat(exprData, "score", 0, 85.5);
	
	// 测试比较运算
	printf("测试 'age = 25': %s\n", xteExprEvalBool("age = 25", 0, exprData, NULL, NULL) ? "✓ true" : "✗ false");
	printf("测试 'age > 20': %s\n", xteExprEvalBool("age > 20", 0, exprData, NULL, NULL) ? "✓ true" : "✗ false");
	printf("测试 'age < 20': %s\n", xteExprEvalBool("age < 20", 0, exprData, NULL, NULL) ? "✓ false" : "✗ true");
	printf("测试 'score >= 85.5': %s\n", xteExprEvalBool("score >= 85.5", 0, exprData, NULL, NULL) ? "✓ true" : "✗ false");
	
	// 测试字符串比较
	printf("测试 'name = \"Alice\"': %s\n", xteExprEvalBool("name = \"Alice\"", 0, exprData, NULL, NULL) ? "✓ true" : "✗ false");
	printf("测试 'name != \"Bob\"': %s\n", xteExprEvalBool("name != \"Bob\"", 0, exprData, NULL, NULL) ? "✓ true" : "✗ false");
	
	// 测试布尔运算
	printf("测试 'active': %s\n", xteExprEvalBool("active", 0, exprData, NULL, NULL) ? "✓ true" : "✗ false");
	printf("测试 'not active': %s\n", xteExprEvalBool("not active", 0, exprData, NULL, NULL) ? "✗ true" : "✓ false");
	
	// 测试逻辑运算
	printf("测试 'age > 20 and active': %s\n", xteExprEvalBool("age > 20 and active", 0, exprData, NULL, NULL) ? "✓ true" : "✗ false");
	printf("测试 'age < 20 or active': %s\n", xteExprEvalBool("age < 20 or active", 0, exprData, NULL, NULL) ? "✓ true" : "✗ false");
	printf("测试 'age < 20 and active': %s\n", xteExprEvalBool("age < 20 and active", 0, exprData, NULL, NULL) ? "✗ true" : "✓ false");
	
	// 测试括号
	printf("测试 '(age > 20) and (score > 80)': %s\n", xteExprEvalBool("(age > 20) and (score > 80)", 0, exprData, NULL, NULL) ? "✓ true" : "✗ false");
	
	// 测试字面量
	printf("测试 'true': %s\n", xteExprEvalBool("true", 0, NULL, NULL, NULL) ? "✓ true" : "✗ false");
	printf("测试 'false': %s\n", xteExprEvalBool("false", 0, NULL, NULL, NULL) ? "✗ true" : "✓ false");
	printf("测试 '100 > 50': %s\n", xteExprEvalBool("100 > 50", 0, NULL, NULL, NULL) ? "✓ true" : "✗ false");
	
	xvoUnref(exprData);
	
	printf("\n模板引擎 Phase 2 测试完成\n");
	
	// ==================== 测试 5: 控制语句 (Phase 3) ====================
	printf("\n--- 测试控制语句 (Phase 3) ---\n");
	
	// 测试 if 条件语句
	printf("\n[测试 if 语句]\n");
	const char* tplIf = "{#if:age > 18}Adult{#else}Minor{#end}";
	XTE_LiteObject objIf = xteParse((char*)tplIf, 0, NULL);
	if ( objIf->Success ) {
		xvalue dataIf = xvoCreateTable();
		xvoTableSetInt(dataIf, "age", 0, 25);
		
		size_t retSize = 0;
		char* result = xteMake(objIf, dataIf, NULL, NULL, &retSize);
		if ( result ) {
			printf("模板: %s\n", tplIf);
			printf("输出: %s\n", result);
			printf("期望: Adult\n");
			printf("结果: %s\n", strcmp(result, "Adult") == 0 ? "✓ 通过" : "✗ 失败");
			xrtFree(result);
		}
		xvoUnref(dataIf);
	} else {
		printf("✗ if 模板解析失败: %s\n", objIf->ErrorDesc);
	}
	xteParseFree(objIf);
	
	// 测试 if/elseif/else
	printf("\n[测试 if/elseif/else]\n");
	const char* tplIfElse = "{#if:score >= 90}A{#elseif:score >= 60}B{#else}C{#end}";
	XTE_LiteObject objIfElse = xteParse((char*)tplIfElse, 0, NULL);
	if ( objIfElse->Success ) {
		xvalue dataIfElse = xvoCreateTable();
		xvoTableSetInt(dataIfElse, "score", 0, 75);
		
		size_t retSize = 0;
		char* result = xteMake(objIfElse, dataIfElse, NULL, NULL, &retSize);
		if ( result ) {
			printf("模板: %s\n", tplIfElse);
			printf("输出: %s\n", result);
			printf("期望: B\n");
			printf("结果: %s\n", strcmp(result, "B") == 0 ? "✓ 通过" : "✗ 失败");
			xrtFree(result);
		}
		xvoUnref(dataIfElse);
	} else {
		printf("✗ if/elseif/else 模板解析失败: %s\n", objIfElse->ErrorDesc);
	}
	xteParseFree(objIfElse);
	
	// 测试 for 计次循环
	printf("\n[测试 for 计次循环]\n");
	const char* tplFor = "{#for:1:3:1}{% __index__ }{#end}";
	XTE_LiteObject objFor = xteParse((char*)tplFor, 0, NULL);
	if ( objFor->Success ) {
		xvalue dataFor = xvoCreateTable();
		
		size_t retSize = 0;
		char* result = xteMake(objFor, dataFor, NULL, NULL, &retSize);
		if ( result ) {
			printf("模板: %s\n", tplFor);
			printf("输出: %s\n", result);
			printf("期望: 1 2 3 \n");
			xrtFree(result);
		}
		xvoUnref(dataFor);
	} else {
		printf("✗ for 模板解析失败: %s\n", objFor->ErrorDesc);
	}
	xteParseFree(objFor);
	
	// 测试 foreach 数组迭代
	printf("\n[测试 foreach 数组迭代]\n");
	const char* tplForeach = "{#foreach:items}{$ name },{#end}";
	XTE_LiteObject objForeach = xteParse((char*)tplForeach, 0, NULL);
	if ( objForeach->Success ) {
		// 创建数组数据
		xvalue dataForeach = xvoCreateTable();
		xvalue itemsArr = xvoCreateArray();
		
		xvalue item0 = xvoCreateTable();
		xvoTableSetText(item0, "name", 0, "Alice", 0, FALSE);
		xvoArrayAppendValue(itemsArr, item0, TRUE);
		
		xvalue item1 = xvoCreateTable();
		xvoTableSetText(item1, "name", 0, "Bob", 0, FALSE);
		xvoArrayAppendValue(itemsArr, item1, TRUE);
		
		xvalue item2 = xvoCreateTable();
		xvoTableSetText(item2, "name", 0, "Charlie", 0, FALSE);
		xvoArrayAppendValue(itemsArr, item2, TRUE);
		
		xvoTableSetValue(dataForeach, "items", 0, itemsArr, TRUE);
		
		size_t retSize = 0;
		char* result = xteMake(objForeach, dataForeach, NULL, NULL, &retSize);
		if ( result ) {
			printf("模板: %s\n", tplForeach);
			printf("输出: %s\n", result);
			printf("期望: Alice,Bob,Charlie,\n");
			printf("结果: %s\n", strcmp(result, "Alice,Bob,Charlie,") == 0 ? "✓ 通过" : "✗ 失败");
			xrtFree(result);
		}
		xvoUnref(dataForeach);
	} else {
		printf("✗ foreach 模板解析失败: %s\n", objForeach->ErrorDesc);
	}
	xteParseFree(objForeach);
	
	// 测试嵌套控制语句
	printf("\n[测试嵌套控制语句]\n");
	const char* tplNested = "{#foreach:users}{#if:active}{$ name } {#end}{#end}";
	XTE_LiteObject objNested = xteParse((char*)tplNested, 0, NULL);
	if ( objNested->Success ) {
		xvalue dataNested = xvoCreateTable();
		xvalue usersArr = xvoCreateArray();
		
		xvalue u0 = xvoCreateTable();
		xvoTableSetText(u0, "name", 0, "Alice", 0, FALSE);
		xvoTableSetBool(u0, "active", 0, TRUE);
		xvoArrayAppendValue(usersArr, u0, TRUE);
		
		xvalue u1 = xvoCreateTable();
		xvoTableSetText(u1, "name", 0, "Bob", 0, FALSE);
		xvoTableSetBool(u1, "active", 0, FALSE);
		xvoArrayAppendValue(usersArr, u1, TRUE);
		
		xvalue u2 = xvoCreateTable();
		xvoTableSetText(u2, "name", 0, "Charlie", 0, FALSE);
		xvoTableSetBool(u2, "active", 0, TRUE);
		xvoArrayAppendValue(usersArr, u2, TRUE);
		
		xvoTableSetValue(dataNested, "users", 0, usersArr, TRUE);
		
		size_t retSize = 0;
		char* result = xteMake(objNested, dataNested, NULL, NULL, &retSize);
		if ( result ) {
			printf("模板: %s\n", tplNested);
			printf("输出: [%s]\n", result);
			printf("期望: [Alice Charlie ]\n");
			xrtFree(result);
		}
		xvoUnref(dataNested);
	} else {
		printf("✗ 嵌套模板解析失败: %s\n", objNested->ErrorDesc);
	}
	xteParseFree(objNested);
	
	printf("\n模板引擎 Phase 3 测试完成\n");
}


