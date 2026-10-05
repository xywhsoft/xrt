/* Public generated-header regression: real constructors and saved callbacks. */
#define XACME_MODULE_ALL
#define XRT_MODULE_MEMORY_DEBUG
#define XACME_IMPLEMENTATION
#include <xacme/features.h>
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xacme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool single_create(unsigned kind, xnetengine* engine, xacmednsprovider* provider)
{
	switch(kind) {
	case 0: {
		xacmednaliconfig c; xrtAcmeDnsAliConfigInit(&c);
		c.sAccessKeyId = "test-id"; c.sAccessKeySecret = "test-key"; c.sEndpoint = "localhost:9";
		return xrtAcmeDnsAli(&c, engine, provider);
	}
	case 1: {
		xacmednscfconfig c; xrtAcmeDnsCfConfigInit(&c);
		c.sApiToken = "test-token"; c.sEndpoint = "localhost:9";
		return xrtAcmeDnsCf(&c, engine, provider);
	}
	case 2: {
		xacmednstencentconfig c; xrtAcmeDnsTencentConfigInit(&c);
		c.sSecretId = "test-id"; c.sSecretKey = "test-key"; c.sEndpoint = "localhost:9";
		return xrtAcmeDnsTencent(&c, engine, provider);
	}
	case 3: {
		xacmednsawsconfig c; xrtAcmeDnsAwsConfigInit(&c);
		c.sAccessKeyId = "test-id"; c.sSecretAccessKey = "test-key"; c.sEndpoint = "localhost:9";
		return xrtAcmeDnsAws(&c, engine, provider);
	}
	case 4: {
		xacmednshuaaweiconfig c; xrtAcmeDnsHuaweiConfigInit(&c);
		c.sAccessKey = "test-id"; c.sSecretKey = "test-key"; c.sEndpoint = "localhost:9";
		return xrtAcmeDnsHuawei(&c, engine, provider);
	}
	default: return false;
	}
}

typedef struct single_allocation_check { ptr address; bool live; } single_allocation_check;

static bool single_find_allocation(const xmemdebugallocation* allocation, ptr data)
{
	single_allocation_check* check = (single_allocation_check*)data;
	if(allocation->Address == check->address) check->live = true;
	return !check->live;
}

static bool single_allocation_live(ptr address)
{
	single_allocation_check check = { address, false };
	xrtMemDebugVisitLive(single_find_allocation, &check);
	return check.live;
}

static bool single_rejected(xacmednsaddproc operation, xacmednsprovider* provider,
	xerrkind kind, int32 code)
{
	bool result, ok;
	const xerror* error;
	xrtClearError();
	xrtSetErrorInfo(XERR_IO, "test.single.sentinel", 93, "replace this error");
	result = operation(provider, XRT_STR_LITERAL("_acme-challenge.api.example.com"),
		XRT_STR_LITERAL("digest_01"));
	error = xrtGetError();
	ok = !result && error != NULL && xrtErrorKind(error) == kind &&
		strcmp(xrtErrorDomain(error), "xrt.acme.dns") == 0 && xrtErrorCode(error) == code;
	xrtClearError();
	return ok;
}

static bool single_guards(void)
{
	static void (*const units[])(xacmednsprovider*) = {
		xrtAcmeDnsAliProviderUnit, xrtAcmeDnsCfProviderUnit, xrtAcmeDnsTencentProviderUnit,
		xrtAcmeDnsAwsProviderUnit, xrtAcmeDnsHuaweiProviderUnit
	};
	unsigned checks = 0u;
	for(unsigned kind = 0u; kind < 5u; kind++) {
		for(unsigned borrowed = 0u; borrowed < 2u; borrowed++) {
			xacmednsprovider provider = { 0 };
			xnetengine* engine = NULL;
			xacmednsaddproc add;
			xacmednsremoveproc remove;
			ptr context;
			xmemdebugsnapshot memory;
			if(borrowed != 0u) {
				xnetengineconfig c; xrtNetEngineConfigInit(&c); c.Workers = 1u;
				engine = xrtNetEngineCreate(&c);
				if(engine == NULL || !xrtNetEngineStart(engine)) return false;
			}
			if(!single_create(kind, engine, &provider)) return false;
			context = provider.pContext; add = provider.Add; remove = provider.Remove;
			units[kind](&provider); units[kind](&provider); units[kind](NULL);
			if(provider.pContext != NULL || single_allocation_live(context)) return false;
			if(!single_rejected(add, &provider, XERR_STATE, XACME_DNS_ERROR_STATE) ||
				!single_rejected(remove, &provider, XERR_STATE, XACME_DNS_ERROR_STATE) ||
				!single_rejected(add, NULL, XERR_ARGUMENT, XACME_DNS_ERROR_ARGUMENT) ||
				!single_rejected(remove, NULL, XERR_ARGUMENT, XACME_DNS_ERROR_ARGUMENT)) return false;
			checks += 4u;
			if(engine != NULL) {
				if(!single_allocation_live(engine) || xrtNetEngineState(engine) != XNET_ENGINE_RUNNING ||
					!xrtNetEngineDestroy(engine)) return false;
			}
			xrtMemDebugSnapshot(&memory);
			if(memory.LiveCount != 0u || memory.LiveBytes != 0u ||
				memory.InvalidFreeCount != 0u || memory.DoubleFreeCount != 0u) return false;
		}
	}
	printf("  public aggregate provider guards: %u exact-error checks, owned/borrowed engines, zero live allocations\n", checks);
	return checks == 40u;
}

int main(int argc, char** argv)
{
	(void)argv;
	/* No connection is made; exercise the public constructors' system trust store. */
	if((argc != 1 && argc != 2) || !xrtMemDebugEnable(true)) return 2;
	return single_guards() ? 0 : 1;
}
