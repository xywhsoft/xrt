#include "xrt/core.h"
#include <assert.h>
#include <stdio.h>

#if UINTPTR_MAX == UINT64_MAX
#if defined(__cplusplus)
static_assert(sizeof(xrtownershipfinishv1)==24,"V1 ABI remains unchanged");
static_assert(sizeof(xrtownershipfinishv2)==40,"V2 ABI");
static_assert(offsetof(xrtownershipfinishv2,V1)==0,"exact prefix");
#else
_Static_assert(sizeof(xrtownershipfinishv1)==24,"V1 ABI remains unchanged");
_Static_assert(sizeof(xrtownershipfinishv2)==40,"V2 ABI");
_Static_assert(offsetof(xrtownershipfinishv2,V1)==0,"exact prefix");
#endif
#endif
typedef struct Counter {size_t refs;unsigned checked,legacy;bool fail;} Counter;
static bool hold(const void* data){++((Counter*)data)->refs;return true;}
static void drop(const void* data){Counter* c=(Counter*)data;assert(c->refs);--c->refs;++c->legacy;}
static bool claim(const void* data,const void* token){return data && token;}
static void clear(const void* data,const void* token){assert(data && token);}
static bool finish(const void* data,const void* token){return data && token;}
static xrtownershipfinishresult step(const void* data,const void* token)
{return data && token ? XRT_OWNERSHIP_FINISH_READY:XRT_OWNERSHIP_FINISH_FAILED;}
static bool checkedDrop(const void* data)
{Counter* c=(Counter*)data;assert(c->refs);--c->refs;++c->checked;return !c->fail;}
int main(void)
{
 const xrtownershipadapterv1 adapter={sizeof(adapter),hold,drop,claim,clear,NULL,clear,finish};
 const xrtownershipfinishv1 old={sizeof(old),&adapter,step};
 const xrtownershipfinishv2 typed={{sizeof(typed),&adapter,step},XRT_OWNERSHIP_FINISH_V2_TAG,checkedDrop};
 const xrtownershipfinishv1* prefix=&typed.V1;
 assert(prefix->size==sizeof(typed) && prefix->Adapter==old.Adapter);
 assert(prefix->Step(&adapter,&typed)==XRT_OWNERSHIP_FINISH_READY);
 Counter c={1,0,0,false};assert(adapter.Hold(&c) && c.refs==2);
 assert(typed.DropChecked(&c) && c.refs==1 && c.checked==1 && c.legacy==0);
 c.fail=true;assert(!typed.DropChecked(&c) && c.refs==0 && c.checked==2 && c.legacy==0);
 c.refs=1;old.Adapter->Drop(&c);assert(c.refs==0 && c.legacy==1);
 puts("ownership completion V2: unchanged V1 prefix, typed ABI and consumed failed drop");
 return 0;
}
