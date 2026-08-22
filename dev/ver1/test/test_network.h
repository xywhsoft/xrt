


// 网络库测试
void Test_Network(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n 网络库测试 :\n\n");
	
	// 基本信息获取
	printf("--- 基本网络信息 ---\n");
	str sIP = xrtGetLocalIP();
	printf("Local IP : %s\n", sIP);
	if ( sIP != NULL && strlen(sIP) > 0 ) {
		printf("✓ IP 获取成功\n");
	} else {
		printf("✗ IP 获取失败\n");
	}
	if ( sIP != xCore->sNull ) {
		xrtFree(sIP);
	}
	
	uint32 u32IP = xrtGetLocalRawIP();
	printf("Local IP [ int ] : %x (%d.%d.%d.%d)\n", u32IP,
		(u8)(u32IP >> 24), (u8)(u32IP >> 16), (u8)(u32IP >> 8), (u8)(u32IP));
	
	printf("Local IP [ int & g ] : %x (%d.%d.%d.%d)\n", xCore->LocalAddr,
		(u8)(xCore->LocalAddr >> 24), (u8)(xCore->LocalAddr >> 16), (u8)(xCore->LocalAddr >> 8), (u8)(xCore->LocalAddr));
	
	str sName = xrtGetLocalName();
	printf("Local Name : %s\n", sName);
	if ( sName != NULL && strlen(sName) > 0 ) {
		printf("✓ 主机名获取成功\n");
	} else {
		printf("✗ 主机名获取失败\n");
	}
	if ( sName != xCore->sNull ) {
		xrtFree(sName);
	}
	
	str sMAC = xrtGetLocalMAC();
	printf("Local MAC : %s\n", sMAC);
	if ( sMAC != NULL && strlen(sMAC) == 12 ) {
		printf("✓ MAC 地址获取成功（格式正确）\n");
	} else {
		printf("✗ MAC 地址获取失败或格式不正确\n");
	}
	if ( sMAC != xCore->sNull ) {
		xrtFree(sMAC);
	}
	
	// 验证 IP 格式
	printf("\n--- IP 地址格式验证 ---\n");
	str sIP2 = xrtGetLocalIP();
	if ( sIP2 != NULL ) {
		int iDotCount = 0;
		for ( size_t i = 0; sIP2[i] != 0; i++ ) {
			if ( sIP2[i] == '.' ) {
				iDotCount++;
			}
		}
		printf("IP 地址中点号数量: %d\n", iDotCount);
		if ( iDotCount == 3 ) {
			printf("✓ IP 地址格式正确（IPv4）\n");
		} else {
			printf("✗ IP 地址格式可能不正确\n");
		}
		xrtFree(sIP2);
	}
	
	// MAC 地址格式验证
	printf("\n--- MAC 地址格式验证 ---\n");
	str sMAC2 = xrtGetLocalMAC();
	if ( sMAC2 != NULL ) {
		printf("MAC 地址长度: %d 字符\n", (int)strlen(sMAC2));
		if ( strlen(sMAC2) == 12 ) {
			printf("✓ MAC 地址长度正确（6字节 = 12个十六进制字符）\n");
		}
		if ( sMAC2 != xCore->sNull ) {
			xrtFree(sMAC2);
		}
	}
	
	// 多次调用稳定性测试
	printf("\n--- 多次调用稳定性测试 ---\n");
	uint32 u32IP_A = xrtGetLocalRawIP();
	uint32 u32IP_B = xrtGetLocalRawIP();
	uint32 u32IP_C = xrtGetLocalRawIP();
	if ( u32IP_A == u32IP_B && u32IP_B == u32IP_C ) {
		printf("✓ IP 地址获取稳定（3次调用结果一致）\n");
	} else {
		printf("✗ IP 地址获取不稳定\n");
	}
	
	// 主机名稳定性测试
	str sName_A = xrtGetLocalName();
	str sName_B = xrtGetLocalName();
	str sName_C = xrtGetLocalName();
	if ( xrtStrComp(sName_A, sName_B, 0, 0) == 0 && xrtStrComp(sName_B, sName_C, 0, 0) == 0 ) {
		printf("✓ 主机名获取稳定（3次调用结果一致）\n");
	} else {
		printf("✗ 主机名获取不稳定\n");
	}
	if ( sName_A != xCore->sNull ) xrtFree(sName_A);
	if ( sName_B != xCore->sNull ) xrtFree(sName_B);
	if ( sName_C != xCore->sNull ) xrtFree(sName_C);
	
	// 特殊情况：没有网络连接
	printf("\n--- 边界情况测试 ---\n");
	printf("（注：网络功能依赖系统配置，无网络时可能返回空或默认值）\n");
}


