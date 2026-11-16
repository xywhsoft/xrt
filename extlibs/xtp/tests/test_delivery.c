#define main xtp_base_session_tests
#include "test_session.c"
#undef main

static xtpconfig delivery_config(void)
{
    xtpconfig c;xtpConfigInit(&c);xtpDeliveryConfigInit(&c.Delivery);c.Delivery.AllowRequests=true;
    c.MaxResponseBytes=256;c.DefaultOperationTimeoutUs=800;c.IncomingRequestTimeoutUs=400;
    c.Delivery.MaxLifetimeUs=1000;c.Delivery.RetryMinUs=50;c.Delivery.RetryMaxUs=200;return c;
}
static xtpdeliveryproof proof(bool peer,bool resume)
{
    xtpdeliveryproof p={0};p.Identity.SessionID[0]=1;p.Identity.Domain[0]=2;
    p.Identity.Local[0]=peer?4:3;p.Identity.Peer[0]=peer?3:4;
    p.UntilUs=1000000;p.LifetimeUs=1000;p.PeerReceiveFloor=p.PeerResultFloor=1;
    p.Resume=resume;p.StateRetained=true;p.OldBindingDetached=resume;p.AllowRequests=true;return p;
}
static xtpresult delivery_bind(session_test* t,bool peer,bool resume,xtpdeliveryproof* custom)
{
    xtplane lanes[]={{1,XTP_INPUT_STREAM,true,0},{2,XTP_INPUT_RECORD,false,1200},{3,XTP_INPUT_RECORD,true,1048576}};
    xtpdeliveryproof p=custom?*custom:proof(peer,resume);xtpbinding b={lanes,3,true,false,&p};
    return xtpBind(t->Session,&b,t->Now,&t->Binding);
}
static void pair_create(session_test* a,session_test* b,xtpconfig* c)
{create(a,c);create(b,c);assert(delivery_bind(a,false,false,NULL)==XTP_OK);assert(delivery_bind(b,true,false,NULL)==XTP_OK);}
static void suspend(session_test* a)
{
    xtpresult r=xtpUnbind(a->Session,a->Binding,a->Now);uint64_t due;
    while(r==XTP_AGAIN)r=xtpDrive(a->Session,a->Now,&due);
    assert(r==XTP_OK&&info(a).Suspended&&!info(a).Bound);
}
static void reconnect(session_test* a,session_test* b)
{suspend(a);suspend(b);assert(delivery_bind(a,false,true,NULL)==XTP_OK);assert(delivery_bind(b,true,true,NULL)==XTP_OK);}
static size_t take_wire(session_test* a,unsigned char* bytes,size_t cap)
{
    xtpoutput o;assert(xtpOutput(a->Session,0,&o)==XTP_OK&&o.Data.Size<=cap);size_t n=o.Data.Size;
    memcpy(bytes,o.Data.Data,n);assert(xtpAdvance(a->Session,o.Ticket,n,a->Now)==XTP_OK);return n;
}
static void drive_at(session_test* a,uint64_t now)
{a->Now=now;uint64_t due;assert(xtpDrive(a->Session,now,&due)>=0);}
static void push_ack_loss(void)
{
    xtpconfig c=delivery_config();session_test a={0},b={0};pair_create(&a,&b,&c);
    xtpmessage m=message(XTP_PUSH);xtpsubmitoptions op={0};op.Replay=true;unsigned char first[64],ack[64];
    assert(xtpSubmit(a.Session,&m,&op,0,NULL)==XTP_OK);size_t n=take_wire(&a,first,sizeof(first));feed(&b,first,n,1,false,0);
    take_wire(&b,ack,sizeof(ack));assert(!a.Completed&&b.Pushes==1&&info(&a).Retained==1);
    drive_at(&a,100);xtpoutput o;assert(xtpOutput(a.Session,0,&o)==XTP_AGAIN);b.Now=100;reconnect(&a,&b);
    assert(xtpOutput(a.Session,0,&o)==XTP_OK&&o.Data.Size==n&&!memcmp(o.Data.Data,first,n));
    assert(transfer(&a,&b,1,1)==1&&b.Pushes==1);assert(transfer(&b,&a,0,1)==1&&a.Completed==1);
    assert(a.Results[0].Knowledge==XTP_KNOW_ACCEPTED&&!info(&a).Retained&&info(&a).Attempts==2);destroy(&a);destroy(&b);
}
static void response_and_receipt_loss(void)
{
    xtpconfig c=delivery_config();session_test a={0},b={0};b.AutoReply=true;pair_create(&a,&b,&c);
    xtpmessage m=message(XTP_REQUEST);xtpsubmitoptions op={0};op.Replay=true;unsigned char wire[512];
    assert(xtpSubmit(a.Session,&m,&op,0,NULL)==XTP_OK);assert(transfer(&a,&b,0,1)==1);
    take_wire(&b,wire,sizeof(wire));assert(b.Requests==1&&info(&b).Retained==1);reconnect(&a,&b);
    assert(transfer(&b,&a,1,1)==1&&a.Completed==1&&!info(&a).Retained&&info(&a).Receipts==1);
    take_wire(&a,wire,sizeof(wire));assert(info(&b).Retained==1);reconnect(&a,&b);
    assert(transfer(&b,&a,0,1)==1&&a.Completed==1);assert(transfer(&a,&b,0,1)==1&&!info(&b).Retained);
    assert(b.Requests==1&&info(&a).Duplicates==1);drive_at(&a,1000);assert(info(&a).Receipts==1&&info(&a).ResultFloor==1);
    destroy(&a);b.Now=1000;destroy(&b);
}
static void async_and_failed_identity(void)
{
    xtpconfig c=delivery_config();session_test a={0},b={0};pair_create(&a,&b,&c);
    xtpmessage m=message(XTP_REQUEST);xtpsubmitoptions op={0};op.Replay=true;
    assert(xtpSubmit(a.Session,&m,&op,0,NULL)==XTP_OK);assert(transfer(&a,&b,0,1)==1);suspend(&a);suspend(&b);
    xtpresponse r={0};r.Body=XRT_BYTES_LITERAL("async");assert(xtpReply(b.Session,b.RequestsHeld[0],&r,1)==XTP_OK);
    a.Now=b.Now=1;assert(delivery_bind(&a,false,true,NULL)==XTP_OK);assert(delivery_bind(&b,true,true,NULL)==XTP_OK);
    assert(transfer(&b,&a,0,1)==1&&a.Completed==1);assert(transfer(&a,&b,0,1)==1);destroy(&a);destroy(&b);
    memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));pair_create(&a,&b,&c);m=message(XTP_PUSH);
    assert(xtpSubmit(a.Session,&m,&op,0,NULL)==XTP_OK);unsigned char wire[64];take_wire(&a,wire,sizeof(wire));suspend(&a);
    xtpdeliveryproof bad=proof(false,true);bad.Identity.Peer[1]=1;assert(delivery_bind(&a,false,true,&bad)==XTP_E_STATE);
    drive_at(&a,1);assert(info(&a).Closed&&a.Completed==1&&a.Results[0].Knowledge==XTP_KNOW_UNKNOWN);destroy(&a);destroy(&b);
}
static void busy_deadlines_and_tickets(void)
{
    xtpconfig c=delivery_config();c.Delivery.MaxAttempts=2;session_test a={0},b={0};b.Decision=xtpBusy();pair_create(&a,&b,&c);
    xtpmessage m=message(XTP_REQUEST);xtpsubmitoptions op={0};op.Replay=true;
    assert(xtpSubmit(a.Session,&m,&op,0,NULL)==XTP_OK);xtpoutput old,o;assert(xtpOutput(a.Session,0,&old)==XTP_OK);
    unsigned char original[64];memcpy(original,old.Data.Data,old.Data.Size);assert(transfer(&a,&b,0,1)==1&&transfer(&b,&a,0,1)==1);
    assert(!a.Completed&&info(&a).Retained==1&&!info(&b).Retained);assert(xtpOutput(a.Session,0,&o)==XTP_AGAIN);
    assert(xtpAdvance(a.Session,old.Ticket,1,0)==XTP_E_ARGUMENT);drive_at(&a,200);b.Now=200;
    assert(xtpOutput(a.Session,0,&o)==XTP_OK&&!memcmp(o.Data.Data,original,o.Data.Size));
    assert(xtpAdvance(a.Session,old.Ticket,1,200)==XTP_E_ARGUMENT);
    assert(transfer(&a,&b,0,1)==1&&transfer(&b,&a,0,1)==1);drive_at(&a,500);
    assert(a.Completed==1&&a.Results[0].LocalResult==XTP_E_LIMIT&&a.Results[0].Knowledge==XTP_KNOW_REJECTED);
    assert(b.Requests==2&&!info(&a).Retained&&!info(&a).Receipts);destroy(&a);b.Now=500;destroy(&b);
}
static void partial_resend_completion(void)
{
    xtpconfig c=delivery_config();session_test a={0},b={0};b.AutoReply=true;pair_create(&a,&b,&c);
    xtpmessage m=message(XTP_REQUEST);m.Body=XRT_BYTES_LITERAL("long-request");xtpsubmitoptions op={0};op.Replay=true;
    assert(xtpSubmit(a.Session,&m,&op,0,NULL)==XTP_OK);assert(transfer(&a,&b,0,1)==1);unsigned char lost[512];take_wire(&b,lost,sizeof(lost));reconnect(&a,&b);
    xtpoutput o;assert(xtpOutput(a.Session,0,&o)==XTP_OK);unsigned char prefix[7];memcpy(prefix,o.Data.Data,7);
    assert(xtpAdvance(a.Session,o.Ticket,7,0)==XTP_OK);feed(&b,prefix,7,1,false,0);
    assert(transfer(&b,&a,0,1)==1&&a.Completed==1);assert(info(&a).Retained==1&&info(&a).TxFrames==2);
    assert(xtpOutput(a.Session,0,&o)==XTP_OK&&o.Data.Size==37);assert(transfer(&a,&b,0,10)==2);
    assert(!info(&a).Retained&&b.Requests==1&&!info(&b).Retained);destroy(&a);destroy(&b);
}
static void partial_cache_receipt(void)
{
    xtpconfig c=delivery_config();session_test a={0},b={0};b.AutoReply=true;pair_create(&a,&b,&c);
    xtpmessage m=message(XTP_REQUEST);xtpsubmitoptions op={0};op.Replay=true;unsigned char ack[64];
    assert(xtpSubmit(a.Session,&m,&op,0,NULL)==XTP_OK);assert(transfer(&a,&b,0,1)==1&&transfer(&b,&a,0,1)==1);
    size_t n=take_wire(&a,ack,sizeof(ack));reconnect(&a,&b);xtpoutput o;assert(xtpOutput(b.Session,0,&o)==XTP_OK);
    unsigned char prefix[3];memcpy(prefix,o.Data.Data,3);assert(xtpAdvance(b.Session,o.Ticket,3,0)==XTP_OK);feed(&a,prefix,3,1,false,0);
    feed(&b,ack,n,1,false,0);assert(info(&b).Retained==1);assert(transfer(&b,&a,0,1)==1&&!info(&b).Retained&&a.Completed==1);
    assert(transfer(&a,&b,0,1)==1);destroy(&a);destroy(&b);
}
static void bounded_receipts_and_replay(void)
{
    xtpconfig c=delivery_config();c.DedupWindow=2;c.Delivery.MaxReplayMessages=2;session_test a={0},b={0};b.AutoReply=true;pair_create(&a,&b,&c);
    xtpmessage m[3]={message(XTP_REQUEST),message(XTP_REQUEST),message(XTP_REQUEST)};xtpsubmitoptions op[3]={{0}};
    for(size_t i=0;i<3;++i)op[i].Replay=true;
    xtpinfo before=info(&a);assert(xtpSubmitBatch(a.Session,m,3,op,0,NULL)==XTP_E_LIMIT);assert(info(&a).NextMessageID==before.NextMessageID);
    assert(xtpSubmitBatch(a.Session,m,2,op,0,NULL)==XTP_OK);assert(transfer(&a,&b,0,3)==2&&transfer(&b,&a,0,3)==2&&transfer(&a,&b,0,3)==2);
    assert(!info(&a).Retained&&info(&a).Receipts==2);
    for(size_t i=0;i<16;++i){assert(xtpSubmit(a.Session,m,op,0,NULL)==XTP_OK);
        assert(transfer(&a,&b,0,1)==1&&transfer(&b,&a,0,1)==1&&transfer(&a,&b,0,1)==1);}
    assert(a.Completed==18&&info(&a).Receipts==2&&info(&a).ResultFloor==17);destroy(&a);destroy(&b);
}
static void delivery_oom(void)
{
    xtpevents e={on_message,on_complete,on_fault};
    for(size_t fail=1;fail<=7;++fail){allocator alloc={0};alloc.FailAt=fail;xtpconfig c=delivery_config();
        c.Realloc=test_realloc;c.AllocatorUser=&alloc;session_test t={0};xtpsession* out=(xtpsession*)(uintptr_t)1;
        assert(xtpCreate(&c,&e,&t,&out)==XTP_E_MEMORY&&out==(xtpsession*)(uintptr_t)1&&!alloc.Live&&!alloc.Bytes);}
    for(size_t fail=1;fail<=4;++fail){allocator alloc={0};xtpconfig c=delivery_config();c.Realloc=test_realloc;c.AllocatorUser=&alloc;
        session_test a={0};create(&a,&c);assert(delivery_bind(&a,false,false,NULL)==XTP_OK);size_t base=alloc.Bytes;
        xtpmessage m[4];xtpsubmitoptions op[4]={{0}};for(size_t i=0;i<4;++i){m[i]=message(XTP_REQUEST);op[i].Replay=true;}
        alloc.FailAt=alloc.Calls+fail;alloc.Probe=a.Session;
        assert(xtpSubmitBatch(a.Session,m,4,op,0,NULL)==XTP_E_MEMORY);xtpinfo in=info(&a);
        assert(!in.Receipts&&!in.Retained&&!in.Pending&&in.NextMessageID==1&&alloc.Bytes==base);
        destroy(&a);assert(!alloc.Live&&!alloc.Bytes&&alloc.Blocked);}
    for(size_t fail=1;fail<=3;++fail){allocator alloc={0};xtpconfig c=delivery_config();c.Realloc=test_realloc;c.AllocatorUser=&alloc;
        session_test a={0};create(&a,&c);assert(delivery_bind(&a,false,false,NULL)==XTP_OK);
        xtpmessage m=message(XTP_PUSH);xtpsubmitoptions op={0};op.Replay=true;assert(xtpSubmit(a.Session,&m,&op,0,NULL)==XTP_OK);
        suspend(&a);size_t base=alloc.Bytes;alloc.FailAt=alloc.Calls+fail;
        assert(delivery_bind(&a,false,true,NULL)==XTP_E_MEMORY&&alloc.Bytes==base&&info(&a).Retained==1&&info(&a).Suspended);
        alloc.FailAt=0;assert(delivery_bind(&a,false,true,NULL)==XTP_OK);destroy(&a);assert(!alloc.Live&&!alloc.Bytes);}
}
static void expiry_proofs_and_window(void)
{
    xtpconfig c=delivery_config();c.DedupWindow=2;session_test a={0},b={0};pair_create(&a,&b,&c);
    xtpmessage request=message(XTP_REQUEST),push=message(XTP_PUSH);xtpsubmitoptions op={0};op.Replay=true;
    op.DeadlineUs=1001;assert(xtpSubmit(a.Session,&request,&op,0,NULL)==XTP_E_LIMIT);op.DeadlineUs=0;
    assert(xtpSubmit(a.Session,&request,&op,0,NULL)==XTP_OK);assert(transfer(&a,&b,0,1)==1);
    /* Exact active request state survives the compact receive history floor. */
    for(size_t i=0;i<4;++i){assert(xtpSubmit(a.Session,&push,NULL,0,NULL)==XTP_OK);assert(transfer(&a,&b,0,1)==1&&transfer(&b,&a,0,1)==1);}
    xtprequesttoken held=b.RequestsHeld[0];suspend(&a);suspend(&b);xtpdeliveryproof p=proof(false,true);p.PeerReceiveFloor=info(&b).ReceiveFloor;
    assert(delivery_bind(&a,false,true,&p)==XTP_OK);assert(delivery_bind(&b,true,true,NULL)==XTP_OK);
    assert(transfer(&a,&b,0,1)==1&&b.Requests==1);drive_at(&b,400);
    assert(xtpReply(b.Session,held,&(xtpresponse){0},400)==XTP_E_STATE);a.Now=400;
    assert(transfer(&b,&a,0,1)==1&&a.Results[a.Completed-1].Status==XTP_STATUS_DEADLINE_EXCEEDED);destroy(&a);destroy(&b);
    memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));pair_create(&a,&b,&c);
    assert(xtpSubmit(a.Session,&push,&op,0,NULL)==XTP_OK);unsigned char wire[64];take_wire(&a,wire,sizeof(wire));suspend(&a);
    drive_at(&a,800);assert(a.Completed==1&&a.Results[0].Knowledge==XTP_KNOW_UNKNOWN&&!info(&a).Closed&&!info(&a).Retained);
    p=proof(false,true);p.StateRetained=false;assert(delivery_bind(&a,false,true,&p)==XTP_E_STATE);drive_at(&a,801);assert(info(&a).Closed);
    destroy(&a);destroy(&b);
    memset(&a,0,sizeof(a));create(&a,&c);p=proof(false,false);p.UntilUs=10;assert(delivery_bind(&a,false,false,&p)==XTP_OK);
    op.DeadlineUs=9;assert(xtpSubmit(a.Session,&push,&op,0,NULL)==XTP_OK);drive_at(&a,10);assert(info(&a).Closed);destroy(&a);
}
static void retired_and_unconsumed_results(void)
{
    xtpconfig c=delivery_config();c.DedupWindow=2;session_test a={0},b={0};b.AutoReply=true;pair_create(&a,&b,&c);
    xtpmessage m=message(XTP_REQUEST);xtpsubmitoptions op={0};op.Replay=true;unsigned char request[64],result[512],receipt[64];
    assert(xtpSubmit(a.Session,&m,&op,0,NULL)==XTP_OK);size_t q=take_wire(&a,request,sizeof(request));feed(&b,request,q,1,false,0);
    size_t n=take_wire(&b,result,sizeof(result));feed(&a,result,n,1,false,0);take_wire(&a,receipt,sizeof(receipt)); /* lost */
    for(size_t i=0;i<3;++i){assert(xtpSubmit(a.Session,&m,&op,0,NULL)==XTP_OK);
        assert(transfer(&a,&b,0,1)==1&&transfer(&b,&a,0,1)==1&&transfer(&a,&b,0,1)==1);}
    assert(info(&a).ResultFloor==3&&info(&b).Retained==1);
    feed(&b,request,q,1,false,0);assert(transfer(&b,&a,0,1)==1&&a.Completed==4&&b.Requests==4);
    n=take_wire(&a,receipt,sizeof(receipt));xtpframeview f;
    assert(xtpDecode((xbytesview){receipt,n},NULL,&f)==XTP_OK&&f.ResultAck&&f.Status==XTP_STATUS_STATE_LOST);
    feed(&b,receipt,n,1,false,0);assert(!info(&b).Retained);destroy(&a);destroy(&b);

    memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));b.AutoReply=true;pair_create(&a,&b,&c);
    op.DeadlineUs=10;assert(xtpSubmit(a.Session,&m,&op,0,NULL)==XTP_OK);assert(transfer(&a,&b,0,1)==1);
    n=take_wire(&b,result,sizeof(result));drive_at(&a,10);b.Now=10;assert(a.Results[0].Knowledge==XTP_KNOW_UNKNOWN);
    feed(&a,result,n,1,false,0);n=take_wire(&a,receipt,sizeof(receipt));
    assert(xtpDecode((xbytesview){receipt,n},NULL,&f)==XTP_OK&&f.ResultAck&&f.Status==XTP_STATUS_STATE_LOST&&a.Completed==1);
    feed(&b,receipt,n,1,false,0);assert(!info(&b).Retained);
    encode_packet(receipt,32,XTP_RESPONSE,999,0,false);feed(&a,receipt,32,1,false,0);xtpoutput out;assert(xtpOutput(a.Session,0,&out)==XTP_AGAIN);
    destroy(&a);destroy(&b);
}
static void pending_below_rolling_floor(void)
{
    xtpconfig c=delivery_config();c.DedupWindow=2;session_test a={0},b={0};pair_create(&a,&b,&c);
    xtpmessage m=message(XTP_REQUEST);xtpsubmitoptions op={0};op.Replay=true;
    assert(xtpSubmit(a.Session,&m,&op,0,NULL)==XTP_OK&&transfer(&a,&b,0,1)==1);xtprequesttoken held=b.RequestsHeld[0];b.AutoReply=true;
    for(size_t i=0;i<4;++i){assert(xtpSubmit(a.Session,&m,&op,0,NULL)==XTP_OK);
        assert(transfer(&a,&b,0,1)==1&&transfer(&b,&a,0,1)==1&&transfer(&a,&b,0,1)==1);}
    assert(info(&a).Pending==1&&info(&a).ResultFloor==1);reconnect(&a,&b);
    assert(xtpReply(b.Session,held,&(xtpresponse){0},0)==XTP_OK);assert(transfer(&b,&a,0,1)==1&&a.Completed==5);
    assert(transfer(&a,&b,0,1)==1&&b.Requests==5&&info(&a).ResultFloor==4);destroy(&a);destroy(&b);
}
static xtpdecision rolling_message(xtpsession* s,const xtpmessageview* m,void* user)
{session_test* t=user;++t->Requests;assert(xtpReply(s,m->Request,&(xtpresponse){0},0)==XTP_OK);return xtpAccept();}
static void rolling_complete(xtpsession* s,const xtpcompletion* c,void* user)
{session_test* t=user;(void)s;assert(c->Reason==XTP_FINISH_RESPONSE&&c->Knowledge==XTP_KNOW_REPLIED&&c->MessageID==++t->Completed);}
static void sustained_replay_requests(void)
{
    xtpconfig c=delivery_config();session_test a={0},b={0};xtpevents e={rolling_message,rolling_complete,on_fault};
    assert(xtpCreate(&c,&e,&a,&a.Session)==XTP_OK&&xtpCreate(&c,&e,&b,&b.Session)==XTP_OK);
    assert(delivery_bind(&a,false,false,NULL)==XTP_OK&&delivery_bind(&b,true,false,NULL)==XTP_OK);
    xtpmessage m=message(XTP_REQUEST);xtpsubmitoptions op={0};op.Replay=true;
    for(size_t i=0;i<12000;++i){assert(xtpSubmit(a.Session,&m,&op,0,NULL)==XTP_OK);
        assert(transfer(&a,&b,0,1)==1&&transfer(&b,&a,0,1)==1&&transfer(&a,&b,0,1)==1);}
    assert(a.Completed==12000&&b.Requests==12000&&info(&a).Receipts==1024&&!info(&a).Retained&&!info(&b).Retained);
    destroy(&a);destroy(&b);
}
int main(void)
{
    assert(xtp_base_session_tests()==0);push_ack_loss();response_and_receipt_loss();async_and_failed_identity();
    busy_deadlines_and_tickets();partial_resend_completion();partial_cache_receipt();bounded_receipts_and_replay();
    delivery_oom();expiry_proofs_and_window();
    retired_and_unconsumed_results();pending_below_rolling_floor();sustained_replay_requests();
    puts("xtp delivery: resume, lost ACK/result/receipt, async token, BUSY, bounds and partial lifetimes passed");return 0;
}
