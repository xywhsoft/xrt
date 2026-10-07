#include "../test.h"

#define LIST_ID UINT64_C(0x721001)
#define SET_ID UINT64_C(0x721002)
#define CLASS_ID UINT64_C(0x721003)
static xstrview view(const char* s) { return (xstrview){s,strlen(s)}; }
static xmemdebugsnapshot live(void)
{ xmemdebugsnapshot s; xrtMemDebugSnapshot(&s); return s; }
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after=live();
    testRequire(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes,
        "nominal sequence projection leaked");
}
static void error(xerrkind kind)
{
    testRequire(xrtGetError() && xrtErrorKind(xrtGetError())==kind,"sequence error contract");
    xrtClearError();
}
static int project(uint64 identity,xvaluegraphnextv1* next,xvalue** output,ptr user)
{
    (void)user;
    if(identity!=CLASS_ID) return 0;
    xvalue* child=xrtValueGraphObjectGetV1(next,view("edge"));
    xvalue* copy=child ? xrtValueGraphNextV1(next,child) : NULL;
    if(!copy) return -1;
    *output=xrtValueObject();
    bool ok=*output && xrtValueObjectSetTake(*output,view("wire"),&copy);
    xrtValueRelease(copy); return ok ? 1 : -1;
}
static xvalue* list(xvalue* edge,uint64 identity)
{
    xvalue* value=xrtValueIntMap();
    testRequire(value && xrtValueIntMapSet(value,-9,edge) &&
        xrtValueIntMapSet(value,100,edge) &&
        (!identity || xrtValueTypeIdBind(value,identity)),"List fixture");
    return value;
}
static void requireProjection(xvalue* result,xvalue* source)
{
    testRequire(result && xrtValueType(result)==XVALUE_ARRAY && xrtValueCount(result)==74,
        "wide sequence result");
    xvalue* projected=xrtValueArrayAt(result,0),*shared=xrtValueArrayAt(result,72);
    testRequire(xrtValueType(projected)==XVALUE_ARRAY && xrtValueCount(projected)==2 &&
        xrtValueArrayAt(projected,0)==shared && xrtValueArrayAt(projected,1)==shared &&
        xrtValueObjectGet(shared,view("wire"))==xrtValueArrayAt(result,71) &&
        xrtValueArrayAt(result,70)==projected && !xrtValueTypeId(projected),
        "List projection did not share the object/sequence memo");
    xvalue* set=xrtValueArrayAt(result,73);
    testRequire(xrtValueType(set)==XVALUE_ARRAY && xrtValueCount(set)==2 &&
        !xrtValueTypeId(set),"Set was not projected to a data Array");
    testRequire(xrtValueType(xrtValueArrayAt(source,0))==XVALUE_INT_MAP &&
        xrtValueTypeId(xrtValueArrayAt(source,0))==LIST_ID,"source List changed");
}
int main(void)
{
    testRequire(xrtMemDebugEnable(true),"sequence logical allocation tracking");
    xmemdebugsnapshot initial=live();
    xvaluegraphsequencev1 sequences[]={{LIST_ID,XVALUE_INT_MAP},{SET_ID,XVALUE_SET}};
    xvaluegraphcopyv1 config={.Size=sizeof(config),.Flags=XVALUE_GRAPH_COPY_DATA_V1,
        .CopyObject=project,.Sequences=sequences,.SequenceCount=2};
    xvalue* edge=xrtValueArray(),*object=xrtValueObject(),*root=xrtValueArray(),*set=xrtValueSet();
    testRequire(edge && object && root && set &&
        xrtValueArrayAppendNew(edge,xrtValueString((xstrview){"a\0b",3})) &&
        xrtValueObjectSet(object,view("edge"),edge) && xrtValueTypeIdBind(object,CLASS_ID) &&
        xrtValueSetAddNew(set,xrtValueInt(1)) && xrtValueSetAddNew(set,xrtValueInt(2)) &&
        xrtValueTypeIdBind(set,SET_ID),"mixed fixture");
    for(size_t i=0;i<70;++i) {
        xvalue* item=list(object,LIST_ID);
        testRequire(xrtValueArrayAppendTake(root,&item),"wide sequence fixture");
    }
    testRequire(xrtValueArrayAppend(root,xrtValueArrayAt(root,0)) &&
        xrtValueArrayAppend(root,edge) && xrtValueArrayAppend(root,object) &&
        xrtValueArrayAppend(root,set),"memo aliases fixture");
    xrtSetErrorKind(XERR_IO); const xerror* incoming=xrtGetError();
    xvalue* result=xrtValueGraphCopyV1(root,&config);
    testRequire(xrtGetError()==incoming,"successful sequence copy lost incoming error");
    requireProjection(result,root); xrtValueRelease(result); xrtClearError();

    xvalue* plain=list(edge,0),*unlisted=list(edge,LIST_ID+100),*plainSet=xrtValueSet();
    testRequire(plainSet && xrtValueSetAddNew(plainSet,xrtValueInt(4)),"ordinary Set fixture");
    xvalue* inputs[]={plain,unlisted,plainSet};
    for(size_t i=0;i<3;++i) {
        result=xrtValueGraphCopyV1(inputs[i],&config);
        testRequire(result && xrtValueType(result)==xrtValueType(inputs[i]) &&
            !xrtValueTypeId(result),"unlisted container representation changed");
        xrtValueRelease(result); xrtValueRelease(inputs[i]);
    }
    xvalue* wrong=list(edge,SET_ID);
    testRequire(!xrtValueGraphCopyV1(wrong,&config),"wrong physical identity accepted");
    error(XERR_TYPE); xrtValueRelease(wrong);
    wrong=xrtValueObject(); testRequire(wrong && xrtValueTypeIdBind(wrong,LIST_ID),"wrong object fixture");
    testRequire(!xrtValueGraphCopyV1(wrong,&config),"sequence identity on object accepted");
    error(XERR_TYPE); xrtValueRelease(wrong);

    xmemdebugsnapshot baseline=live();
    for(int mode=0;mode<6;++mode) {
        xvaluegraphcopyv1 bad=config;
        xvaluegraphsequencev1 invalid[2]={{LIST_ID,XVALUE_INT_MAP},{SET_ID,XVALUE_SET}};
        bad.Sequences=invalid;
        if(mode==0) bad.Sequences=NULL;
        if(mode==1) bad.Flags=0;
        if(mode==2) invalid[0].TypeId=0;
        if(mode==3) invalid[0].Type=XVALUE_OBJECT;
        if(mode==4) invalid[1].TypeId=LIST_ID;
        if(mode==5) --bad.Size;
        testRequire(!xrtValueGraphCopyV1(root,&bad),"invalid sequence policy accepted");
        error(XERR_ARGUMENT); balanced(baseline);
    }
    /* Original callers have an actual shorter object, not merely a smaller
     * Size inside today's struct. ASan must find no trailing-field read. */
    struct original { size_t Size; uint32 Flags; xvaluegraphhandlecopyv1 CopyHandle;
        ptr UserData; xvaluegraphobjectcopyv1 CopyObject; } original={0};
    original.Size=sizeof(original); original.Flags=XVALUE_GRAPH_COPY_DATA_V1;
    testRequire(sizeof(original)==offsetof(xvaluegraphcopyv1,Sequences),"V1 prefix size");
    result=xrtValueGraphCopyV1(root,(const xvaluegraphcopyv1*)(const void*)&original);
    testRequire(result && xrtValueType(xrtValueArrayAt(result,0))==XVALUE_INT_MAP,
        "original V1 configuration changed or read beyond its prefix");
    xrtValueRelease(result); balanced(baseline);

    size_t failures=0,firstNoHit=0;
    for(size_t point=0;point<4097;++point) {
        testRequire(xrtMemDebugFailAfter(point),"arm sequence allocation failure");
        result=xrtValueGraphCopyV1(root,&config);
        bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if(hit) { ++failures; testRequire(!result,"sequence failure published a partial result"); error(XERR_MEMORY); }
        else { requireProjection(result,root); firstNoHit=point; }
        xrtValueRelease(result); balanced(baseline);
        if(!hit) break;
    }
    testRequire(failures && failures==firstNoHit,"incomplete sequence OOM prefix");
    xrtValueRelease(root); xrtValueRelease(set); xrtValueRelease(object); xrtValueRelease(edge);
    xrtClearError(); balanced(initial); testMemoryDebugDrain("sequence retained allocations");
    printf("[PASS] nominal sequence policies; %zu complete allocation failure positions\n",failures);
    return 0;
}
