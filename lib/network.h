


// 获取本机IP地址



// 获取本机 IP ( 返回字符串 )
char* GetLocalIP()
{
	str sRet = xCore.sNull;
	str sLocalName = malloc(260);
	if ( gethostname(sLocalName, 260) == 0 ) {
		struct hostent* host = gethostbyname(sLocalName);
		if ( host ) {
			sRet = xrtFormat("%d.%d.%d.%d", (uint8)host->h_addr_list[0][0], (uint8)host->h_addr_list[0][1], (uint8)host->h_addr_list[0][2], (uint8)host->h_addr_list[0][3]);
		}
	}
	free(sLocalName);
	return sRet;
}



// 获取本机 MAC 地址



// 获取本机名称
char* GetLocalName()
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		DWORD iSize = MAX_COMPUTERNAME_LENGTH + 1;
		str sName = malloc(iSize);
		if ( GetComputerNameA(sName, &iSize) ) {
			return sName;
		} else {
			return xCore.sNull;
		}
	#else
		return xCore.sNull;
	#endif
}


