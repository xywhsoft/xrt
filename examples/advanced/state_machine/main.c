/*
 * XRT Example - State Machine
 * XRT 范例 - 状态机
 *
 * Description / 说明:
 *   EN: Demonstrates finite state machine implementation for protocol parsing,
 *       lexical analysis, and event-driven systems. Shows state transitions,
 *       event handling, and callback mechanisms. Implements a simple HTTP-like
 *       request parser as example.
 *   CN: 演示有限状态机实现，用于协议解析、词法分析和事件驱动系统。
 *       展示状态转换、事件处理和回调机制。
 *       实现一个简单的类HTTP请求解析器作为示例。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/advanced_state_machine.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/advanced_state_machine -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Finite state machine pattern
 *   - State transition tables
 *   - Event-driven architecture
 *   - Callback functions
 *   - Protocol parsing example
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <ctype.h>
#include <time.h>

// State machine states
// 状态机状态
typedef enum {
	STATE_START = 0,
	STATE_METHOD,
	STATE_URI,
	STATE_VERSION,
	STATE_HEADER_NAME,
	STATE_HEADER_VALUE,
	STATE_BODY,
	STATE_COMPLETE,
	STATE_ERROR
} State;

// Events that trigger state transitions
// 触发状态转换的事件
typedef enum {
	EVENT_CHAR = 1,
	EVENT_SPACE,
	EVENT_NEWLINE,
	EVENT_COLON,
	EVENT_EOF
} Event;

// Forward declarations
// 前向声明
typedef struct StateMachine StateMachine;
typedef void (*StateHandler)(StateMachine* pSM, Event eEvent, char cChar);

// State machine structure
// 状态机结构
struct StateMachine {
	State eCurrentState;
	StateHandler aStateHandlers[9];  // One for each state / 每个状态一个处理器
	
	// Parsed data storage
	// 解析的数据存储
	xbuffer pBufferMethod;
	xbuffer pBufferUri;
	xbuffer pBufferVersion;
	xbuffer pBufferHeaderName;
	xbuffer pBufferHeaderValue;
	xbuffer pBufferBody;
	
	str sCurrentHeaderName;
	
	// Statistics
	// 统计信息
	int iHeadersParsed;
	int iBytesProcessed;
};

// State handler functions forward declarations
// 状态处理函数前向声明
void HandleStart(StateMachine* pSM, Event eEvent, char cChar);
void HandleMethod(StateMachine* pSM, Event eEvent, char cChar);
void HandleUri(StateMachine* pSM, Event eEvent, char cChar);
void HandleVersion(StateMachine* pSM, Event eEvent, char cChar);
void HandleHeaderName(StateMachine* pSM, Event eEvent, char cChar);
void HandleHeaderValue(StateMachine* pSM, Event eEvent, char cChar);
void HandleBody(StateMachine* pSM, Event eEvent, char cChar);
void HandleComplete(StateMachine* pSM, Event eEvent, char cChar);
void HandleError(StateMachine* pSM, Event eEvent, char cChar);

// Create state machine
// 创建状态机
StateMachine* StateMachineCreate()
{
	StateMachine* pSM = xrtMalloc(sizeof(StateMachine));
	pSM->eCurrentState = STATE_START;
	
	// Initialize state handlers
	// 初始化状态处理器
	pSM->aStateHandlers[STATE_START] = HandleStart;
	pSM->aStateHandlers[STATE_METHOD] = HandleMethod;
	pSM->aStateHandlers[STATE_URI] = HandleUri;
	pSM->aStateHandlers[STATE_VERSION] = HandleVersion;
	pSM->aStateHandlers[STATE_HEADER_NAME] = HandleHeaderName;
	pSM->aStateHandlers[STATE_HEADER_VALUE] = HandleHeaderValue;
	pSM->aStateHandlers[STATE_BODY] = HandleBody;
	pSM->aStateHandlers[STATE_COMPLETE] = HandleComplete;
	pSM->aStateHandlers[STATE_ERROR] = HandleError;
	
	// Initialize buffers
	// 初始化缓冲区
	pSM->pBufferMethod = xrtBufferCreate(0);
	pSM->pBufferUri = xrtBufferCreate(0);
	pSM->pBufferVersion = xrtBufferCreate(0);
	pSM->pBufferHeaderName = xrtBufferCreate(0);
	pSM->pBufferHeaderValue = xrtBufferCreate(0);
	pSM->pBufferBody = xrtBufferCreate(0);
	
	pSM->sCurrentHeaderName = NULL;
	pSM->iHeadersParsed = 0;
	pSM->iBytesProcessed = 0;
	
	return pSM;
}

// Destroy state machine
// 销毁状态机
void StateMachineDestroy(StateMachine* pSM)
{
	if ( pSM ) {
		xrtBufferDestroy(pSM->pBufferMethod);
		xrtBufferDestroy(pSM->pBufferUri);
		xrtBufferDestroy(pSM->pBufferVersion);
		xrtBufferDestroy(pSM->pBufferHeaderName);
		xrtBufferDestroy(pSM->pBufferHeaderValue);
		xrtBufferDestroy(pSM->pBufferBody);
		
		if ( pSM->sCurrentHeaderName ) {
			xrtFree(pSM->sCurrentHeaderName);
		}
		
		xrtFree(pSM);
	}
}

// Get current state as string
// 获取当前状态的字符串表示
str StateToString(State eState)
{
	switch ( eState ) {
		case STATE_START: return "START";
		case STATE_METHOD: return "METHOD";
		case STATE_URI: return "URI";
		case STATE_VERSION: return "VERSION";
		case STATE_HEADER_NAME: return "HEADER_NAME";
		case STATE_HEADER_VALUE: return "HEADER_VALUE";
		case STATE_BODY: return "BODY";
		case STATE_COMPLETE: return "COMPLETE";
		case STATE_ERROR: return "ERROR";
		default: return "UNKNOWN";
	}
}

// Transition to new state
// 转换到新状态
void StateMachineTransition(StateMachine* pSM, State eNewState)
{
	printf("State transition: %s -> %s\n", 
	       StateToString(pSM->eCurrentState), StateToString(eNewState));
	pSM->eCurrentState = eNewState;
}

// Process event
// 处理事件
void StateMachineProcessEvent(StateMachine* pSM, Event eEvent, char cChar)
{
	if ( pSM->eCurrentState < 9 ) {
		pSM->aStateHandlers[pSM->eCurrentState](pSM, eEvent, cChar);
	}
	pSM->iBytesProcessed++;
}

// Handle START state
// 处理START状态
void HandleStart(StateMachine* pSM, Event eEvent, char cChar)
{
	switch ( eEvent ) {
		case EVENT_CHAR:
			if ( isalpha(cChar) ) {
				xrtBufferAppend(pSM->pBufferMethod, &cChar, 1, XBUF_BINARY);
				StateMachineTransition(pSM, STATE_METHOD);
			} else {
				StateMachineTransition(pSM, STATE_ERROR);
			}
			break;
		default:
			StateMachineTransition(pSM, STATE_ERROR);
			break;
	}
}

// Handle METHOD state
// 处理METHOD状态
void HandleMethod(StateMachine* pSM, Event eEvent, char cChar)
{
	switch ( eEvent ) {
		case EVENT_CHAR:
			if ( isalpha(cChar) ) {
				xrtBufferAppend(pSM->pBufferMethod, &cChar, 1, XBUF_BINARY);
			} else {
				StateMachineTransition(pSM, STATE_ERROR);
			}
			break;
		case EVENT_SPACE:
			xrtBufferAppend(pSM->pBufferMethod, "\0", 1, XBUF_ANSI);  // Null terminate / 空终止
			StateMachineTransition(pSM, STATE_URI);
			break;
		default:
			StateMachineTransition(pSM, STATE_ERROR);
			break;
	}
}

// Handle URI state
// 处理URI状态
void HandleUri(StateMachine* pSM, Event eEvent, char cChar)
{
	switch ( eEvent ) {
		case EVENT_CHAR:
			if ( cChar != ' ' ) {
				xrtBufferAppend(pSM->pBufferUri, &cChar, 1, XBUF_BINARY);
			} else {
				xrtBufferAppend(pSM->pBufferUri, "\0", 1, XBUF_ANSI);
				StateMachineTransition(pSM, STATE_VERSION);
			}
			break;
		default:
			StateMachineTransition(pSM, STATE_ERROR);
			break;
	}
}

// Handle VERSION state
// 处理VERSION状态
void HandleVersion(StateMachine* pSM, Event eEvent, char cChar)
{
	switch ( eEvent ) {
		case EVENT_CHAR:
			xrtBufferAppend(pSM->pBufferVersion, &cChar, 1, XBUF_BINARY);
			break;
		case EVENT_NEWLINE:
			xrtBufferAppend(pSM->pBufferVersion, "\0", 1, XBUF_ANSI);
			StateMachineTransition(pSM, STATE_HEADER_NAME);
			break;
		default:
			StateMachineTransition(pSM, STATE_ERROR);
			break;
	}
}

// Handle HEADER_NAME state
// 处理HEADER_NAME状态
void HandleHeaderName(StateMachine* pSM, Event eEvent, char cChar)
{
	switch ( eEvent ) {
		case EVENT_CHAR:
			if ( cChar != ':' ) {
				xrtBufferAppend(pSM->pBufferHeaderName, &cChar, 1, XBUF_BINARY);
			} else {
				xrtBufferAppend(pSM->pBufferHeaderName, "\0", 1, XBUF_ANSI);
				StateMachineTransition(pSM, STATE_HEADER_VALUE);
			}
			break;
		case EVENT_NEWLINE:
			// Empty line indicates end of headers
			// 空行表示头部结束
			StateMachineTransition(pSM, STATE_BODY);
			break;
		default:
			StateMachineTransition(pSM, STATE_ERROR);
			break;
	}
}

// Handle HEADER_VALUE state
// 处理HEADER_VALUE状态
void HandleHeaderValue(StateMachine* pSM, Event eEvent, char cChar)
{
	switch ( eEvent ) {
		case EVENT_CHAR:
			xrtBufferAppend(pSM->pBufferHeaderValue, &cChar, 1, XBUF_BINARY);
			break;
		case EVENT_NEWLINE:
			xrtBufferAppend(pSM->pBufferHeaderValue, "\0", 1, XBUF_ANSI);
			
			// Store header
			// 存储头部
			pSM->sCurrentHeaderName = xrtCopyStr(
				(char*)pSM->pBufferHeaderName->Buffer,
				pSM->pBufferHeaderName->Length
			);
			
			str sHeaderValue = xrtCopyStr(
				(char*)pSM->pBufferHeaderValue->Buffer,
				pSM->pBufferHeaderValue->Length
			);
			
			printf("Header: %s = %s\n", pSM->sCurrentHeaderName, sHeaderValue);
			xrtFree(sHeaderValue);
			
			if ( pSM->sCurrentHeaderName ) {
				xrtFree(pSM->sCurrentHeaderName);
				pSM->sCurrentHeaderName = NULL;
			}
			
			pSM->iHeadersParsed++;
			
			// Reset buffers for next header
			// 重置缓冲区以处理下一个头部
			xrtBufferUnit(pSM->pBufferHeaderName);
			xrtBufferUnit(pSM->pBufferHeaderValue);
			
			StateMachineTransition(pSM, STATE_HEADER_NAME);
			break;
		default:
			StateMachineTransition(pSM, STATE_ERROR);
			break;
	}
}

// Handle BODY state
// 处理BODY状态
void HandleBody(StateMachine* pSM, Event eEvent, char cChar)
{
	switch ( eEvent ) {
		case EVENT_CHAR:
			xrtBufferAppend(pSM->pBufferBody, &cChar, 1, XBUF_BINARY);
			break;
		case EVENT_EOF:
			StateMachineTransition(pSM, STATE_COMPLETE);
			break;
		default:
			// Continue collecting body
			// 继续收集主体
			break;
	}
}

// Handle COMPLETE state
// 处理COMPLETE状态
void HandleComplete(StateMachine* pSM, Event eEvent, char cChar)
{
	// Stay in complete state
	// 保持在完成状态
}

// Handle ERROR state
// 处理ERROR状态
void HandleError(StateMachine* pSM, Event eEvent, char cChar)
{
	// Stay in error state
	// 保持在错误状态
	printf("Error occurred at byte %d\n", pSM->iBytesProcessed);
}

// Parse HTTP-like request
// 解析类HTTP请求
void ParseHttpRequest(StateMachine* pSM, str sRequest)
{
	printf("Parsing request:\n%s\n", sRequest);
	printf("================\n");
	
	size_t iLen = strlen(sRequest);
	
	for ( size_t i = 0; i < iLen; i++ ) {
		char c = sRequest[i];
		Event eEvent;
		
		if ( c == ' ' ) {
			eEvent = EVENT_SPACE;
		} else if ( c == '\n' || c == '\r' ) {
			eEvent = EVENT_NEWLINE;
		} else if ( c == ':' ) {
			eEvent = EVENT_COLON;
		} else {
			eEvent = EVENT_CHAR;
		}
		
		StateMachineProcessEvent(pSM, eEvent, c);
		
		if ( pSM->eCurrentState == STATE_ERROR ) {
			break;
		}
	}
	
	// Send EOF event
	// 发送EOF事件
	StateMachineProcessEvent(pSM, EVENT_EOF, '\0');
}

// Print parsed results
// 打印解析结果
void PrintParsedResults(StateMachine* pSM)
{
	printf("\n=== Parsed Results ===\n");
	printf("=== 解析结果 ===\n");
	
	if ( pSM->eCurrentState == STATE_COMPLETE ) {
		printf("Status: Success\n");
		printf("Method: %s\n", (char*)pSM->pBufferMethod->Buffer);
		printf("URI: %s\n", (char*)pSM->pBufferUri->Buffer);
		printf("Version: %s\n", (char*)pSM->pBufferVersion->Buffer);
		printf("Headers parsed: %d\n", pSM->iHeadersParsed);
		printf("Body size: %u bytes\n", pSM->pBufferBody->Length);
		printf("Total bytes processed: %d\n", pSM->iBytesProcessed);
	} else {
		printf("Status: Failed (State: %s)\n", StateToString(pSM->eCurrentState));
	}
}

// Test state machine with valid request
// 用有效请求测试状态机
void TestValidRequest()
{
	printf("=== Valid Request Test ===\n");
	printf("=== 有效请求测试 ===\n");
	
	StateMachine* pSM = StateMachineCreate();
	
	str sRequest = 
		"GET /index.html HTTP/1.1\r\n"
		"Host: example.com\r\n"
		"User-Agent: XRT-Client/1.0\r\n"
		"Accept: */*\r\n"
		"\r\n"
		"<html><body>Hello World</body></html>";
	
	ParseHttpRequest(pSM, sRequest);
	PrintParsedResults(pSM);
	
	StateMachineDestroy(pSM);
}

