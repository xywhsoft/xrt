


// 获取本机 IP ( 需使用 xrtFree 释放 )
str xrtGetLocalIP()
{
	str sRet = xCore.sNull;
	char sLocalName[260];
	if ( gethostname(sLocalName, 260) == 0 ) {
		struct hostent* host = gethostbyname(sLocalName);
		if ( host ) {
			sRet = xrtFormat("%d.%d.%d.%d", (uint8)(host->h_addr_list[0][0]), (uint8)(host->h_addr_list[0][1]), (uint8)(host->h_addr_list[0][2]), (uint8)(host->h_addr_list[0][3]));
		}
	}
	return sRet;
}



// 获取本机 IP ( 返回 uint32 )
uint32 xrtGetLocalRawIP()
{
	char sLocalName[260];
	if ( gethostname(sLocalName, 260) == 0 ) {
		struct hostent* host = gethostbyname(sLocalName);
		if ( host ) {
			return ((uint8)(host->h_addr_list[0][0]) << 24) | ((uint8)(host->h_addr_list[0][1]) << 16) | ((uint8)(host->h_addr_list[0][2]) << 8) | (uint8)(host->h_addr_list[0][3]);
		}
	}
	return 0;
}



// 获取本机 MAC 地址 ( 需使用 xrtFree 释放 )
str xrtGetLocalMAC()
{
	return xCore.sNull;
}



// 获取本机名称 ( 需使用 xrtFree 释放 )
str xrtGetLocalName()
{
	char sLocalName[260];
	if ( gethostname(sLocalName, 260) == 0 ) {
		return xrtCopyStr(sLocalName, 0);
	}
	return xCore.sNull;
}


