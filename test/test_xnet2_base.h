void Test_XNet2_Base(void)
{
	printf("\n\n\n------------------------------------\n\n XNet2 Base Address Test:\n\n");

	{
		xnetaddr tAddr;
		xrtNetAddrInitAny(&tAddr, AF_INET, 8080);
		printf("  InitAny IPv4 family : %s\n", tAddr.iFamily == AF_INET ? "PASS" : "FAIL");
		printf("  InitAny IPv4 port : %s\n", tAddr.iPort == 8080 ? "PASS" : "FAIL");
		printf("  InitAny IPv4 text : %s\n", strcmp(xrtNetAddrToStr(&tAddr), "0.0.0.0") == 0 ? "PASS" : "FAIL");
	}

	{
		xnetaddr tAddr;
		xrtNetAddrInitAny(&tAddr, AF_INET6, 9090);
		printf("  InitAny IPv6 family : %s\n", tAddr.iFamily == AF_INET6 ? "PASS" : "FAIL");
		printf("  InitAny IPv6 port : %s\n", tAddr.iPort == 9090 ? "PASS" : "FAIL");
		printf("  InitAny IPv6 text : %s\n", strcmp(xrtNetAddrToStr(&tAddr), "::") == 0 ? "PASS" : "FAIL");
	}

	{
		xnetaddr tAddr;
		xnet_result iRet = xrtNetAddrParse(&tAddr, "127.0.0.1", 80);
		printf("  Parse IPv4 literal : %s\n", iRet == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Parse IPv4 family : %s\n", tAddr.iFamily == AF_INET ? "PASS" : "FAIL");
		printf("  Parse IPv4 port : %s\n", tAddr.iPort == 80 ? "PASS" : "FAIL");
		printf("  Parse IPv4 text : %s\n", strcmp(xrtNetAddrToStr(&tAddr), "127.0.0.1") == 0 ? "PASS" : "FAIL");
	}

	{
		xnetaddr tAddr;
		xnet_result iRet = xrtNetAddrParse(&tAddr, "::1", 443);
		printf("  Parse IPv6 literal : %s\n", iRet == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Parse IPv6 family : %s\n", tAddr.iFamily == AF_INET6 ? "PASS" : "FAIL");
		printf("  Parse IPv6 port : %s\n", tAddr.iPort == 443 ? "PASS" : "FAIL");
		printf("  Parse IPv6 text : %s\n", strcmp(xrtNetAddrToStr(&tAddr), "::1") == 0 ? "PASS" : "FAIL");
	}

	{
		xnetaddr tAddr;
		xnet_result iRet = xrtNetAddrParse(&tAddr, "not-an-ip", 0);
		printf("  Reject invalid literal : %s\n", iRet == XRT_NET_ERROR ? "PASS" : "FAIL");
	}

	{
		xnetaddr tAddr;
		xrtNetAddrInitAny(&tAddr, AF_INET, 1234);
		xnet_result iRet = xrtNetResolve("localhost", &tAddr);
		printf("  Resolve localhost : %s\n", iRet == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Resolve localhost family : %s\n",
			iRet == XRT_NET_OK && (tAddr.iFamily == AF_INET || tAddr.iFamily == AF_INET6) ? "PASS" : "FAIL");
		printf("  Resolve localhost preserves port : %s\n",
			iRet == XRT_NET_OK && tAddr.iPort == 1234 ? "PASS" : "FAIL");
		printf("  Resolve localhost text non-empty : %s\n",
			iRet == XRT_NET_OK && xrtNetAddrToStr(&tAddr)[0] != '\0' ? "PASS" : "FAIL");
	}

	{
		xnetlistenconfig tListenCfg;
		xnetdgramconfig tDgramCfg;
		xrtNetListenConfigInit(&tListenCfg);
		xrtNetDgramConfigInit(&tDgramCfg);
		printf("  Listen config bind family : %s\n", tListenCfg.tBindAddr.iFamily == AF_INET ? "PASS" : "FAIL");
		printf("  Dgram config bind family : %s\n", tDgramCfg.tBindAddr.iFamily == AF_INET ? "PASS" : "FAIL");
	}
}
