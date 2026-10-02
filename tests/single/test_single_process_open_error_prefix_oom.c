/* Exercise the real error construction transaction without launching a GUI. */
#define XRT_MODULE_PROCESS_OPEN
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    unsigned failures=0,completions=0;
    assert(xrtMemDebugEnable(true));
    for(unsigned cause=0;cause<3;++cause) {
        bool complete=false;
        for(size_t point=0;point<32;++point) {
            xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
            if(cause==1)xrtSetErrorKind(XERR_IO);
            if(cause==2)xrtSetErrorInfo(XERR_IO,"controlled.launch",7,"native launch cause");
            const xerror* original=xrtGetError();
            assert(cause ? original!=NULL : original==NULL);
            assert(xrtMemDebugFailAfter(point));
            __xrtProcessOpenError("controlled launch failed");
            bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
            const xerror* error=xrtGetError();
            if(hit){assert(xrtErrorKind(error)==XERR_MEMORY);++failures;}
            else {
                assert(xrtErrorKind(error)==XERR_IO);
                assert(xrtErrorCode(error)==XPROCESS_ERROR_OPEN);
                assert(!strcmp(xrtErrorDomain(error),"xrt.process"));
                assert(!strcmp(xrtErrorOperation(error),"open"));
                assert(xrtErrorCause(error)==original);++completions;
            }
            xrtClearError();xrtMemDebugSnapshot(&after);
            assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
            assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
            assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
            assert(before.UseAfterFreeCount==after.UseAfterFreeCount);
            if(!hit){complete=true;break;}
        }
        assert(complete);
    }
    printf("process open error prefix: %u failures and %u first no-hit, causes preserved, balanced\n",failures,completions);
    return 0;
}
