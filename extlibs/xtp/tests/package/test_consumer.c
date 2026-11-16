#include <xtp.h>
#if defined(XTP_FEATURE_XTP_READER)
static xtpflow consume_frame(xtpreader* r,const xtpframeview* f,void* user)
{(void)r;(void)f;++*(int*)user;return XTP_FLOW_ACCEPT;}
#endif
#if defined(XTP_FEATURE_XTP_SESSION)
static xtpdecision consume_message(xtpsession* s,const xtpmessageview* m,void* user)
{(void)s;(void)m;++*(int*)user;return xtpAccept();}
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
#if defined(XTP_FEATURE_XTP_SESSION)
    xtpsession* session=NULL;int messages=0;xtpevents events={consume_message,NULL,NULL};
    if(xtpCreate(NULL,&events,&messages,&session)!=XTP_OK)return 5;
    xtplane lane={1,XTP_INPUT_STREAM,true,0};xtpbinding binding={&lane,1,false,true,NULL};uint64_t generation;
    if(xtpBind(session,&binding,0,&generation)!=XTP_OK)return 6;
    xtpreadprogress used;if(xtpInput(session,generation,1,(xbytesview){wire,n},false,0,&used)!=XTP_OK||messages!=1)return 7;
    if(xtpClose(session,0)!=XTP_CLOSED||xtpDestroy(session)!=XTP_OK)return 8;
#endif
    return 0;
}
