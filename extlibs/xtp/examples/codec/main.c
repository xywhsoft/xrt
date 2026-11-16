#include <xtp.h>
#include <stdio.h>
int main(void)
{
    const unsigned char tick[]={1,0,0,0};
    xtpfield fields[]={{XRT_STR_INIT("tick"),{tick,sizeof(tick)}},{XRT_STR_INIT("dx"),XRT_BYTES_INIT("2")}};
    xtppacket p={0};p.Type=XTP_REQUEST;p.MessageID=1;p.Command=xtpCmdID(1001);p.Params=xtpTable(fields,2);
    unsigned char wire[256];size_t n;xtpframeview f;xbytesview value;
    if(xtpEncode(&p,1,NULL,wire,sizeof(wire),&n)!=XTP_OK||xtpDecode((xbytesview){wire,n},NULL,&f)!=XTP_OK)return 1;
    if(xtpParamAt(&f.Params,0,&value)!=XTP_PARAM_FOUND||value.Size!=4||value.Data[0]!=1)return 2;
    printf("XTP3 command=%u params=%zu first=tick bytes=%zu\n",f.Command.As.ID,f.Params.Count,n);return 0;
}
