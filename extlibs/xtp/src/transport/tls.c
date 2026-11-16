#include <xtp/tls.h>
#if defined(XTP_FEATURE_XTP_TLS)
XTP_API void xtpTlsPolicyInit(xtlspolicy* p,bool only)
{
    static const xtlsversion versions[]={XTLS_VERSION_13};
    static const xtlscipher ciphers[]={XTLS_CHACHA20_POLY1305_SHA256,XTLS_AES_128_GCM_SHA256};
    static const uint16 groups[]={XTLS_GROUP_X25519};
    static const xtlssignature signatures[]={XTLS_SIGNATURE_ED25519,XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256};
    if(p)*p=(xtlspolicy){versions,1,ciphers,only?1u:2u,groups,1,signatures,2,XTLS_KEY_SHARE_PREFER_GROUP};
}
XTP_API xtpresult xtpTlsWrite(void* stream,const xtpoutput* out,size_t* n)
{
    if(!stream||!out||!n||out->Mode!=XTP_INPUT_STREAM||!out->Reliable)return XTP_E_ARGUMENT;
    *n=0;if(xrtTlsStreamState(stream)!=XTLS_STREAM_OPEN)return XTP_CLOSED;
    xtlssession* session=xrtTlsStreamSession(stream);xbytesview protocol={0};
    xtlscipher cipher=xrtTlsSessionCipher(session);
    if(xrtTlsSessionVersion(session)!=XTLS_VERSION_13||
        (cipher!=XTLS_CHACHA20_POLY1305_SHA256&&cipher!=XTLS_AES_128_GCM_SHA256)||
        !xrtTlsSessionProtocol(session,&protocol)||protocol.Size!=5||memcmp(protocol.Data,"xtp/3",5))return XTP_E_STATE;
    xtlsresult r=xrtTlsStreamSend(stream,out->Data.Data,out->Data.Size,n);
    return r==XTLS_OK?XTP_OK:r==XTLS_AGAIN?XTP_AGAIN:r==XTLS_CLOSED?XTP_CLOSED:XTP_E_STATE;
}
#endif
