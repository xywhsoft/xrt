#include <xtp.h>
#if defined(XTP_FEATURE_XTP_READER)
static xtpflow consume_frame(xtpreader* r,const xtpframeview* f,void* user)
{(void)r;(void)f;++*(int*)user;return XTP_FLOW_ACCEPT;}
#endif
int main(void)
{
    xtppacket p={0};p.Type=XTP_NOTIFY;p.Command=xtpCmdID(0);
    unsigned char wire[32];size_t n;xtpframeview f;
    if(xtpEncode(&p,1,0,wire,sizeof(wire),&n)!=XTP_OK||n!=32||
        xtpDecode((xbytesview){wire,n},0,&f)!=XTP_OK||f.MessageID!=0)return 1;
#if defined(XTP_FEATURE_XTP_READER)
    xtpreader* r=NULL;int count=0;xtpreaderevents e={consume_frame,NULL};xtpreadprogress progress;
    if(xtpReaderCreate(NULL,&e,&count,&r)!=XTP_OK)return 2;
    if(xtpReaderInput(r,(xbytesview){wire,n},false,0,&progress)!=XTP_OK||count!=1)return 3;
    if(xtpReaderEnd(r,0)!=XTP_END||xtpReaderDestroy(r)!=XTP_OK)return 4;
#endif
    return 0;
}
