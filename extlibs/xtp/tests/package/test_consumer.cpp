#include <xtp.h>
#include <cassert>
#include <cstring>
static unsigned completed;
static xtpdecision message(xtpsession* s,const xtpmessageview* m,void*)
{xtpresponse r{};r.Body=XRT_BYTES_LITERAL("cpp");assert(xtpReply(s,m->Request,&r,0)==XTP_OK);return xtpAccept();}
static void result(xtpsession*,const xtpcompletion* c,void*)
{assert(c->Reason==XTP_FINISH_RESPONSE&&c->Body.Size==3&&!std::memcmp(c->Body.Data,"cpp",3));++completed;}
static void transfer(xtpsession* a,xtpsession* b,uint64_t generation)
{xtpoutput o{};while(xtpOutput(a,0,&o)==XTP_OK){unsigned char bytes[64];auto n=o.Data.Size;assert(n<=sizeof(bytes));
    std::memcpy(bytes,o.Data.Data,n);assert(xtpAdvance(a,o.Ticket,n,0)==XTP_OK);xtpreadprogress progress{};
    xbytesview in{bytes,n};assert(xtpInput(b,generation,1,in,false,0,&progress)>=0&&progress.Used==n);}}
int main()
{xtpevents e{message,result,nullptr};xtpsession* a;xtpsession* b;
    assert(xtpCreate(nullptr,&e,nullptr,&a)==XTP_OK&&xtpCreate(nullptr,&e,nullptr,&b)==XTP_OK);
    xtplane lane{1,XTP_INPUT_STREAM,true,0};xtpbinding bind{&lane,1,false,true,nullptr};uint64_t ab,bb;
    assert(xtpBind(a,&bind,0,&ab)==XTP_OK&&xtpBind(b,&bind,0,&bb)==XTP_OK);xtpmessage m{};
    m.Type=XTP_REQUEST;m.Command=xtpCmdID(0);assert(xtpSubmit(a,&m,nullptr,0,nullptr)==XTP_OK);
    transfer(a,b,bb);transfer(b,a,ab);assert(completed==1);
    assert(xtpClose(a,0)==XTP_CLOSED&&xtpClose(b,0)==XTP_CLOSED&&xtpDestroy(a)==XTP_OK&&xtpDestroy(b)==XTP_OK);}
