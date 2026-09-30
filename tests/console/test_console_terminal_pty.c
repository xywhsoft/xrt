/* Launched with a pseudo-terminal; deterministic input is supplied by the driver. */
#define XRT_MODULE_CONSOLE_TERMINAL
#define XRT_MODULE_CONSOLE_INPUT
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#include <assert.h>
#if !defined(_WIN32) && !defined(_WIN64)
#include <termios.h>
static void testParserBounds(void)
{
    uint32 Values[3] = {0};
    xconsolesession Session = {0};
    xconsoleevent* Event;
    assert(__xrtConsoleNumbers((const unsigned char*)"4294967295",10,Values,1) && Values[0]==UINT32_MAX);
    assert(!__xrtConsoleNumbers((const unsigned char*)"4294967296",10,Values,1));
    assert(!__xrtConsoleNumbers((const unsigned char*)"1;2x",4,Values,2));
    assert(!__xrtConsoleNumbers((const unsigned char*)"1;;2",4,Values,3));
    assert(xrtBufferInit(&Session.Input));
    assert(xrtBufferAppend(&Session.Input,(xbytesview){(const unsigned char*)"\x1b[4294967296A",13}));
    Event=__xrtConsoleParseEvent(&Session);
    assert(Event!=NULL && Event->Kind==XCONSOLE_EVENT_KEY && Event->Key==XCONSOLE_KEY_ESCAPE);
    assert(Session.Input.Size==12); xrtConsoleEventDestroy(Event);
    xrtBufferUnit(&Session.Input);
}
int main(void)
{
    struct termios Before, After;
    xconsolesession* Session;
    xconsoleevent* Event;
    xreader* Raw;
    unsigned char Byte;
    size_t Read;
    testParserBounds();
    assert(tcgetattr(0,&Before)==0);
    Session=xrtConsoleSessionOpen(15);
    assert(Session!=NULL && xrtConsoleSessionPasteSupported(Session));
    assert(xrtConsoleSessionOpen(1)==NULL && xrtErrorKind(xrtGetError())==XERR_STATE);
    xrtClearError();
    Raw=xrtReaderStdin();
    assert(Raw==NULL);
    assert(xrtErrorKind(xrtGetError())==XERR_STATE);
    xrtClearError();
    (void)Byte; (void)Read;
    assert(xrtConsoleSessionWrite(Session,XRT_STR_LITERAL("READY\n")) && xrtConsoleSessionFlush(Session));
    Event=xrtConsoleSessionRead(Session,2000);
    assert(Event!=NULL && Event->Kind==XCONSOLE_EVENT_TEXT && Event->Text->Size==3);
    assert(memcmp(Event->Text->Data,"\xe4\xbd\xa0",3)==0); xrtConsoleEventDestroy(Event);
    Event=xrtConsoleSessionRead(Session,2000);
    assert(Event!=NULL && Event->Kind==XCONSOLE_EVENT_KEY && Event->Key==XCONSOLE_KEY_UP);
    xrtConsoleEventDestroy(Event);
    Event=xrtConsoleSessionRead(Session,2000);
    assert(Event!=NULL && Event->Kind==XCONSOLE_EVENT_KEY && Event->Key==XCONSOLE_KEY_UP && Event->Modifiers==2);
    xrtConsoleEventDestroy(Event);
    Event=xrtConsoleSessionRead(Session,2000);
    assert(Event!=NULL && Event->Kind==XCONSOLE_EVENT_KEY && Event->Key==XCONSOLE_KEY_UP && Event->Modifiers==4);
    xrtConsoleEventDestroy(Event);
    Event=xrtConsoleSessionRead(Session,2000);
    assert(Event!=NULL && Event->Kind==XCONSOLE_EVENT_TEXT && Event->Key==0xe9 && Event->Modifiers==4);
    xrtConsoleEventDestroy(Event);
    Event=xrtConsoleSessionRead(Session,2000);
    assert(Event!=NULL && Event->Kind==XCONSOLE_EVENT_PASTE && Event->Text->Size==5);
    assert(memcmp(Event->Text->Data,"a\0b\nZ",5)==0); xrtConsoleEventDestroy(Event);
    Event=xrtConsoleSessionRead(Session,2000);
    assert(Event!=NULL && Event->Kind==XCONSOLE_EVENT_MOUSE && Event->X==2 && Event->Y==3 && Event->Key==1 && Event->Down);
    xrtConsoleEventDestroy(Event);
    assert(xrtConsoleSessionWrite(Session,XRT_STR_LITERAL("RESIZE\n")) && xrtConsoleSessionFlush(Session));
    Event=xrtConsoleSessionRead(Session,2000);
    assert(Event!=NULL && Event->Kind==XCONSOLE_EVENT_RESIZE && Event->Columns==91 && Event->Rows==31);
    xrtConsoleEventDestroy(Event);
    assert(xrtConsoleSessionRead(Session,20)==NULL && xrtGetError()==NULL);
    assert(xrtConsoleSessionCursor(Session,false));
    assert(xrtConsoleSessionMove(Session,0,0));
    assert(xrtConsoleSessionStyle(Session,1,-1,1));
    assert(xrtConsoleSessionClose(Session) && xrtConsoleSessionClosed(Session));
    assert(xrtConsoleSessionClose(Session));
    assert(tcgetattr(0,&After)==0 && Before.c_iflag==After.c_iflag && Before.c_lflag==After.c_lflag &&
        Before.c_cflag==After.c_cflag && Before.c_oflag==After.c_oflag && memcmp(Before.c_cc,After.c_cc,sizeof(Before.c_cc))==0);
    xrtConsoleSessionDestroy(Session);
    Session=xrtConsoleSessionOpen(1); assert(Session!=NULL);
    xrtConsoleSessionDestroy(Session);
    assert(tcgetattr(0,&After)==0 && Before.c_lflag==After.c_lflag);
    return 0;
}
#else
int main(void) { return 0; }
#endif
