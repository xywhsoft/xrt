#include <xtp.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct transport_test {unsigned char Wire[2048];size_t Size,Messages,Completions;bool Block,Fail,Pause;xtpsession* Protected;} transport_test;
static xtpdecision receive(xtpsession* s,const xtpmessageview* m,void* user)
{transport_test* t=user;(void)s;(void)m;if(t->Pause){t->Pause=false;return xtpPause();}++t->Messages;return xtpAccept();}
static void completed(xtpsession* s,const xtpcompletion* c,void* user)
{(void)s;(void)c;++((transport_test*)user)->Completions;}
static xtpresult write_bytes(void* user,const xtpoutput* o,size_t* n)
{
    transport_test* t=user;*n=0;assert(xtpClose(t->Protected,0)==XTP_E_STATE);if(t->Block)return XTP_AGAIN;
    size_t take=t->Fail?1:o->Data.Size;assert(t->Size+take<=sizeof(t->Wire));
    memcpy(t->Wire+t->Size,o->Data.Data,take);t->Size+=take;*n=take;return t->Fail?XTP_E_STATE:XTP_OK;
}
static xtpsession* create(transport_test* t,bool record,uint64_t* binding)
{
    xtpsession* s=NULL;xtpevents e={receive,completed,NULL};assert(xtpCreate(NULL,&e,t,&s)==XTP_OK);
    xtplane lane={1,record?XTP_INPUT_RECORD:XTP_INPUT_STREAM,true,1048576};xtpbinding b={&lane,1,false,true};
    assert(xtpBind(s,&b,0,binding)==XTP_OK);t->Protected=s;return s;
}
static void finish(xtpsession* s)
{assert(xtpClose(s,0)==XTP_CLOSED&&xtpDestroy(s)==XTP_OK);}
int main(void)
{
    transport_test a={0},b={0};uint64_t ab,bb;xtpsession* left=create(&a,false,&ab),*right=create(&b,false,&bb);
    xtpmessage m={0};m.Type=XTP_NOTIFY;m.Command=xtpCmdID(1);unsigned char body[128]={0};m.Body=(xbytesview){body,128};
    assert(xtpSubmit(left,&m,NULL,0,NULL)==XTP_OK);xtpwritebudget budget={40,2,9};xtpwriteprogress p;
    a.Block=true;assert(xtpDrain(left,1,write_bytes,&a,&budget,0,&p)==XTP_AGAIN&&p.Bytes==0&&p.Calls==1);
    a.Block=false;while(!a.Completions){assert(xtpDrain(left,1,write_bytes,&a,&budget,0,&p)==XTP_AGAIN);assert(p.Bytes<=40);}
    assert(a.Size==160);xtpreadprogress read;assert(xtpInput(right,bb,1,(xbytesview){a.Wire,a.Size},false,0,&read)==XTP_OK&&b.Messages==1);
    m.Body=(xbytesview){NULL,0};assert(xtpSubmit(left,&m,NULL,0,NULL)==XTP_OK);a.Fail=true;
    assert(xtpDrain(left,1,write_bytes,&a,NULL,0,&p)==XTP_E_STATE&&p.Bytes==1);
    xtpoutput out;assert(xtpOutput(left,1,&out)==XTP_OK&&out.Data.Size==31);finish(left);finish(right);
    memset(&a,0,sizeof(a));left=create(&a,true,&ab);assert(xtpSubmit(left,&m,NULL,0,NULL)==XTP_OK);
    budget.Bytes=31;assert(xtpDrain(left,1,write_bytes,&a,&budget,0,&p)==XTP_E_LIMIT&&!p.Bytes);
    budget.Bytes=64;assert(xtpDrain(left,1,write_bytes,&a,&budget,0,&p)==XTP_OK&&p.Bytes==32&&p.Frames==1);
#if defined(XTP_FEATURE_XTP_WS)
    memset(&b,0,sizeof(b));right=create(&b,true,&bb);xtpwsinput input={right,bb,1,false};
    assert(xtpWsBegin(&input,XWS_OPCODE_TEXT)==XTP_E_TYPE&&!input.Active);
    assert(xtpWsBegin(&input,XWS_OPCODE_BINARY)==XTP_OK);
    assert(xtpWsData(&input,(xbytesview){a.Wire,7},0,&read)==XTP_MORE);
    assert(xtpWsData(&input,(xbytesview){a.Wire+7,25},0,&read)==XTP_OK&&b.Messages==1);
    assert(xtpWsEnd(&input,0,&read)==XTP_OK&&!input.Active);
    assert(xtpWsBegin(&input,XWS_OPCODE_BINARY)==XTP_OK);
    assert(xtpWsData(&input,(xbytesview){a.Wire,9},0,&read)==XTP_MORE);
    assert(xtpWsEnd(&input,0,&read)==XTP_DROPPED&&!input.Active);finish(right);
#endif
#if defined(XTP_FEATURE_XTP_TLS)
    xtlspolicy policy;xtpTlsPolicyInit(&policy,false);assert(xrtTlsPolicyValid(&policy));
    assert(policy.VersionCount==1&&policy.Versions[0]==XTLS_VERSION_13&&policy.CipherCount==2&&policy.Groups[0]==XTLS_GROUP_X25519);
    xtpTlsPolicyInit(&policy,true);assert(policy.CipherCount==1&&policy.Ciphers[0]==XTLS_CHACHA20_POLY1305_SHA256);
#endif
    finish(left);puts("xtp transport: budget/short-write/error/record/WS boundaries/TLS policy PASS");return 0;
}
