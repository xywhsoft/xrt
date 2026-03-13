/*
 * XRT Example - HTTP Client Server
 * XRT 范例 - HTTP客户端服务器
 *
 * Description / 说明:
 *   EN: Demonstrates HTTP client-server communication with basic HTTP/1.1 protocol
 *       implementation including GET/POST requests, static file serving,
 *       JSON API endpoints, and simple routing.
 *   CN: 演示 HTTP 客户端-服务器通信，包含基本的 HTTP/1.1 协议实现，
 *       包括 GET/POST 请求、静态文件服务、JSON API 端点和简单路由。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/network_http_client_server.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/network_http_client_server -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - HTTP server runs on localhost:8080
 *   - Implements basic HTTP/1.1 parser and responder
 *   - Static file serving from ./www/ directory
 *   - JSON REST API endpoints
 *   - Simple router for URL dispatching
 *   - HTTP client with GET/POST support
 *   - Cross-platform socket operations
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#endif

// HTTP Request structure
// HTTP 请求结构
typedef struct {
	str sMethod;
	str sPath;
	str sVersion;
	str sHeaders;
	str sBody;
} HttpRequest;

// HTTP Response structure
// HTTP 响应结构
typedef struct {
	int iStatusCode;
	str sStatusText;
	str sHeaders;
	str sBody;
} HttpResponse;

// Global server state
// 全局服务器状态
volatile int gbServerRunning = 1;

// Signal handler for graceful shutdown
// 优雅关闭的信号处理器
void HttpSignalHandler(int iSignal)
{
	gbServerRunning = 0;
	printf("\nShutting down HTTP server gracefully...\n");
}

// Parse HTTP request
// 解析 HTTP 请求
HttpRequest* ParseHttpRequest(str sRawRequest)
{
	HttpRequest* pReq = xrtMalloc(sizeof(HttpRequest));
	memset(pReq, 0, sizeof(HttpRequest));
	
	// Parse request line
	// 解析请求行
	str sLineEnd = strchr(sRawRequest, '\n');
	if ( !sLineEnd ) {
		return pReq;
	}
	
	*sLineEnd = '\0';
	str sRequestLine = sRawRequest;
	
	// Extract method, path, version
	// 提取方法、路径、版本
	str sMethodEnd = strchr(sRequestLine, ' ');
	if ( sMethodEnd ) {
		*sMethodEnd = '\0';
		pReq->sMethod = xrtCopyStr(sRequestLine, strlen(sRequestLine) + 1);
		
		str sPathStart = sMethodEnd + 1;
		str sPathEnd = strchr(sPathStart, ' ');
		if ( sPathEnd ) {
			*sPathEnd = '\0';
			pReq->sPath = xrtCopyStr(sPathStart, strlen(sPathStart) + 1);
			
			str sVersionStart = sPathEnd + 1;
			pReq->sVersion = xrtCopyStr(sVersionStart, strlen(sVersionStart) + 1);
		}
	}
	
	// Parse headers and body
	// 解析头部和主体
	str sHeaderStart = sLineEnd + 1;
	str sBodySeparator = strstr(sHeaderStart, "\r\n\r\n");
	if ( sBodySeparator ) {
		*sBodySeparator = '\0';
		pReq->sHeaders = xrtCopyStr(sHeaderStart, strlen(sHeaderStart) + 1);
		pReq->sBody = xrtCopyStr(sBodySeparator + 4, strlen(sBodySeparator + 4) + 1);
	} else {
		pReq->sHeaders = xrtCopyStr(sHeaderStart, strlen(sHeaderStart) + 1);
		pReq->sBody = xrtCopyStr("", 1);
	}
	
	return pReq;
}

// Create HTTP response
// 创建 HTTP 响应
HttpResponse* CreateHttpResponse(int iStatusCode, str sContentType, str sBody)
{
	HttpResponse* pResp = xrtMalloc(sizeof(HttpResponse));
	pResp->iStatusCode = iStatusCode;
	
	switch ( iStatusCode ) {
		case 200: pResp->sStatusText = "OK"; break;
		case 404: pResp->sStatusText = "Not Found"; break;
		case 500: pResp->sStatusText = "Internal Server Error"; break;
		default: pResp->sStatusText = "Unknown";
	}
	
	// Build headers
	// 构建头部
	size_t iBodyLen = strlen(sBody);
	pResp->sHeaders = xrtFormat(
		"Content-Type: %s\r\n"
		"Content-Length: %zu\r\n"
		"Connection: close\r\n",
		sContentType, iBodyLen
	);
	
	pResp->sBody = xrtCopyStr(sBody, iBodyLen + 1);
	
	return pResp;
}

// Format HTTP response string
// 格式化 HTTP 响应字符串
str FormatHttpResponse(HttpResponse* pResp)
{
	return xrtFormat(
		"HTTP/1.1 %d %s\r\n"
		"%s\r\n"
		"%s",
		pResp->iStatusCode, pResp->sStatusText,
		pResp->sHeaders, pResp->sBody
	);
}

// Handle API routes
// 处理 API 路由
HttpResponse* HandleApiRoute(str sPath, str sMethod, str sBody)
{
	if ( strcmp(sPath, "/api/hello") == 0 && strcmp(sMethod, "GET") == 0 ) {
		// Hello endpoint
		// Hello 端点
		str sJson = "{\n  \"message\": \"Hello, World!\",\n  \"status\": \"success\"\n}";
		return CreateHttpResponse(200, "application/json", sJson);
	}
	else if ( strcmp(sPath, "/api/echo") == 0 && strcmp(sMethod, "POST") == 0 ) {
		// Echo endpoint
		// Echo 端点
		str sJson = xrtFormat("{\n  \"echo\": \"%s\",\n  \"status\": \"success\"\n}", sBody);
		return CreateHttpResponse(200, "application/json", sJson);
	}
	else if ( strcmp(sPath, "/api/status") == 0 && strcmp(sMethod, "GET") == 0 ) {
		// Status endpoint
		// 状态端点
		time_t tNow = time(NULL);
		str sTime = ctime(&tNow);
		sTime[strlen(sTime)-1] = '\0';  // Remove newline
		
		str sJson = xrtFormat(
			"{\n"
			"  \"status\": \"running\",\n"
			"  \"timestamp\": \"%s\",\n"
			"  \"server\": \"XRT HTTP Server 1.0\"\n"
			"}",
			sTime
		);
		return CreateHttpResponse(200, "application/json", sJson);
	}
	
	// Route not found
	// 路由未找到
	str sError = "{\n  \"error\": \"Endpoint not found\",\n  \"status\": \"error\"\n}";
	return CreateHttpResponse(404, "application/json", sError);
}

// Serve static files
// 提供静态文件服务
HttpResponse* ServeStaticFile(str sFilePath)
{
	// Security: prevent directory traversal
	// 安全：防止目录遍历
	if ( strstr(sFilePath, "..") ) {
		str sError = "<html><body><h1>403 Forbidden</h1><p>Access denied.</p></body></html>";
		return CreateHttpResponse(403, "text/html", sError);
	}
	
	// Build full path
	// 构建完整路径
	str sFullpath = xrtPathJoin(2, "./www", sFilePath[0] == '/' ? sFilePath + 1 : sFilePath);
	
	// Default to index.html for directories
	// 目录默认为 index.html
	if ( sFilePath[strlen(sFilePath)-1] == '/' ) {
		sFullpath = xrtPathJoin(2, sFullpath, "index.html");
	}
	
	// Check if file exists
	// 检查文件是否存在
	if ( !xrtFileExists(sFullpath) ) {
		xrtFree(sFullpath);
		str sNotFound = "<html><body><h1>404 Not Found</h1><p>The requested file was not found.</p></body></html>";
		return CreateHttpResponse(404, "text/html", sNotFound);
	}
	
	// Determine content type
	// 确定内容类型
	str sContentType = "text/plain";
	str sExt = strrchr(sFullpath, '.');
	if ( sExt ) {
		if ( strcmp(sExt, ".html") == 0 ) sContentType = "text/html";
		else if ( strcmp(sExt, ".css") == 0 ) sContentType = "text/css";
		else if ( strcmp(sExt, ".js") == 0 ) sContentType = "application/javascript";
		else if ( strcmp(sExt, ".png") == 0 ) sContentType = "image/png";
		else if ( strcmp(sExt, ".jpg") == 0 ) sContentType = "image/jpeg";
		else if ( strcmp(sExt, ".gif") == 0 ) sContentType = "image/gif";
	}
	
	// Read file content
	// 读取文件内容
	size_t iFileSize = 0;
	str sFileContent = xrtFileReadAll(sFullpath, XRT_CP_UTF8, &iFileSize);
	
	xrtFree(sFullpath);
	
	if ( sFileContent ) {
		HttpResponse* pResp = CreateHttpResponse(200, sContentType, sFileContent);
		xrtFree(sFileContent);
		return pResp;
	} else {
		str sError = "<html><body><h1>500 Internal Server Error</h1><p>Failed to read file.</p></body></html>";
		return CreateHttpResponse(500, "text/html", sError);
	}
}

// HTTP Server implementation
// HTTP 服务器实现
void RunHttpServer()
{
	printf("=== HTTP Server ===\n");
	printf("=== HTTP 服务器 ===\n");
	
#ifdef _WIN32
	WSADATA wsaData;
	if ( WSAStartup(MAKEWORD(2, 2), &wsaData) != 0 ) {
		printf("Failed to initialize Winsock\n");
		return;
	}
#endif
	
	// Create server socket
	// 创建服务器套接字
	int iServerSocket = socket(AF_INET, SOCK_STREAM, 0);
	if ( iServerSocket < 0 ) {
		printf("Failed to create server socket\n");
#ifdef _WIN32
		WSACleanup();
#endif
		return;
	}
	
	// Allow socket reuse
	// 允许套接字重用
	int iOpt = 1;
	setsockopt(iServerSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&iOpt, sizeof(iOpt));
	
	// Bind to address
	// 绑定到地址
	struct sockaddr_in srvAddr = {0};
	srvAddr.sin_family = AF_INET;
	srvAddr.sin_addr.s_addr = INADDR_ANY;
	srvAddr.sin_port = htons(8080);
	
	if ( bind(iServerSocket, (struct sockaddr*)&srvAddr, sizeof(srvAddr)) < 0 ) {
		printf("Failed to bind socket\n");
#ifdef _WIN32
		closesocket(iServerSocket);
		WSACleanup();
#else
		close(iServerSocket);
#endif
		return;
	}
	
	// Listen for connections
	// 监听连接
	if ( listen(iServerSocket, 10) < 0 ) {
		printf("Failed to listen on socket\n");
#ifdef _WIN32
		closesocket(iServerSocket);
		WSACleanup();
#else
		close(iServerSocket);
#endif
		return;
	}
	
	printf("HTTP Server listening on http://localhost:8080\n");
	printf("Static files served from ./www/ directory\n");
	printf("API endpoints:\n");
	printf("  GET  /api/hello    - Hello world\n");
	printf("  POST /api/echo     - Echo request body\n");
	printf("  GET  /api/status   - Server status\n");
	printf("Press Ctrl+C to stop server\n\n");
	
	// Create www directory if it doesn't exist
	// 如果不存在则创建 www 目录
	if ( !xrtDirExists("./www") ) {
		xrtDirCreate("./www");
		printf("Created ./www/ directory for static files\n");
	}
	
	// Set up signal handler
	// 设置信号处理器
#ifndef _WIN32
	signal(SIGINT, HttpSignalHandler);
#endif
	
	// Main server loop
	// 主服务器循环
	while ( gbServerRunning ) {
		struct sockaddr_in cliAddr = {0};
		socklen_t iCliLen = sizeof(cliAddr);
		
		// Accept client connection
		// 接受客户端连接
#ifdef _WIN32
		int iClientSocket = accept(iServerSocket, (struct sockaddr*)&cliAddr, &iCliLen);
#else
		int iClientSocket = accept(iServerSocket, (struct sockaddr*)&cliAddr, &iCliLen);
#endif
		
		if ( iClientSocket < 0 ) {
			continue;
		}
		
		// Get client info
		// 获取客户端信息
		char sClientIP[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &cliAddr.sin_addr, sClientIP, INET_ADDRSTRLEN);
		int iClientPort = ntohs(cliAddr.sin_port);
		
		// Receive HTTP request
		// 接收 HTTP 请求
		char sBuffer[4096];
		int iBytesReceived = recv(iClientSocket, sBuffer, sizeof(sBuffer) - 1, 0);
		
		if ( iBytesReceived > 0 ) {
			sBuffer[iBytesReceived] = '\0';
			
			// Parse request
			// 解析请求
			HttpRequest* pReq = ParseHttpRequest(sBuffer);
			
			if ( pReq->sMethod && pReq->sPath ) {
				printf("[%s:%d] %s %s\n", sClientIP, iClientPort, pReq->sMethod, pReq->sPath);
				
				HttpResponse* pResp = NULL;
				
				// Route request
				// 路由请求
				if ( strncmp(pReq->sPath, "/api/", 5) == 0 ) {
					// API route
					// API 路由
					pResp = HandleApiRoute(pReq->sPath, pReq->sMethod, pReq->sBody);
				} else {
					// Static file route
					// 静态文件路由
					if ( strcmp(pReq->sPath, "/") == 0 ) {
						pReq->sPath = "/index.html";  // Default page
					}
					pResp = ServeStaticFile(pReq->sPath);
				}
				
				// Send response
				// 发送响应
				if ( pResp ) {
					str sResponse = FormatHttpResponse(pResp);
					send(iClientSocket, sResponse, strlen(sResponse), 0);
					xrtFree(sResponse);
					
					// Cleanup response
					// 清理响应
					xrtFree(pResp->sHeaders);
					xrtFree(pResp->sBody);
					xrtFree(pResp);
				}
			}
			
			// Cleanup request
			// 清理请求
			if ( pReq->sMethod ) xrtFree(pReq->sMethod);
			if ( pReq->sPath ) xrtFree(pReq->sPath);
			if ( pReq->sVersion ) xrtFree(pReq->sVersion);
			if ( pReq->sHeaders ) xrtFree(pReq->sHeaders);
			if ( pReq->sBody ) xrtFree(pReq->sBody);
			xrtFree(pReq);
		}
		
		// Close client connection
		// 关闭客户端连接
#ifdef _WIN32
		closesocket(iClientSocket);
#else
		close(iClientSocket);
#endif
	}
	
	// Cleanup
	// 清理
	printf("Closing HTTP server...\n");
#ifdef _WIN32
	closesocket(iServerSocket);
	WSACleanup();
#else
	close(iServerSocket);
#endif
}

// HTTP Client implementation
// HTTP 客户端实现
void RunHttpClient(str sUrl, str sMethod, str sData)
{
	printf("=== HTTP Client ===\n");
	printf("=== HTTP 客户端 ===\n");
	
	// Parse URL (simple implementation)
	// 解析 URL（简单实现）
	if ( strncmp(sUrl, "http://", 7) != 0 ) {
		printf("Only http:// URLs supported\n");
		return;
	}
	
	str sHostPath = sUrl + 7;
	str sPath = strchr(sHostPath, '/');
	if ( !sPath ) {
		sPath = "/";
	} else {
		*(str)sPath = '\0';
		sPath++;
	}
	
	str sHost = sHostPath;
	int iPort = 80;
	
	// Check for port in host
	// 检查主机中的端口
	str sPortColon = strchr(sHost, ':');
	if ( sPortColon ) {
		*sPortColon = '\0';
		iPort = atoi(sPortColon + 1);
	}
	
	printf("Request: %s %s:%d%s\n", sMethod, sHost, iPort, sPath[0] ? sPath - 1 : "/");
	
#ifdef _WIN32
	WSADATA wsaData;
	if ( WSAStartup(MAKEWORD(2, 2), &wsaData) != 0 ) {
		printf("Failed to initialize Winsock\n");
		return;
	}
#endif
	
	// Create client socket
	// 创建客户端套接字
	int iClientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if ( iClientSocket < 0 ) {
		printf("Failed to create client socket\n");
#ifdef _WIN32
		WSACleanup();
#endif
		return;
	}
	
	// Connect to server
	// 连接到服务器
	struct sockaddr_in srvAddr = {0};
	srvAddr.sin_family = AF_INET;
	srvAddr.sin_port = htons(iPort);
	
#ifdef _WIN32
	inet_pton(AF_INET, sHost, &srvAddr.sin_addr);
#else
	struct hostent* pHost = gethostbyname(sHost);
	if ( pHost ) {
		memcpy(&srvAddr.sin_addr, pHost->h_addr_list[0], pHost->h_length);
	} else {
		inet_aton(sHost, &srvAddr.sin_addr);
	}
#endif
	
	if ( connect(iClientSocket, (struct sockaddr*)&srvAddr, sizeof(srvAddr)) < 0 ) {
		printf("Failed to connect to server\n");
#ifdef _WIN32
		closesocket(iClientSocket);
		WSACleanup();
#else
		close(iClientSocket);
#endif
		return;
	}
	
	// Build HTTP request
	// 构建 HTTP 请求
	str sRequest;
	if ( strcmp(sMethod, "POST") == 0 && sData ) {
		sRequest = xrtFormat(
			"%s /%s HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: %zu\r\n"
			"Connection: close\r\n"
			"\r\n"
			"%s",
			sMethod, sPath, sHost, strlen(sData), sData
		);
	} else {
		sRequest = xrtFormat(
			"%s /%s HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Connection: close\r\n"
			"\r\n",
			sMethod, sPath, sHost
		);
	}
	
	// Send request
	// 发送请求
	printf("Sending request...\n");
	send(iClientSocket, sRequest, strlen(sRequest), 0);
	xrtFree(sRequest);
	
	// Receive response
	// 接收响应
	char sResponse[8192];
	int iBytesReceived = recv(iClientSocket, sResponse, sizeof(sResponse) - 1, 0);
	
	if ( iBytesReceived > 0 ) {
		sResponse[iBytesReceived] = '\0';
		printf("Response received (%d bytes):\n", iBytesReceived);
		printf("-----\n%s-----\n", sResponse);
	}
	
	// Cleanup
	// 清理
#ifdef _WIN32
	closesocket(iClientSocket);
	WSACleanup();
#else
	close(iClientSocket);
#endif
}

// Test API endpoints
// 测试 API 端点
void TestApiEndpoints()
{
	printf("=== API Endpoint Tests ===\n");
	printf("=== API 端点测试 ===\n");
	
	// These would typically be run against a running server
	// 这些通常针对正在运行的服务器执行
	printf("Test commands (run server first):\n");
	printf("  Client GET /api/hello:\n");
	printf("    network_http_client_server.exe client http://localhost:8080/api/hello GET\n\n");
	
	printf("  Client POST /api/echo:\n");
	printf("    network_http_client_server.exe client http://localhost:8080/api/echo POST \"{\\\"test\\\":\\\"data\\\"}\"\n\n");
	
	printf("  Client GET /api/status:\n");
	printf("    network_http_client_server.exe client http://localhost:8080/api/status GET\n\n");
	
	// Create sample static files
	// 创建示例静态文件
	if ( !xrtDirExists("./www") ) {
		xrtDirCreate("./www");
	}
	
	// Create index.html
	// 创建 index.html
	str sIndexHtml = 
		"<!DOCTYPE html>\n"
		"<html>\n"
		"<head>\n"
		"    <title>XRT HTTP Server</title>\n"
		"</head>\n"
		"<body>\n"
		"    <h1>Welcome to XRT HTTP Server</h1>\n"
		"    <p>This is a static HTML file served by the XRT HTTP server.</p>\n"
		"    <h2>Available API Endpoints:</h2>\n"
		"    <ul>\n"
		"        <li>GET /api/hello - Hello world message</li>\n"
		"        <li>POST /api/echo - Echo request body</li>\n"
		"        <li>GET /api/status - Server status information</li>\n"
		"    </ul>\n"
		"</body>\n"
		"</html>\n";
	
	xrtFileWriteAll("./www/index.html", sIndexHtml, strlen(sIndexHtml), XRT_CP_UTF8);
	printf("Created ./www/index.html\n");
	xrtFree(sIndexHtml);
}

int main(int argc, char* argv[])
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT HTTP Client-Server Demo\n");
	printf("XRT HTTP 客户端-服务器演示\n");
	printf("===========================\n\n");
	
	// Parse command line arguments
	// 解析命令行参数
	if ( argc > 1 ) {
		if ( strcmp(argv[1], "server") == 0 ) {
			// Run HTTP server
			// 运行 HTTP 服务器
			RunHttpServer();
		} else if ( strcmp(argv[1], "client") == 0 ) {
			// Run HTTP client
			// 运行 HTTP 客户端
			if ( argc >= 3 ) {
				str sUrl = argv[2];
				str sMethod = (argc >= 4) ? argv[3] : "GET";
				str sData = (argc >= 5) ? argv[4] : NULL;
				RunHttpClient(sUrl, sMethod, sData);
			} else {
				printf("Usage: %s client <url> [GET|POST] [data]\n", argv[0]);
				printf("Example: %s client http://localhost:8080/api/hello GET\n", argv[0]);
			}
		} else if ( strcmp(argv[1], "test") == 0 ) {
			// Test API endpoints
			// 测试 API 端点
			TestApiEndpoints();
		} else {
			printf("Usage: %s [server|client|test]\n", argv[0]);
			printf("  server - Run HTTP server on port 8080\n");
			printf("  client - Run HTTP client (requires URL and method)\n");
			printf("  test   - Create test files and show usage examples\n");
		}
	} else {
		// Show help
		// 显示帮助
		printf("XRT HTTP Client-Server Demo\n");
		printf("Commands:\n");
		printf("  server - Start HTTP server\n");
		printf("  client - Make HTTP requests\n");
		printf("  test   - Setup test environment\n");
		printf("Run with argument for specific mode\n");
	}
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}