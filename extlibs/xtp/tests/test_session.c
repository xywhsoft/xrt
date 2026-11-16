#include <xtp.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct session_test {
    xtpsession* Session;uint64_t Binding,Now;
    size_t Messages,Requests,Pushes,Notifies,Completed,Faults,Fatal;
    xtprequesttoken RequestsHeld[256];xtpcompletion Results[512];
    bool AutoReply,Pause,CloseInMessage,CloseInComplete,InvalidDecision,SubmitInComplete;
    xtpdecision Decision;
} session_test;
static xtpmessage message(xtpmessagekind kind)
{xtpmessage m={0};m.Type=kind;m.Command=xtpCmdID(0);return m;}
static xtpdecision on_message(xtpsession* s,const xtpmessageview* m,void* user)
{
    session_test* t=user;++t->Messages;assert(s==t->Session);
    assert(xtpDestroy(s)==XTP_E_STATE);
    uint64_t due;assert(xtpDrive(s,t->Now,&due)==XTP_E_ARGUMENT);
    if(t->Pause){t->Pause=false;return xtpPause();}
    if(m->Params.Count){xbytesview a,b;assert(m->Params.Count==2);
        assert(xtpParamAt(&m->Params,0,&a)==XTP_PARAM_FOUND&&a.Size==1&&a.Data[0]==7);
        assert(xtpParamAt(&m->Params,1,&b)==XTP_PARAM_FOUND&&b.Size==2&&b.Data[0]==8&&b.Data[1]==0);}
    if(m->Type==XTP_REQUEST){assert(t->Requests<256);t->RequestsHeld[t->Requests++]=m->Request;
        if(t->AutoReply){xtpresponse r={0};r.Status=17;r.Params=xtpArray(NULL,0);r.Body=XRT_BYTES_LITERAL("result");
            assert(xtpReply(s,m->Request,&r,t->Now)==XTP_OK);
            assert(xtpReply(s,m->Request,&r,t->Now)==XTP_E_STATE);}}
    else if(m->Type==XTP_PUSH)++t->Pushes;else if(m->Type==XTP_NOTIFY)++t->Notifies;else assert(0);
    if(t->CloseInMessage)assert(xtpClose(s,t->Now)==XTP_OK);
    if(t->InvalidDecision)return (xtpdecision){XTP_ACTION_PAUSE,1};
    return t->Decision.Action?t->Decision:xtpAccept();
}
static void on_complete(xtpsession* s,const xtpcompletion* c,void* user)
{
    session_test* t=user;assert(t->Completed<512);assert(xtpDestroy(s)==XTP_E_STATE);
    if(c->Reason==XTP_FINISH_RESPONSE&&c->Status==17&&c->Body.Size){assert(c->Params.Kind==XTP_PARAMS_ARRAY);
        assert(c->Body.Size==6&&!memcmp(c->Body.Data,"result",6));}
    t->Results[t->Completed++]=*c;
    /* Deliberately do not retain borrowed result/packet pointers. */
    t->Results[t->Completed-1].Params=(xtpparamsview){0};
    t->Results[t->Completed-1].Body=t->Results[t->Completed-1].Packet=(xbytesview){NULL,0};
    if(t->SubmitInComplete){t->SubmitInComplete=false;xtpmessage m=message(XTP_NOTIFY);
        assert(xtpSubmit(s,&m,NULL,t->Now,NULL)==XTP_OK);}
    if(t->CloseInComplete)assert(xtpClose(s,t->Now)==XTP_OK);
}
static void on_fault(xtpsession* s,const xtpfault* f,void* user)
{session_test* t=user;(void)s;++t->Faults;if(f->Fatal)++t->Fatal;}
static void create(session_test* t,xtpconfig* c)
{xtpevents e={on_message,on_complete,on_fault};assert(xtpCreate(c,&e,t,&t->Session)==XTP_OK);}
static void bind_session(session_test* t)
{
    xtplane lanes[]={{1,XTP_INPUT_STREAM,true,0},{2,XTP_INPUT_RECORD,false,1200},{3,XTP_INPUT_RECORD,true,1048576}};
    xtpbinding b={lanes,3,true,false,NULL};assert(xtpBind(t->Session,&b,t->Now,&t->Binding)==XTP_OK);
}
static void destroy(session_test* t)
{
    xtpresult r=xtpClose(t->Session,t->Now);size_t iterations=0;
    while(r==XTP_AGAIN){uint64_t due;r=xtpDrive(t->Session,t->Now,&due);assert(++iterations<1000);}
    assert(r==XTP_CLOSED);assert(xtpDestroy(t->Session)==XTP_OK);t->Session=NULL;
}
static void feed(session_test* t,const unsigned char* bytes,size_t size,uint32_t lane,bool record,size_t chunk)
{
    size_t offset=0,turns=0;while(offset<size){size_t n=size-offset;if(chunk&&n>chunk)n=chunk;xtpreadprogress p;
        xtpresult r=xtpInput(t->Session,t->Binding,lane,(xbytesview){bytes+offset,n},record&&offset+n==size,t->Now,&p);
        if(r<0||r==XTP_PAUSED||r==XTP_CLOSED)fprintf(stderr,"feed result=%d lane=%u offset=%zu/%zu input=%zu used=%zu messages=%zu faults=%zu now=%llu\n",
            (int)r,lane,offset,size,n,p.Used,t->Messages,t->Faults,(unsigned long long)t->Now);
        assert(r>=0&&r!=XTP_PAUSED&&r!=XTP_CLOSED);offset+=p.Used;assert(++turns<100000);}
}
static size_t transfer(session_test* a,session_test* b,size_t chunk,size_t max)
{
    size_t count=0;xtpoutput out;
    while(count<max&&xtpOutput(a->Session,0,&out)==XTP_OK){
        unsigned char* wire=malloc(out.Data.Size);assert(wire);size_t n=out.Data.Size;memcpy(wire,out.Data.Data,n);
        assert(xtpAdvance(a->Session,out.Ticket,n,a->Now)>=0);
        feed(b,wire,n,out.LaneID,out.Mode==XTP_INPUT_RECORD,chunk);free(wire);++count;}
    return count;
}
static xtpinfo info(session_test* t)
{xtpinfo i;assert(xtpInfo(t->Session,&i)==XTP_OK);return i;}
static size_t encode_packet(unsigned char* wire,size_t cap,xtpmessagekind type,uint64_t id,int32_t status,bool result_ack)
{
    xtppacket p={0};p.Type=type;p.MessageID=id;p.Status=status;p.ResultAck=result_ack;
    if(type==XTP_REQUEST||type==XTP_PUSH||type==XTP_NOTIFY)p.Command=xtpCmdID(99);
    size_t n;assert(xtpEncode(&p,1,NULL,wire,cap,&n)==XTP_OK);return n;
}
static void shapes_and_duplex(void)
{
    session_test a={0},b={0};a.AutoReply=b.AutoReply=true;create(&a,NULL);create(&b,NULL);bind_session(&a);bind_session(&b);
    const xbytesview args[]={XRT_BYTES_INIT("\7"),XRT_BYTES_INIT("\10\0")};
    const xtpfield fields[]={{XRT_STR_INIT("z"),XRT_BYTES_INIT("\7")},{XRT_STR_INIT("a"),XRT_BYTES_INIT("\10\0")}};
    xtpmessage m[12];xtpoperationtoken tokens[12];xtpsubmitoptions options[12];memset(options,0,sizeof(options));
    for(size_t i=0;i<12;++i){m[i]=message(i<4?XTP_REQUEST:i<8?XTP_PUSH:XTP_NOTIFY);
        m[i].Command=i%2?xtpCmdName(XRT_STR_LITERAL("move")):xtpCmdID(0);
        m[i].Params=i%4<2?xtpArray(args,2):xtpTable(fields,2);options[i].Tag=(void*)(uintptr_t)(i+1);}
    assert(xtpSubmitBatch(a.Session,m,12,options,0,tokens)==XTP_OK);
    assert(xtpSubmitBatch(b.Session,m,12,options,0,NULL)==XTP_OK);
    assert(transfer(&a,&b,1,100)==12);assert(transfer(&b,&a,3,100)==20);assert(transfer(&a,&b,0,100)==8);
    assert(a.Messages==12&&b.Messages==12&&a.Completed==12&&b.Completed==12);
    assert(a.Requests==4&&a.Pushes==4&&a.Notifies==4);
    for(size_t i=0;i<a.Completed;++i){xtpcompletion* c=&a.Results[i];size_t original=(size_t)(uintptr_t)c->Tag-1;
        assert(!memcmp(&c->Operation,&tokens[original],sizeof(c->Operation)));
        if(original<4)assert(c->Reason==XTP_FINISH_RESPONSE&&c->Knowledge==XTP_KNOW_REPLIED&&c->Status==17);
        else if(original<8)assert(c->Reason==XTP_FINISH_ACK&&c->Knowledge==XTP_KNOW_ACCEPTED);
        else assert(c->Reason==XTP_FINISH_SENT&&c->Knowledge==XTP_KNOW_UNKNOWN&&!c->HasRemoteStatus);}
    assert(!info(&a).Pending&&!info(&a).TxFrames&&!a.Faults&&!b.Faults);destroy(&a);destroy(&b);
}
static void out_of_order_and_types(void)
{
    session_test a={0},b={0};xtpconfig config;xtpConfigInit(&config);config.MaxResponseBytes=256;
    create(&a,&config);create(&b,&config);bind_session(&a);bind_session(&b);
    xtpmessage m[32];for(size_t i=0;i<32;++i)m[i]=message(XTP_REQUEST);
    assert(xtpSubmitBatch(a.Session,m,32,NULL,0,NULL)==XTP_OK);assert(transfer(&a,&b,7,100)==32);
    unsigned char wire[32];encode_packet(wire,32,XTP_ACK,1,0,false);feed(&a,wire,32,1,false,0);
    assert(!a.Completed&&info(&a).UnknownResults==1);
    xtpresponse response={0};for(size_t i=32;i>0;--i){response.Status=(int32_t)i;
        assert(xtpReply(b.Session,b.RequestsHeld[i-1],&response,0)==XTP_OK);}
    assert(transfer(&b,&a,0,100)==32);assert(a.Completed==32);
    for(size_t i=0;i<32;++i)assert(a.Results[i].MessageID==32-i&&a.Results[i].Status==(int32_t)(32-i));
    encode_packet(wire,32,XTP_RESPONSE,1,0,false);feed(&a,wire,32,1,false,0);
    assert(a.Completed==32&&info(&a).UnknownResults==2);destroy(&a);destroy(&b);
}
static void short_writes_and_records(void)
{
    session_test a={0},b={0};create(&a,NULL);create(&b,NULL);bind_session(&a);bind_session(&b);
    xtpmessage m=message(XTP_PUSH);assert(xtpSubmit(a.Session,&m,NULL,0,NULL)==XTP_OK);
    unsigned char wire[32];xtpoutput out;assert(xtpOutput(a.Session,0,&out)==XTP_OK);memcpy(wire,out.Data.Data,32);
    xtpoutputticket old=out.Ticket;assert(xtpAdvance(a.Session,old,0,0)==XTP_AGAIN);
    assert(xtpAdvance(a.Session,old,3,0)==XTP_OK);assert(xtpAdvance(a.Session,old,1,0)==XTP_E_ARGUMENT);
    assert(xtpOutput(a.Session,0,&out)==XTP_OK&&out.Data.Size==29&&!memcmp(out.Data.Data,wire+3,29));
    assert(xtpAdvance(a.Session,out.Ticket,29,0)==XTP_OK);assert(!a.Completed);
    feed(&b,wire,32,1,false,4);assert(transfer(&b,&a,0,1)==1&&a.Completed==1);
    xtpsubmitoptions o={0};o.LaneID=2;m=message(XTP_NOTIFY);assert(xtpSubmit(a.Session,&m,&o,0,NULL)==XTP_OK);
    assert(xtpOutput(a.Session,2,&out)==XTP_OK&&out.Mode==XTP_INPUT_RECORD);
    assert(xtpAdvance(a.Session,out.Ticket,1,0)==XTP_E_ARGUMENT);assert(transfer(&a,&b,2,1)==1&&b.Notifies==1);
    m=message(XTP_REQUEST);assert(xtpSubmit(a.Session,&m,&o,0,NULL)==XTP_E_STATE);
    o.LaneID=3;b.AutoReply=true;assert(xtpSubmit(a.Session,&m,&o,0,NULL)==XTP_OK);
    assert(transfer(&a,&b,1,1)==1&&transfer(&b,&a,1,1)==1);destroy(&a);destroy(&b);
}
static void pause_reject_and_duplicates(void)
{
    session_test a={0},b={0};create(&a,NULL);create(&b,NULL);bind_session(&a);bind_session(&b);b.Pause=true;
    xtpmessage m=message(XTP_PUSH);assert(xtpSubmit(a.Session,&m,NULL,0,NULL)==XTP_OK);xtpoutput out;
    assert(xtpOutput(a.Session,0,&out)==XTP_OK);unsigned char wire[32];memcpy(wire,out.Data.Data,32);
    assert(xtpAdvance(a.Session,out.Ticket,32,0)==XTP_OK);xtpreadprogress progress;
    assert(xtpInput(b.Session,b.Binding,1,(xbytesview){wire,32},false,0,&progress)==XTP_PAUSED&&progress.Used==0);
    assert(!b.Pushes&&!info(&b).TxFrames);assert(xtpResume(b.Session,b.Binding,1,0)==XTP_OK);
    feed(&b,wire,32,1,false,0);assert(b.Pushes==1);assert(transfer(&b,&a,0,1)==1);
    feed(&b,wire,32,1,false,0);assert(b.Pushes==1&&info(&b).Duplicates==1);
    assert(transfer(&b,&a,0,1)==1&&a.Completed==1&&info(&a).UnknownResults==1);
    b.Decision=xtpBusy();assert(xtpSubmit(a.Session,&m,NULL,0,NULL)==XTP_OK);
    assert(transfer(&a,&b,0,1)==1&&transfer(&b,&a,0,1)==1);
    assert(a.Results[1].Status==XTP_STATUS_BUSY&&a.Results[1].Knowledge==XTP_KNOW_REJECTED);
    b.Decision=xtpReject(123);m=message(XTP_REQUEST);assert(xtpSubmit(a.Session,&m,NULL,0,NULL)==XTP_OK);
    assert(transfer(&a,&b,0,1)==1&&transfer(&b,&a,0,1)==1);assert(a.Results[2].Status==123);
    assert(xtpReply(b.Session,b.RequestsHeld[b.Requests-1],&(xtpresponse){0},0)==XTP_E_STATE);destroy(&a);destroy(&b);
}
static void deadlines_and_rebind(void)
{
    session_test a={0},b={0};xtpconfig c;xtpConfigInit(&c);c.DefaultOperationTimeoutUs=100;c.IncomingRequestTimeoutUs=50;
    create(&a,&c);create(&b,&c);bind_session(&a);bind_session(&b);xtpmessage m=message(XTP_REQUEST);
    assert(xtpSubmit(a.Session,&m,NULL,0,NULL)==XTP_OK);uint64_t due;
    a.Now=100;assert(xtpDrive(a.Session,a.Now,&due)==XTP_OK&&a.Completed==1);
    assert(a.Results[0].Reason==XTP_FINISH_TIMEOUT&&a.Results[0].Knowledge==XTP_KNOW_NOT_SENT);
    assert(xtpSubmit(a.Session,&m,NULL,100,NULL)==XTP_OK);assert(transfer(&a,&b,0,1)==1);
    b.Now=50;assert(xtpDrive(b.Session,50,&due)==XTP_OK&&due==150);
    assert(xtpReply(b.Session,b.RequestsHeld[0],&(xtpresponse){0},50)==XTP_E_STATE);
    assert(transfer(&b,&a,0,1)==1&&a.Results[1].Status==XTP_STATUS_DEADLINE_EXCEEDED);
    assert(xtpSubmit(a.Session,&m,NULL,100,NULL)==XTP_OK);assert(transfer(&a,&b,0,1)==1);
    xtprequesttoken stale=b.RequestsHeld[1];uint64_t old=b.Binding;
    assert(xtpUnbind(b.Session,old,b.Now)==XTP_OK);bind_session(&b);assert(b.Binding>old);
    assert(xtpReply(b.Session,stale,&(xtpresponse){0},b.Now)==XTP_E_STATE);
    xtpreadprogress p;assert(xtpInput(b.Session,old,1,(xbytesview){NULL,0},false,b.Now,&p)==XTP_E_STATE);
    assert(xtpEnd(b.Session,old,1,b.Now)==XTP_E_STATE&&xtpUnbind(b.Session,old,b.Now)==XTP_E_STATE);
    a.Now=200;assert(xtpDrive(a.Session,200,&due)==XTP_OK&&a.Results[2].Knowledge==XTP_KNOW_UNKNOWN);
    assert(xtpDrive(a.Session,199,&due)==XTP_E_ARGUMENT);destroy(&a);destroy(&b);
}
static void callback_close_and_contract(void)
{
    for(size_t mode=0;mode<4;++mode){session_test a={0},b={0};create(&a,NULL);create(&b,NULL);bind_session(&a);bind_session(&b);
        b.CloseInMessage=mode==0;b.InvalidDecision=mode==1;b.AutoReply=mode>=2;
        if(mode==2)b.Decision=xtpPause();
        if(mode==3)a.CloseInComplete=true;
        xtpmessage m=message(XTP_REQUEST);assert(xtpSubmit(a.Session,&m,NULL,0,NULL)==XTP_OK);
        xtpoutput out;assert(xtpOutput(a.Session,0,&out)==XTP_OK);unsigned char wire[32];memcpy(wire,out.Data.Data,32);
        assert(xtpAdvance(a.Session,out.Ticket,32,0)==XTP_OK);xtpreadprogress p;
        xtpresult r=xtpInput(b.Session,b.Binding,1,(xbytesview){wire,32},false,0,&p);
        if(mode<3){assert(r==XTP_CLOSED&&info(&b).Closed);assert(b.Fatal==(mode?1u:0u));}
        else {assert(r==XTP_OK);xtpoutput response;assert(xtpOutput(b.Session,0,&response)==XTP_OK);
            unsigned char bytes[64];size_t n=response.Data.Size;memcpy(bytes,response.Data.Data,n);
            assert(xtpAdvance(b.Session,response.Ticket,n,0)==XTP_OK);
            assert(xtpInput(a.Session,a.Binding,1,(xbytesview){bytes,n},false,0,&p)==XTP_CLOSED&&a.Completed==1);}
        destroy(&a);destroy(&b);}
    session_test a={0},b={0};create(&a,NULL);create(&b,NULL);bind_session(&a);bind_session(&b);a.SubmitInComplete=true;
    xtpmessage m=message(XTP_NOTIFY);assert(xtpSubmit(a.Session,&m,NULL,0,NULL)==XTP_OK);
    assert(transfer(&a,&b,0,2)==2&&a.Completed==2&&b.Notifies==2);destroy(&a);destroy(&b);
}
typedef union test_memory {size_t Size;long double Align;} test_memory;
typedef struct allocator {size_t Calls,FailAt,Live,Bytes,Blocked;xtpsession* Probe;} allocator;
static void* test_realloc(void* user,void* memory,size_t n)
{
    allocator* a=user;test_memory* old=memory?(test_memory*)memory-1:NULL;
    if(a->Probe){assert(xtpClose(a->Probe,0)==XTP_E_STATE&&xtpDestroy(a->Probe)==XTP_E_STATE);++a->Blocked;}
    if(!n){if(old){--a->Live;a->Bytes-=old->Size;free(old);}return NULL;}
    ++a->Calls;if(a->Calls==a->FailAt)return NULL;size_t oldn=old?old->Size:0;
    test_memory* p=realloc(old,sizeof(*p)+n);if(!p)return NULL;p->Size=n;
    if(!memory)++a->Live;
    a->Bytes=a->Bytes-oldn+n;return p+1;
}
static void allocation_and_atomic_batch(void)
{
    for(size_t fail=1;fail<=6;++fail){allocator a={0};a.FailAt=fail;xtpconfig c;xtpConfigInit(&c);c.Realloc=test_realloc;c.AllocatorUser=&a;
        session_test t={0};xtpevents e={on_message,on_complete,on_fault};xtpsession* out=(xtpsession*)(uintptr_t)1;
        assert(xtpCreate(&c,&e,&t,&out)==XTP_E_MEMORY&&out==(xtpsession*)(uintptr_t)1&&!a.Live&&!a.Bytes);}
    for(size_t fail=1;fail<=3;++fail){allocator alloc={0};xtpconfig c;xtpConfigInit(&c);c.Realloc=test_realloc;c.AllocatorUser=&alloc;
        session_test a={0};create(&a,&c);size_t base=alloc.Bytes;alloc.FailAt=alloc.Calls+fail;
        xtplane lanes[]={{1,XTP_INPUT_STREAM,true,0},{2,XTP_INPUT_RECORD,false,1200},{3,XTP_INPUT_STREAM,true,0}};
        xtpbinding binding={lanes,3,true,false,NULL};uint64_t generation=999;
        assert(xtpBind(a.Session,&binding,0,&generation)==XTP_E_MEMORY&&generation==999);
        assert(alloc.Bytes==base&&!info(&a).Bound&&info(&a).BindingID==0);
        alloc.FailAt=0;bind_session(&a);destroy(&a);assert(!alloc.Live&&!alloc.Bytes);}
    for(size_t fail=1;fail<=4;++fail){allocator alloc={0};xtpconfig c;xtpConfigInit(&c);c.Realloc=test_realloc;c.AllocatorUser=&alloc;
        session_test a={0};create(&a,&c);bind_session(&a);size_t base=alloc.Bytes;alloc.FailAt=alloc.Calls+fail;
        alloc.Probe=a.Session;
        xtpmessage m[4];for(size_t i=0;i<4;++i)m[i]=message(XTP_REQUEST);xtpoperationtoken ops[4],original[4];
        memset(ops,0xaa,sizeof(ops));memcpy(original,ops,sizeof(ops));
        assert(xtpSubmitBatch(a.Session,m,4,NULL,0,ops)==XTP_E_MEMORY&&!memcmp(ops,original,sizeof(ops)));
        xtpinfo in=info(&a);assert(in.NextMessageID==1&&!in.Pending&&!in.TxFrames&&!in.Submitted&&alloc.Bytes==base);
        alloc.FailAt=0;m[3].Command.Kind=XTP_COMMAND_NONE;
        assert(xtpSubmitBatch(a.Session,m,4,NULL,0,ops)==XTP_E_VALUE&&!memcmp(ops,original,sizeof(ops)));
        destroy(&a);assert(!alloc.Live&&!alloc.Bytes&&alloc.Blocked);}
    allocator alloc={0};xtpconfig c;xtpConfigInit(&c);c.Realloc=test_realloc;c.AllocatorUser=&alloc;
    session_test a={0},b={0};create(&a,NULL);create(&b,&c);bind_session(&a);bind_session(&b);alloc.FailAt=alloc.Calls+1;
    xtpmessage m=message(XTP_REQUEST);assert(xtpSubmit(a.Session,&m,NULL,0,NULL)==XTP_OK);
    assert(transfer(&a,&b,0,1)==1&&b.Messages==0);assert(transfer(&b,&a,0,1)==1&&a.Results[0].Status==XTP_STATUS_BUSY);
    assert(info(&b).PeakReservedBytes<=c.MaxSessionBytes);destroy(&a);destroy(&b);assert(!alloc.Live);
}
static void limits_and_partial_timeout(void)
{
    session_test a={0};xtpconfig c;xtpConfigInit(&c);c.MaxPending=1;c.MaxTxFrames=4;c.ResponseReserveFrames=1;
    c.MaxTxBytes=128;c.MaxResponseBytes=32;c.ResponseReserveBytes=32;c.DefaultOperationTimeoutUs=10;
    create(&a,&c);bind_session(&a);xtpmessage m=message(XTP_REQUEST);assert(xtpSubmit(a.Session,&m,NULL,0,NULL)==XTP_OK);
    assert(xtpSubmit(a.Session,&m,NULL,0,NULL)==XTP_E_LIMIT);xtpoutput out;assert(xtpOutput(a.Session,0,&out)==XTP_OK);
    assert(xtpAdvance(a.Session,out.Ticket,1,0)==XTP_OK);a.Now=10;uint64_t due;
    assert(xtpDrive(a.Session,10,&due)==XTP_CLOSED&&a.Fatal==1&&a.Completed==1);
    assert(a.Results[0].Knowledge==XTP_KNOW_UNKNOWN&&a.Results[0].Reason==XTP_FINISH_TIMEOUT);destroy(&a);
    session_test b={0};create(&b,&c);bind_session(&b);m=message(XTP_NOTIFY);
    for(size_t i=0;i<3;++i)assert(xtpSubmit(b.Session,&m,NULL,0,NULL)==XTP_OK);
    assert(xtpSubmit(b.Session,&m,NULL,0,NULL)==XTP_E_LIMIT);
    unsigned char wire[32];encode_packet(wire,32,XTP_PUSH,1,0,false);feed(&b,wire,32,1,false,0);
    assert(b.Pushes==1&&info(&b).TxFrames==4&&info(&b).TxBytes==128);
    assert(xtpSubmit(b.Session,&m,NULL,0,NULL)==XTP_E_LIMIT);
    encode_packet(wire,32,XTP_PUSH,2,0,false);xtpreadprogress p;
    assert(xtpInput(b.Session,b.Binding,1,(xbytesview){wire,32},false,0,&p)==XTP_PAUSED&&p.Used==0&&b.Pushes==1);
    assert(xtpOutput(b.Session,0,&out)==XTP_OK&&xtpAdvance(b.Session,out.Ticket,out.Data.Size,0)==XTP_OK);
    assert(xtpResume(b.Session,b.Binding,1,0)==XTP_OK);feed(&b,wire,32,1,false,0);assert(b.Pushes==2);destroy(&b);
}
static void reserved_reply_and_state_lost(void)
{
    allocator alloc={0};xtpconfig c;xtpConfigInit(&c);c.Realloc=test_realloc;c.AllocatorUser=&alloc;c.MaxResponseBytes=64;
    session_test a={0},b={0};create(&a,NULL);create(&b,&c);bind_session(&a);bind_session(&b);
    xtpmessage m=message(XTP_REQUEST);assert(xtpSubmit(a.Session,&m,NULL,0,NULL)==XTP_OK);
    assert(transfer(&a,&b,0,1)==1);alloc.FailAt=alloc.Calls+1;size_t calls=alloc.Calls;
    unsigned char body[64]={0};xtpresponse response={0};response.Body=(xbytesview){body,sizeof(body)};
    assert(xtpReply(b.Session,b.RequestsHeld[0],&response,0)==XTP_E_LIMIT&&info(&b).IncomingRequests==1);
    response.Body=(xbytesview){NULL,0};response.Status=XTP_STATUS_RESULT_TOO_LARGE;
    assert(xtpReply(b.Session,b.RequestsHeld[0],&response,0)==XTP_OK&&alloc.Calls==calls);
    assert(transfer(&b,&a,0,1)==1&&a.Results[0].Status==XTP_STATUS_RESULT_TOO_LARGE);
    m=message(XTP_PUSH);assert(xtpSubmit(a.Session,&m,NULL,0,NULL)==XTP_OK);xtpoutput out;
    assert(xtpOutput(a.Session,0,&out)==XTP_OK&&xtpAdvance(a.Session,out.Ticket,out.Data.Size,0)==XTP_OK);
    unsigned char wire[32];encode_packet(wire,32,XTP_ACK,2,XTP_STATUS_STATE_LOST,false);feed(&a,wire,32,1,false,0);
    assert(a.Results[1].Knowledge==XTP_KNOW_UNKNOWN&&a.Results[1].HasRemoteStatus);
    destroy(&a);destroy(&b);assert(!alloc.Live);
}
static void bounded_close_and_stale_tickets(void)
{
    session_test a={0},b={0};xtpconfig c;xtpConfigInit(&c);c.Reader.MaxFrames=2;
    create(&a,&c);create(&b,&c);bind_session(&a);bind_session(&b);
    xtpmessage messages[8];for(size_t i=0;i<8;++i)messages[i]=message(i%2?XTP_NOTIFY:XTP_REQUEST);
    assert(xtpSubmitBatch(a.Session,messages,8,NULL,0,NULL)==XTP_OK);
    xtpoutput out;assert(xtpOutput(a.Session,0,&out)==XTP_OK);xtpoutputticket old=out.Ticket;
    assert(xtpUnbind(a.Session,a.Binding,0)==XTP_AGAIN&&a.Completed==2);
    assert(xtpAdvance(a.Session,old,32,0)==XTP_E_STATE);
    uint64_t due;size_t previous=a.Completed,turns=0;xtpresult r;
    do{r=xtpDrive(a.Session,0,&due);assert(a.Completed-previous<=2);previous=a.Completed;assert(++turns<10);}while(r==XTP_AGAIN);
    assert(r==XTP_OK&&a.Completed==8);bind_session(&a);
    xtpmessage m=message(XTP_NOTIFY);assert(xtpSubmit(a.Session,&m,NULL,0,NULL)==XTP_OK);
    assert(xtpAdvance(a.Session,old,32,0)==XTP_E_STATE);
    assert(transfer(&a,&b,0,1)==1&&a.Completed==9);destroy(&a);destroy(&b);
}
static void dedup_floor_and_unsent_result(void)
{
    session_test a={0};xtpconfig c;xtpConfigInit(&c);c.DedupWindow=4;create(&a,&c);bind_session(&a);
    xtpmessage m=message(XTP_REQUEST);assert(xtpSubmit(a.Session,&m,NULL,0,NULL)==XTP_OK);
    unsigned char wire[32];encode_packet(wire,32,XTP_RESPONSE,1,0,false);feed(&a,wire,32,1,false,0);
    assert(!a.Completed&&info(&a).UnknownResults==1);
    for(size_t i=1;i<=8;++i){encode_packet(wire,32,XTP_PUSH,i,0,false);feed(&a,wire,32,1,false,0);}
    assert(a.Pushes==8);encode_packet(wire,32,XTP_PUSH,1,0,false);feed(&a,wire,32,1,false,0);
    assert(a.Pushes==8&&info(&a).Duplicates==1);
    xtppacket p={0};p.Type=XTP_REQUEST;p.MessageID=9;p.Command=xtpCmdID(0);p.Replayable=true;size_t n;
    assert(xtpEncode(&p,1,NULL,wire,32,&n)==XTP_OK);feed(&a,wire,32,1,false,0);assert(!a.Requests);
    encode_packet(wire,32,XTP_PUSH,10,0,false);feed(&a,wire,32,2,true,0);
    assert(a.Pushes==8&&a.Faults==1);destroy(&a);
}
static void lane_rotation_and_callback_deadline(void)
{
    session_test a={0};create(&a,NULL);bind_session(&a);xtpmessage messages[4];xtpsubmitoptions options[4];
    memset(options,0,sizeof(options));for(size_t i=0;i<4;++i){messages[i]=message(XTP_NOTIFY);options[i].LaneID=i%2?2:1;}
    assert(xtpSubmitBatch(a.Session,messages,4,options,0,NULL)==XTP_OK);xtpoutput out;
    assert(xtpOutput(a.Session,0,&out)==XTP_OK&&out.LaneID==1);
    assert(xtpAdvance(a.Session,out.Ticket,1,0)==XTP_OK);
    assert(xtpOutput(a.Session,0,&out)==XTP_OK&&out.LaneID==2);
    assert(xtpAdvance(a.Session,out.Ticket,out.Data.Size,0)==XTP_OK);
    assert(xtpOutput(a.Session,0,&out)==XTP_OK&&out.LaneID==1&&out.Data.Size==31);
    assert(xtpAdvance(a.Session,out.Ticket,31,0)==XTP_OK&&a.Completed==2);destroy(&a);
    memset(&a,0,sizeof(a));create(&a,NULL);bind_session(&a);a.SubmitInComplete=true;
    xtpmessage m=message(XTP_NOTIFY);xtpsubmitoptions o={0};o.DeadlineUs=1;
    assert(xtpSubmit(a.Session,&m,&o,0,NULL)==XTP_OK);a.Now=1;uint64_t due;
    assert(xtpDrive(a.Session,1,&due)==XTP_OK&&due==5000001&&a.Completed==1&&info(&a).TxFrames==1);destroy(&a);
}
#ifdef XTP_IMPLEMENTATION_ONCE
static void exhausted_ids(void)
{
    session_test a={0};create(&a,NULL);bind_session(&a);a.Session->NextID=UINT64_MAX;
    xtpmessage m[2];m[0]=message(XTP_REQUEST);m[1]=message(XTP_PUSH);
    assert(xtpSubmitBatch(a.Session,m,2,NULL,0,NULL)==XTP_E_LIMIT&&info(&a).NextMessageID==UINT64_MAX);
    assert(xtpSubmit(a.Session,m,NULL,0,NULL)==XTP_OK&&info(&a).NextMessageID==0);
    assert(xtpSubmit(a.Session,m,NULL,0,NULL)==XTP_E_LIMIT);
    m[0]=message(XTP_NOTIFY);assert(xtpSubmit(a.Session,m,NULL,0,NULL)==XTP_OK);
    a.Session->NextSerial=UINT64_MAX;assert(xtpSubmit(a.Session,m,NULL,0,NULL)==XTP_OK);
    assert(xtpSubmit(a.Session,m,NULL,0,NULL)==XTP_E_LIMIT);destroy(&a);
}
#endif
int main(void)
{
    shapes_and_duplex();out_of_order_and_types();short_writes_and_records();pause_reject_and_duplicates();
    deadlines_and_rebind();callback_close_and_contract();allocation_and_atomic_batch();limits_and_partial_timeout();
    bounded_close_and_stale_tickets();dedup_floor_and_unsent_result();
    reserved_reply_and_state_lost();lane_rotation_and_callback_deadline();
#ifdef XTP_IMPLEMENTATION_ONCE
    exhausted_ids();
#endif
    puts("xtp session: duplex/shapes/order/ACK/short-write/records/pause/tokens/deadlines/OOM PASS");return 0;
}
