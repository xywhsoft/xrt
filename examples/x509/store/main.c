#include <xrt.h>

#include "../../../tests/fixtures/x509_path_legacy.h"

#include <stdio.h>



/* 建立拥有式信任库，并生成可与外部中间证书组合的建链源。 */
int main(void)
{
	xx509store* pStore = xrtX509StoreCreate();
	xx509pathsource Source;

	if ( (pStore == NULL) || (xrtX509StoreAdd(
		pStore, X509_PATH_ROOT, sizeof(X509_PATH_ROOT)
	) != X509_VALUE) || !xrtX509StoreSource(
		pStore, NULL, 0, &Source
	) ) {
		fprintf(stderr, "%s\n", xrtErrorMessage(xrtGetError()));
		xrtX509StoreFree(pStore);
		return 1;
	}
	printf("trust store contains %zu anchor(s)\n", Source.AnchorCount);
	xrtX509StoreFree(pStore);
	return 0;
}
