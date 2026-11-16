#define main xtp_batch_base_tests
#include "test_session.c"
#undef main

static void crossing_frames(bool record)
{
    session_test a={0},b={0};create(&a,NULL);create(&b,NULL);bind_session(&a);bind_session(&b);
    xtpmessage m[3];xtpsubmitoptions options[3]={{0}};
    for(size_t i=0;i<3;++i){m[i]=message(XTP_NOTIFY);options[i].Tag=(void*)(uintptr_t)(i+1);options[i].LaneID=record?3:1;}
    assert(xtpSubmitBatch(a.Session,m,3,options,0,NULL)==XTP_OK);xtpoutput out,again;
    assert(xtpOutput(a.Session,0,&out)==XTP_OK&&out.Frames==3&&out.Data.Size==96);
    assert(xtpOutput(a.Session,0,&again)==XTP_OK&&!memcmp(&out.Ticket,&again.Ticket,sizeof(out.Ticket)));
    unsigned char original[96],wire[96];memcpy(original,out.Data.Data,96);xtpoutputticket old=out.Ticket;
    assert(xtpAdvance(a.Session,old,0,0)==XTP_AGAIN&&!a.Completed);
    if(record){assert(xtpAdvance(a.Session,old,33,0)==XTP_E_ARGUMENT&&!a.Completed);
        assert(xtpAdvance(a.Session,old,96,0)==XTP_OK);memcpy(wire,original,96);}
    else{memcpy(wire,out.Data.Data,33);assert(xtpAdvance(a.Session,old,33,0)==XTP_OK&&a.Completed==1);
        assert(xtpAdvance(a.Session,old,1,0)==XTP_E_ARGUMENT);
        assert(xtpOutput(a.Session,1,&out)==XTP_OK&&out.Frames==2&&out.Data.Size==63);
        memcpy(wire+33,out.Data.Data,63);assert(xtpAdvance(a.Session,out.Ticket,63,0)==XTP_OK);}
    assert(!memcmp(wire,original,96)&&a.Completed==3);
    feed(&b,wire,96,record?3:1,record,1);assert(b.Notifies==3);
    for(size_t i=0;i<3;++i)assert(a.Results[i].Tag==(void*)(uintptr_t)(i+1));
    assert(info(&a).BatchCopiedBytes>=96);destroy(&a);destroy(&b);
}
static void callbacks_during_batch(void)
{
    for(size_t mode=0;mode<2;++mode){session_test a={0};create(&a,NULL);bind_session(&a);
        xtpmessage m[3]={message(XTP_NOTIFY),message(XTP_NOTIFY),message(XTP_NOTIFY)};
        assert(xtpSubmitBatch(a.Session,m,3,NULL,0,NULL)==XTP_OK);a.SubmitInComplete=mode==0;a.CloseInComplete=mode==1;
        xtpoutput out;assert(xtpOutput(a.Session,1,&out)==XTP_OK&&out.Frames==3);
        xtpresult r=xtpAdvance(a.Session,out.Ticket,out.Data.Size,0);assert(a.Completed==3);
        if(mode){assert(r==XTP_CLOSED&&info(&a).Closed);}else{
            assert(r==XTP_OK&&info(&a).TxFrames==1&&xtpOutput(a.Session,1,&out)==XTP_OK&&out.Frames==1);
            assert(xtpAdvance(a.Session,out.Ticket,out.Data.Size,0)==XTP_OK&&a.Completed==4);}
        destroy(&a);}
}
static void allocation_fallback(void)
{
    allocator alloc={0};xtpconfig c;xtpConfigInit(&c);c.Realloc=test_realloc;c.AllocatorUser=&alloc;session_test a={0};
    create(&a,&c);bind_session(&a);xtpmessage m[3]={message(XTP_NOTIFY),message(XTP_NOTIFY),message(XTP_NOTIFY)};
    assert(xtpSubmitBatch(a.Session,m,3,NULL,0,NULL)==XTP_OK);alloc.FailAt=alloc.Calls+1;alloc.Probe=a.Session;
    xtpoutput raw,batch;assert(xtpOutput(a.Session,1,&raw)==XTP_OK&&raw.Frames==1&&raw.Data.Size==32);
    assert(!info(&a).Batches&&info(&a).TxFrames==3);alloc.FailAt=0;
    assert(xtpOutput(a.Session,1,&batch)==XTP_OK&&batch.Frames==3);
    /* An older direct-frame offer may still be used; the aggregate must then
     * fail validation instead of replaying that freed/reused slot. */
    assert(xtpAdvance(a.Session,raw.Ticket,32,0)==XTP_OK&&a.Completed==1);
    assert(xtpAdvance(a.Session,batch.Ticket,96,0)==XTP_E_ARGUMENT);
    assert(xtpOutput(a.Session,1,&batch)==XTP_OK&&batch.Frames==2);
    assert(xtpAdvance(a.Session,batch.Ticket,64,0)==XTP_OK&&a.Completed==3);
    destroy(&a);assert(!alloc.Live&&!alloc.Bytes&&alloc.Blocked);
}
static void lanes_limits_and_unbind(void)
{
    session_test a={0};xtpconfig c;xtpConfigInit(&c);c.MaxResponseBytes=32;create(&a,&c);
    xtplane lanes[]={{1,XTP_INPUT_STREAM,true,0},{2,XTP_INPUT_RECORD,false,1200},{3,XTP_INPUT_RECORD,true,64}};
    xtpbinding binding={lanes,3,true,false,NULL};assert(xtpBind(a.Session,&binding,0,&a.Binding)==XTP_OK);
    xtpmessage m[3]={message(XTP_NOTIFY),message(XTP_NOTIFY),message(XTP_NOTIFY)};xtpsubmitoptions op[3]={{0}};
    for(size_t i=0;i<3;++i)op[i].LaneID=3;
    assert(xtpSubmitBatch(a.Session,m,3,op,0,NULL)==XTP_OK);xtpoutput out;
    assert(xtpOutput(a.Session,3,&out)==XTP_OK&&out.Data.Size==64&&out.Frames==2);
    xtpoutputticket old=out.Ticket;assert(xtpAdvance(a.Session,old,64,0)==XTP_OK);
    for(size_t i=0;i<3;++i)op[i].LaneID=2;
    assert(xtpSubmitBatch(a.Session,m,3,op,0,NULL)==XTP_OK);
    assert(xtpOutput(a.Session,2,&out)==XTP_OK&&out.Data.Size==32&&out.Frames==1);
    assert(xtpUnbind(a.Session,a.Binding,0)==XTP_OK);assert(xtpAdvance(a.Session,old,1,0)==XTP_E_STATE);destroy(&a);
    memset(&a,0,sizeof(a));xtpConfigInit(&c);c.BatchBytes=0;create(&a,&c);bind_session(&a);
    assert(xtpSubmitBatch(a.Session,m,3,NULL,0,NULL)==XTP_OK);
    assert(xtpOutput(a.Session,1,&out)==XTP_OK&&out.Frames==1&&out.Data.Size==32&&!info(&a).Batches);destroy(&a);
}
int main(void)
{
    assert(xtp_batch_base_tests()==0);crossing_frames(false);crossing_frames(true);
    callbacks_during_batch();allocation_fallback();lanes_limits_and_unbind();
    puts("xtp batch: stream/record, partial prefixes, tickets, callback reuse/close, OOM and limits passed");return 0;
}
