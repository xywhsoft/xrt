#include <xrt/console.h>
#include <assert.h>
#include <wchar.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>

static DWORD WINAPI destroyFromOtherThread(void* Context)
{
    xconsolesession* Session=(xconsolesession*)Context;
    assert(!xrtConsoleSessionWrite(Session,XRT_STR_LITERAL("forbidden")));
    assert(xrtErrorKind(xrtGetError())==XERR_STATE);
    xrtClearError();
    xrtConsoleSessionDestroy(Session);
    return 0;
}
static int runChild(void)
{
    HANDLE Input=GetStdHandle(STD_INPUT_HANDLE), Output=GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD BeforeIn, BeforeOut, AfterIn, AfterOut, Written;
    CONSOLE_CURSOR_INFO BeforeCursor, AfterCursor;
    INPUT_RECORD Records[8]={{0}};
    xconsolesession* Session; xconsoleevent* Event;
    assert(GetConsoleMode(Input,&BeforeIn) && GetConsoleMode(Output,&BeforeOut));
    assert(GetConsoleCursorInfo(Output,&BeforeCursor));
    assert(FlushConsoleInputBuffer(Input));
    Session=xrtConsoleSessionOpen(15);
    assert(Session!=NULL && !xrtConsoleSessionPasteSupported(Session));
    Records[0].EventType=KEY_EVENT; Records[0].Event.KeyEvent.bKeyDown=TRUE;
    Records[0].Event.KeyEvent.wRepeatCount=1; Records[0].Event.KeyEvent.uChar.UnicodeChar=0x4f60;
    Records[1]=Records[0]; Records[1].Event.KeyEvent.uChar.UnicodeChar=0xd83d;
    Records[2]=Records[1]; Records[2].Event.KeyEvent.bKeyDown=FALSE;
    Records[3]=Records[0]; Records[3].Event.KeyEvent.uChar.UnicodeChar=0xde00;
    Records[4]=Records[3]; Records[4].Event.KeyEvent.bKeyDown=FALSE;
    Records[5]=Records[0]; Records[5].Event.KeyEvent.uChar.UnicodeChar=0;
    Records[5].Event.KeyEvent.wVirtualKeyCode=VK_UP;
    Records[6].EventType=MOUSE_EVENT; Records[6].Event.MouseEvent.dwMousePosition.X=3;
    Records[6].Event.MouseEvent.dwMousePosition.Y=4; Records[6].Event.MouseEvent.dwButtonState=FROM_LEFT_1ST_BUTTON_PRESSED;
    Records[7]=Records[0]; Records[7].Event.KeyEvent.uChar.UnicodeChar='@';
    Records[7].Event.KeyEvent.dwControlKeyState=LEFT_CTRL_PRESSED|RIGHT_ALT_PRESSED;
    assert(WriteConsoleInputW(Input,Records,8,&Written) && Written==8);
    Event=xrtConsoleSessionRead(Session,1000);
    assert(Event!=NULL && Event->Kind==XCONSOLE_EVENT_TEXT && Event->Text->Size==3);
    assert(memcmp(Event->Text->Data,"\xe4\xbd\xa0",3)==0); xrtConsoleEventDestroy(Event);
    Event=xrtConsoleSessionRead(Session,1000);
    assert(Event!=NULL && Event->Kind==XCONSOLE_EVENT_TEXT && Event->Text->Size==4);
    assert(memcmp(Event->Text->Data,"\xf0\x9f\x98\x80",4)==0); xrtConsoleEventDestroy(Event);
    Event=xrtConsoleSessionRead(Session,1000);
    assert(Event!=NULL && Event->Kind==XCONSOLE_EVENT_KEY && Event->Key==XCONSOLE_KEY_UP);
    xrtConsoleEventDestroy(Event);
    Event=xrtConsoleSessionRead(Session,1000);
    assert(Event!=NULL && Event->Kind==XCONSOLE_EVENT_MOUSE && Event->X==3 && Event->Y==4);
    xrtConsoleEventDestroy(Event);
    Event=xrtConsoleSessionRead(Session,1000);
    assert(Event!=NULL && Event->Kind==XCONSOLE_EVENT_TEXT && Event->Key=='@' && Event->Modifiers==6);
    assert(Event->Text->Size==1 && Event->Text->Data[0]=='@'); xrtConsoleEventDestroy(Event);
    assert(xrtConsoleSessionRead(Session,20)==NULL && xrtGetError()==NULL);
    assert(xrtConsoleSessionCursor(Session,false) && xrtConsoleSessionMove(Session,0,0));
    assert(xrtConsoleSessionStyle(Session,0x1123456,-1,1));
    assert(xrtConsoleSessionWrite(Session,XRT_STR_LITERAL("\xe4\xbd\xa0")));
    assert(xrtConsoleSessionClose(Session) && xrtConsoleSessionClose(Session));
    assert(GetConsoleMode(Input,&AfterIn) && GetConsoleMode(Output,&AfterOut));
    assert(BeforeIn==AfterIn && BeforeOut==AfterOut);
    assert(GetConsoleCursorInfo(Output,&AfterCursor) && BeforeCursor.bVisible==AfterCursor.bVisible && BeforeCursor.dwSize==AfterCursor.dwSize);
    xrtConsoleSessionDestroy(Session);
    Session=xrtConsoleSessionOpen(1); assert(Session!=NULL);
    { HANDLE Thread=CreateThread(NULL,0,destroyFromOtherThread,Session,0,NULL);
      assert(Thread!=NULL && WaitForSingleObject(Thread,2000)==WAIT_OBJECT_0); assert(CloseHandle(Thread)); }
    assert(GetConsoleMode(Input,&AfterIn) && BeforeIn==AfterIn);
    Session=xrtConsoleSessionOpen(1); assert(Session!=NULL); xrtConsoleSessionDestroy(Session);
    return 0;
}
int main(int argc, char** argv)
{
    WCHAR Path[32768], Command[32780]; STARTUPINFOW Startup={0}; PROCESS_INFORMATION Process={0}; DWORD Code;
    (void)argv;
    if (argc>1) return runChild();
    assert(GetModuleFileNameW(NULL,Path,32768)!=0);
    assert(swprintf(Command,32780,L"\"%ls\" child",Path)>0);
    Startup.cb=sizeof(Startup);
    /* A real console with no desktop window; no inherited user terminal is modified. */
    assert(CreateProcessW(Path,Command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&Startup,&Process));
    assert(WaitForSingleObject(Process.hProcess,10000)==WAIT_OBJECT_0);
    assert(GetExitCodeProcess(Process.hProcess,&Code) && Code==0);
    assert(CloseHandle(Process.hThread) && CloseHandle(Process.hProcess));
    return 0;
}
#else
int main(void) { return 0; }
#endif
