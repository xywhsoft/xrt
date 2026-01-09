


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
	
	printf("\n模板引擎 Phase 1 测试完成\n");
}


