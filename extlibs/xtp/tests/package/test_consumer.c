#include <xtp.h>
int main(void)
{
    xtppacket p={0};p.Type=XTP_NOTIFY;p.Command=xtpCmdID(0);
    unsigned char wire[32];size_t n;xtpframeview f;
    return xtpEncode(&p,1,0,wire,sizeof(wire),&n)!=XTP_OK||n!=32||
        xtpDecode((xbytesview){wire,n},0,&f)!=XTP_OK||f.MessageID!=0;
}
