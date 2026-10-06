#include <xrt/detail/wait.h>
#ifndef XMAIL_EXAMPLE_CLIENT_SETUP_H
#define XMAIL_EXAMPLE_CLIENT_SETUP_H

/* POP3/SMTP/IMAP 真实客户端范例共享的网络与 CA 配置。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct mail_example_net {
	xnetengine* Engine;
	xnetresolver* Resolver;
	xtlscontext* Tls;
	xtlsverifier* Verifier;
} mail_example_net;

static bool mailExamplePort(const char* text, uint16* port)
{
	char* end;
	unsigned long value;
	if ( text == NULL || text[0] == 0 || port == NULL ) return false;
	value = strtoul(text, &end, 10);
	if ( *end != 0 || value == 0 || value > 65535u ) return false;
	*port = (uint16)value;
	return true;
}

static bool mailExampleCleanupWait(double deadline)
{
	if ( __xrtWaitExpired(deadline) ) {
		xerror* timeout = xrtErrorCreate(XERR_TIMEOUT, "mail-example", 1,
			"network cleanup still has live objects");
		if ( timeout != NULL ) xrtSetErrorTake(timeout);
		return false;
	}
	xrtSleep(1u);
	return true;
}

/* 只在退休成功时清空拥有型指针；失败后调用方仍可重试。 */
static bool mailExampleNetCleanup(mail_example_net* net, double deadline)
{
	xerror* previous;
	bool ready = true;
	if ( net == NULL ) return true;
	previous = xrtTakeError();
	if ( net->Resolver != NULL ) {
		for ( ;; ) {
			xnetretireresult result = xrtNetResolverTryDestroy(net->Resolver);
			if ( result == XNET_RETIRE_READY ) { net->Resolver = NULL; break; }
			if ( result == XNET_RETIRE_ERROR || !mailExampleCleanupWait(deadline) ) {
				ready = false;
				break;
			}
		}
	}
	if ( ready && net->Engine != NULL ) {
		for ( ;; ) {
			xnetretireresult result = xrtNetEngineTryDestroy(net->Engine);
			if ( result == XNET_RETIRE_READY ) { net->Engine = NULL; break; }
			if ( result == XNET_RETIRE_ERROR ) { ready = false; break; }
			if ( !mailExampleCleanupWait(deadline) ) {
				ready = false;
				break;
			}
		}
	}
	if ( ready ) {
		if ( net->Verifier != NULL ) xrtTlsVerifierRelease(net->Verifier);
		if ( net->Tls != NULL ) xrtTlsContextRelease(net->Tls);
		net->Verifier = NULL;
		net->Tls = NULL;
	}
	if ( previous != NULL ) xrtSetErrorTake(previous);
	else if ( ready ) xrtClearError();
	return ready;
}

static bool mailExampleNetUnit(mail_example_net* net)
{
	bool ready = mailExampleNetCleanup(net, __xrtWaitAfter(INT64_C(5000)));
	if ( !ready ) fputs("Mail network cleanup incomplete; handles retained for retry\n", stderr);
	return ready;
}

/* 不输出认证材料或服务器返回的任意文本。 */
static inline void mailExampleDiagnostic(const char* stage)
{
	fprintf(stderr, "%s failed: kind=%d code=%d\n", stage,
		(int)xrtErrorKind(xrtGetError()), (int)xrtErrorCode(xrtGetError()));
}

static bool mailExampleNetInit(mail_example_net* net, const char* caPath)
{
	FILE* file = NULL;
	long length;
	char* pem = NULL;
	xx509store* store = NULL;
	size_t added = 0;
	xtlsverifierconfig verifier;
	xnetengineconfig engine;
	if ( net == NULL || caPath == NULL ) return false;
	memset(net, 0, sizeof(*net));
	file = fopen(caPath, "rb");
	if ( file == NULL || fseek(file, 0, SEEK_END) != 0 ) goto Fail;
	length = ftell(file);
	if ( length <= 0 || length > 4 * 1024 * 1024 ||
		fseek(file, 0, SEEK_SET) != 0 ) goto Fail;
	pem = (char*)xrtMalloc((size_t)length + 1u);
	if ( pem == NULL || fread(pem, 1u, (size_t)length, file) !=
		(size_t)length ) goto Fail;
	pem[length] = 0;
	if ( fclose(file) != 0 ) { file = NULL; goto Fail; }
	file = NULL;
	store = xrtX509StoreCreate();
	if ( store == NULL || !xrtX509StoreAddPem(store, pem,
		(size_t)length, &added) || added == 0 ) goto Fail;
	xrtTlsVerifierConfigInit(&verifier);
	verifier.Store = store;
	net->Verifier = xrtTlsVerifierCreate(&verifier);
	if ( net->Verifier == NULL ) goto Fail;
	net->Tls = xrtTlsContextCreate(NULL);
	if ( net->Tls == NULL ) goto Fail;
	xrtNetEngineConfigInit(&engine);
	net->Engine = xrtNetEngineCreate(&engine);
	if ( net->Engine == NULL || !xrtNetEngineStart(net->Engine) ) goto Fail;
	net->Resolver = xrtNetResolverCreate(NULL);
	if ( net->Resolver == NULL ) goto Fail;
	xrtX509StoreFree(store);
	xrtFree(pem);
	return true;
Fail:
	if ( file != NULL ) fclose(file);
	xrtX509StoreFree(store);
	xrtFree(pem);
	mailExampleNetUnit(net);
	return false;
}

#endif
