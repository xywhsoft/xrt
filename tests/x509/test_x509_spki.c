#include "../test_allocator.h"
#include "../fixtures/x509_spki_vectors.h"

static bool borrows(xbytesview source,xbytesview view)
{
    uintptr_t first=(uintptr_t)source.Data,at=(uintptr_t)view.Data;
    return view.Data && view.Size<=source.Size && at>=first && at-first<=source.Size-view.Size;
}
/* Standalone SPKI must not need a fabricated certificate or an allocation. */
int main(void)
{
    testRequire(testInstallFailAllocator(),"SPKI zero-allocation allocator install failed");
    for(size_t i=0;i<sizeof(spki_vectors)/sizeof(*spki_vectors);++i) {
        const SpkiVector* vector=&spki_vectors[i];xx509pubkey key;
        testRequire(xrtX509PublicKeyParse(vector->der,&key),"standalone SPKI parse failed");
        testRequire(key.Type==vector->type && key.Curve==vector->curve,"standalone key identity changed");
        testRequire(borrows(vector->der,key.Algorithm.Raw),"algorithm view does not borrow its SPKI");
        testRequire(key.Key.Size && borrows(vector->der,key.Key),"key view escaped source SPKI");
        if(key.Type==X509_KEY_RSA)testRequire(key.Modulus.Size && key.Exponent.Size,"RSA components missing");
        testRequire(xrtGetError()==NULL,"valid standalone SPKI published an error");
    }
    printf("[PASS] x509_spki: seven normal keys, zero allocation\n");return 0;
}
