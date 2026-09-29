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

static void mailExampleNetUnit(mail_example_net* net)
{
	if ( net == NULL ) return;
	if ( net->Resolver != NULL ) (void)xrtNetResolverDestroy(net->Resolver);
	if ( net->Verifier != NULL ) xrtTlsVerifierRelease(net->Verifier);
	if ( net->Tls != NULL ) xrtTlsContextRelease(net->Tls);
	if ( net->Engine != NULL ) (void)xrtNetEngineDestroy(net->Engine);
	memset(net, 0, sizeof(*net));
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