// Test state machine with invalid request
// 用无效请求测试状态机
void TestInvalidRequest()
{
	printf("\n=== Invalid Request Test ===\n");
	printf("=== 无效请求测试 ===\n");
	
	StateMachine* pSM = StateMachineCreate();
	
	str sRequest = "INVALID REQUEST WITHOUT PROPER FORMAT";
	
	ParseHttpRequest(pSM, sRequest);
	PrintParsedResults(pSM);
	
	StateMachineDestroy(pSM);
}

// Test incremental parsing
// 测试增量解析
void TestIncrementalParsing()
{
	printf("\n=== Incremental Parsing Test ===\n");
	printf("=== 增量解析测试 ===\n");
	
	StateMachine* pSM = StateMachineCreate();
	
	// Parse character by character
	// 逐字符解析
	str sRequest = "POST /api/data HTTP/1.1\r\nContent-Length: 10\r\n\r\n1234567890";
	
	printf("Parsing incrementally:\n");
	
	size_t iLen = strlen(sRequest);
	for ( size_t i = 0; i < iLen; i++ ) {
		char c = sRequest[i];
		Event eEvent = (c == ' ') ? EVENT_SPACE : 
		              (c == '\n' || c == '\r') ? EVENT_NEWLINE :
		              (c == ':') ? EVENT_COLON : EVENT_CHAR;
		
		StateMachineProcessEvent(pSM, eEvent, c);
		
		// Show state every 10 characters
		// 每10个字符显示一次状态
		if ( (i + 1) % 10 == 0 || i == iLen - 1 ) {
			printf("After %zu chars: State = %s\n", i + 1, StateToString(pSM->eCurrentState));
		}
		
		if ( pSM->eCurrentState == STATE_ERROR ) {
			break;
		}
	}
	
	StateMachineProcessEvent(pSM, EVENT_EOF, '\0');
	PrintParsedResults(pSM);
	
	StateMachineDestroy(pSM);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT State Machine Demo\n");
	printf("XRT 状态机演示\n");
	printf("===================\n\n");
	
	// Run tests
	// 运行测试
	TestValidRequest();
	TestInvalidRequest();
	TestIncrementalParsing();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}