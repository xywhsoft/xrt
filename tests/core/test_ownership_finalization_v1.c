/* Public C/C++ type/ABI and orthogonal completion/failure contract. The actual
 * graph collector continuation/rollback tests live in its runtime consumer. */
#if defined(XRT_TEST_SINGLE_DECL)
#include "single/xrt_decl.h"
#elif defined(XRT_TEST_SINGLE_FULL)
#include "single/xrt.h"
#else
#include "xrt/core.h"
#endif
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#if defined(__cplusplus)
#define STATIC_ASSERT static_assert
#else
#define STATIC_ASSERT _Static_assert
#endif
STATIC_ASSERT(sizeof(void*)!=8 || sizeof(xrtownershipadapterv1)==64,"V1 adapter unchanged");
STATIC_ASSERT(sizeof(void*)!=8 || sizeof(xrtownershipfinishv2)==48,"V2 completion unchanged");
STATIC_ASSERT(sizeof(void*)!=8 || sizeof(xrtownershipfinalizationv1)==40,"Finalization ABI");
STATIC_ASSERT(offsetof(xrtownershipfinalizationv1,size)==0,"Size first");
typedef struct Duty {int calls;bool ready,failed;} Duty;
static bool eligible(const void* data,const void* token){return data!=NULL && token==data;}
static bool step(const void* data,const void* token,bool* complete)
{
 Duty* duty=(Duty*)data;assert(eligible(data,token) && complete);++duty->calls;
 *complete=duty->ready;return !duty->failed;
}
static const xrtownershipadapterv1 adapter={sizeof(xrtownershipadapterv1),NULL,NULL,NULL,NULL,NULL,NULL,NULL};
static const xrtownershipfinalizationv1 protocol={sizeof(xrtownershipfinalizationv1),XRT_OWNERSHIP_FINALIZATION_V1_TAG,&adapter,eligible,step};
int main(void)
{
 assert(protocol.Adapter==&adapter && protocol.CanStep && protocol.Step);
 for(int failure=0;failure<2;++failure)for(int done=0;done<2;++done){
  Duty duty={0,done!=0,failure!=0};bool complete=false;
  assert(protocol.CanStep(&duty,&duty));bool success=protocol.Step(&duty,&duty,&complete);
  assert(success==(failure==0) && complete==(done!=0) && duty.calls==1);
 }
 puts("independent FinalizationV1 ABI and four completion/failure outcomes passed");return 0;
}
