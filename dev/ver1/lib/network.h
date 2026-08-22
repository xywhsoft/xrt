


static volatile long __xrtNetworkHostLock = 0;


static inline void __xrtNetworkHostLockEnter()
{
	while ( __xrtAtomicCompareExchange32(&__xrtNetworkHostLock, 1, 0) != 0 ) {
	}
}


static inline void __xrtNetworkHostLockLeave()
{
	__xrtAtomicExchange32(&__xrtNetworkHostLock, 0);
}


// 获取本机 IP （ 需使用 xrtFree 释放 ）
str xrtGetLocalIP()
{
	str sRet = xCore.sNull;
	char sLocalName[260];
	if ( gethostname(sLocalName, 260) == 0 ) {
		#ifdef __TINYC__
			// TCC 不支持 getaddrinfo，使用 gethostbyname
			__xrtNetworkHostLockEnter();
			struct hostent* host = gethostbyname(sLocalName);
			if ( host ) {
				sRet = xrtFormat("%d.%d.%d.%d", (uint8)(host->h_addr_list[0][0]), (uint8)(host->h_addr_list[0][1]), (uint8)(host->h_addr_list[0][2]), (uint8)(host->h_addr_list[0][3]));
			}
			__xrtNetworkHostLockLeave();
		#else
			struct addrinfo hints, *res, *p;
			memset(&hints, 0, sizeof(hints));
			hints.ai_family = AF_INET;  // IPv4
			hints.ai_socktype = SOCK_STREAM;
			if ( getaddrinfo(sLocalName, NULL, &hints, &res) == 0 ) {
				for ( p = res; p != NULL; p = p->ai_next ) {
					if ( p->ai_family == AF_INET ) {
						struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
						uint8* addr = (uint8*)&ipv4->sin_addr;
						sRet = xrtFormat("%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
						break;
					}
				}
				freeaddrinfo(res);
			}
		#endif
	}
	return sRet;
}



// 获取本机 IP （ 返回 uint32 ）
uint32 xrtGetLocalRawIP()
{
	char sLocalName[260];
	if ( gethostname(sLocalName, 260) == 0 ) {
		#ifdef __TINYC__
			// TCC 不支持 getaddrinfo，使用 gethostbyname
			__xrtNetworkHostLockEnter();
			struct hostent* host = gethostbyname(sLocalName);
			if ( host ) {
				uint32 iRet = ((uint32)(uint8)(host->h_addr_list[0][0]) << 24) | ((uint32)(uint8)(host->h_addr_list[0][1]) << 16) | ((uint32)(uint8)(host->h_addr_list[0][2]) << 8) | (uint32)(uint8)(host->h_addr_list[0][3]);
				__xrtNetworkHostLockLeave();
				return iRet;
			}
			__xrtNetworkHostLockLeave();
		#else
			struct addrinfo hints, *res, *p;
			memset(&hints, 0, sizeof(hints));
			hints.ai_family = AF_INET;  // IPv4
			hints.ai_socktype = SOCK_STREAM;
			if ( getaddrinfo(sLocalName, NULL, &hints, &res) == 0 ) {
				for ( p = res; p != NULL; p = p->ai_next ) {
					if ( p->ai_family == AF_INET ) {
						struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
						uint8* addr = (uint8*)&ipv4->sin_addr;
						uint32 result = ((uint32)addr[0] << 24) | ((uint32)addr[1] << 16) | ((uint32)addr[2] << 8) | (uint32)addr[3];
						freeaddrinfo(res);
						return result;
					}
				}
				freeaddrinfo(res);
			}
		#endif
	}
	return 0;
}



// 获取本机 MAC 地址 （ 需使用 xrtFree 释放 ）
str xrtGetLocalMAC()
{
	#if defined(_WIN32) || defined(_WIN64)
		// 分类 IP_ADAPTER_INFO 结构内存
		ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
		PIP_ADAPTER_INFO pAdapterInfo = xrtMalloc(sizeof(IP_ADAPTER_INFO));
		if ( pAdapterInfo == NULL ) {
			return xCore.sNull;
		}
		//空间不够，重新分配
		if ( GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW ) {
			xrtFree(pAdapterInfo);
			pAdapterInfo = xrtMalloc(ulOutBufLen);
			if ( pAdapterInfo == NULL ) {
				return xCore.sNull;
			}
		}
		// 获取第一个 MAC 地址
		str sRet = xCore.sNull;
		DWORD errValue = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
		if ( errValue == NO_ERROR ) {
			sRet = xrtHexEncode(pAdapterInfo->Address, 6);
		}
		if ( pAdapterInfo ) {
			xrtFree(pAdapterInfo);
		}
		return sRet;
	#elif defined(__linux__)
		str sRet = xCore.sNull;

		#if !defined(__ANDROID__) || !defined(__ANDROID_API__) || __ANDROID_API__ >= 24
		struct ifaddrs* pIfList = NULL;
		struct ifaddrs* pIf = NULL;
		if ( getifaddrs(&pIfList) == 0 ) {
			for ( pIf = pIfList; pIf != NULL; pIf = pIf->ifa_next ) {
				const struct sockaddr_ll* pAddr;
				const uint8* pMac;
				bool bZero = true;
				if ( pIf->ifa_addr == NULL || pIf->ifa_addr->sa_family != AF_PACKET ) {
					continue;
				}
				if ( (pIf->ifa_flags & IFF_LOOPBACK) != 0 || (pIf->ifa_flags & IFF_UP) == 0 ) {
					continue;
				}
				pAddr = (const struct sockaddr_ll*)pIf->ifa_addr;
				if ( pAddr->sll_halen != 6 ) {
					continue;
				}
				pMac = (const uint8*)pAddr->sll_addr;
				for ( int i = 0; i < 6; ++i ) {
					if ( pMac[i] != 0 ) {
						bZero = false;
						break;
					}
				}
				if ( bZero ) {
					continue;
				}
				sRet = xrtHexEncode((ptr)pMac, 6);
				break;
			}
			freeifaddrs(pIfList);
			if ( sRet != xCore.sNull ) {
				return sRet;
			}
		}
		#endif

		struct ifreq buf[16];
		struct ifconf ifc;
		int fd = socket(AF_INET, SOCK_DGRAM, 0);
		if ( fd < 0 ) {
			xrtSetError("socket error !", FALSE);
			return xCore.sNull;
		}
		ifc.ifc_len = sizeof(buf);
		ifc.ifc_buf = (caddr_t)buf;
		if ( ioctl(fd, SIOCGIFCONF, (char*)&ifc) ) {
			xrtSetError(xrtFormat("ioctl (SIOCGIFCONF) : %s [%s:%d]", strerror(errno), __FILE__, __LINE__), TRUE);
			close(fd);
			return xCore.sNull;
		}
		int interfaceNum = ifc.ifc_len / sizeof(struct ifreq);
		if ( interfaceNum <= 0 ) {
			xrtSetError("Network device not found !", FALSE);
			close(fd);
			return xCore.sNull;
		}
		for ( int i = 0; i < interfaceNum; ++i ) {
			struct ifreq ifrFlags = buf[i];
			struct ifreq ifrHw = buf[i];
			uint8* pMac;
			bool bZero = true;
			if ( ioctl(fd, SIOCGIFFLAGS, &ifrFlags) != 0 ) {
				continue;
			}
			if ( (ifrFlags.ifr_flags & IFF_LOOPBACK) != 0 || (ifrFlags.ifr_flags & IFF_UP) == 0 ) {
				continue;
			}
			if ( ioctl(fd, SIOCGIFHWADDR, &ifrHw) != 0 ) {
				continue;
			}
			pMac = (uint8*)ifrHw.ifr_hwaddr.sa_data;
			for ( int j = 0; j < 6; ++j ) {
				if ( pMac[j] != 0 ) {
					bZero = false;
					break;
				}
			}
			if ( bZero ) {
				continue;
			}
			sRet = xrtHexEncode(pMac, 6);
			break;
		}
		close(fd);
		if ( sRet == xCore.sNull ) {
			DIR* pDir = opendir("/sys/class/net");
			if ( pDir ) {
				struct dirent* pEntry;
				while ( (pEntry = readdir(pDir)) != NULL ) {
					char sPath[512];
					char sLine[64];
					unsigned int arrMac[6];
					bool bZero = true;
					FILE* pFile;
					if ( strcmp(pEntry->d_name, ".") == 0 || strcmp(pEntry->d_name, "..") == 0 || strcmp(pEntry->d_name, "lo") == 0 ) {
						continue;
					}
					snprintf(sPath, sizeof(sPath), "/sys/class/net/%s/address", pEntry->d_name);
					pFile = fopen(sPath, "r");
					if ( !pFile ) {
						continue;
					}
					if ( fgets(sLine, sizeof(sLine), pFile) == NULL ) {
						fclose(pFile);
						continue;
					}
					fclose(pFile);
					if ( sscanf(sLine, "%02x:%02x:%02x:%02x:%02x:%02x",
						&arrMac[0], &arrMac[1], &arrMac[2], &arrMac[3], &arrMac[4], &arrMac[5]) != 6 ) {
						continue;
					}
					for ( int j = 0; j < 6; ++j ) {
						if ( arrMac[j] != 0 ) {
							bZero = false;
							break;
						}
					}
					if ( bZero ) {
						continue;
					}
					{
						uint8 arrBytes[6];
						for ( int j = 0; j < 6; ++j ) {
							arrBytes[j] = (uint8)(arrMac[j] & 0xFFu);
						}
						sRet = xrtHexEncode(arrBytes, 6);
					}
					break;
				}
				closedir(pDir);
			}
		}
		if ( sRet == xCore.sNull ) {
			xrtSetError("Network device not found !", FALSE);
		}
		return sRet;
	#elif defined(__APPLE__) && defined(__MACH__)
		struct ifaddrs* pIfList = NULL;
		struct ifaddrs* pIf = NULL;
		str sRet = xCore.sNull;

		if ( getifaddrs(&pIfList) != 0 ) {
			xrtSetError(xrtFormat("getifaddrs : %s [%s:%d]", strerror(errno), __FILE__, __LINE__), TRUE);
			return xCore.sNull;
		}
		for ( pIf = pIfList; pIf != NULL; pIf = pIf->ifa_next ) {
			const struct sockaddr_dl* pAddr;
			const uint8* pMac;
			bool bZero = true;
			if ( pIf->ifa_addr == NULL || pIf->ifa_addr->sa_family != AF_LINK ) {
				continue;
			}
			if ( (pIf->ifa_flags & IFF_LOOPBACK) != 0 || (pIf->ifa_flags & IFF_UP) == 0 ) {
				continue;
			}
			pAddr = (const struct sockaddr_dl*)pIf->ifa_addr;
			if ( pAddr->sdl_type != IFT_ETHER || pAddr->sdl_alen != 6 ) {
				continue;
			}
			pMac = (const uint8*)LLADDR(pAddr);
			for ( int i = 0; i < 6; ++i ) {
				if ( pMac[i] != 0 ) {
					bZero = false;
					break;
				}
			}
			if ( bZero ) {
				continue;
			}
			sRet = xrtHexEncode((ptr)pMac, 6);
			break;
		}
		freeifaddrs(pIfList);
		if ( sRet == xCore.sNull ) {
			xrtSetError("Network device not found !", FALSE);
		}
		return sRet;
	#else
		xrtSetError("network MAC address query is not implemented on this platform", FALSE);
		return xCore.sNull;
	#endif
}



// windows 获取网卡信息 - 代码备份 : https://www.cnblogs.com/qing123/p/13223027.html
/*


// test 相关处理
void test()
{
	// 分类 IP_ADAPTER_INFO 结构内存
	ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
	PIP_ADAPTER_INFO pAdapterInfo = xrtMalloc(sizeof(IP_ADAPTER_INFO));
	if ( pAdapterInfo == NULL ) {
		return xCore.sNull;
	}
	//空间不够，重新分配
	if ( GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW ) {
		xrtFree(pAdapterInfo);
		pAdapterInfo = xrtMalloc(ulOutBufLen);
		if ( pAdapterInfo == NULL ) {
			return xCore.sNull;
		}
	}
	DWORD errValue = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
	if ( errValue == NO_ERROR ) {
		PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
		while ( pAdapter ) {
			printf("Adapter Name : %s \n", pAdapter->AdapterName);//名字
			printf("Adapter Desc : %s \n", pAdapter->Description);//描述
			printf("Adapter Mac : %s \n", xrtHexEncode(pAdapter->Address, 6));//MAC地址
			if ( pAdapter->Type == MIB_IF_TYPE_OTHER ) {
				printf("网卡类型：其他\n");
			} else if ( pAdapter->Type == MIB_IF_TYPE_ETHERNET ) {
				printf("网卡类型：以太网接口\n");
			} else if ( pAdapter->Type == IF_TYPE_ISO88025_TOKENRING ) {
				printf("网卡类型：ISO88025令牌环\n");
			} else if ( pAdapter->Type == MIB_IF_TYPE_PPP ) {
				printf("网卡类型：PPP接口\n");
			} else if ( pAdapter->Type == MIB_IF_TYPE_LOOPBACK ) {
				printf("网卡类型：软件回路接口\n");
			} else if ( pAdapter->Type == MIB_IF_TYPE_SLIP ) {
				printf("网卡类型：ATM网络接口\n");
			} else if ( pAdapter->Type == IF_TYPE_IEEE80211 ) {
				printf("网卡类型：无线网络接口\n");
			} else {
				printf("网卡类型：未知接口\n");
			}
			printf("IP地址：%s \n", pAdapter->IpAddressList.IpAddress.String);
			printf("子网掩码：%s \n", pAdapter->IpAddressList.IpMask.String);
			printf("默认网关：%s \n", pAdapter->GatewayList.IpAddress.String);
			printf("是否DHCP：%d \n", pAdapter->DhcpEnabled);
			printf("DHCP地址：%s \n", pAdapter->DhcpServer.IpAddress.String);
			pAdapter = pAdapter->Next;
		}
	} else {
		printf("GetAdaptersInfo failed with error: %d\n", errValue);
	}
	if ( pAdapterInfo ) {
		xrtFree(pAdapterInfo);
	}
}
*/



// 获取本机名称 （ 需使用 xrtFree 释放 ）
str xrtGetLocalName()
{
	char sLocalName[260];
	if ( gethostname(sLocalName, 260) == 0 ) {
		return xrtCopyStr((str)sLocalName, 0);
	}
	return xCore.sNull;
}
