#define XWS_MODULE_ALL
#include <xws.h>



/* 发布包消费者验证核心帧与高级连接声明可以从同一聚合头访问。 */
int main(void)
{
	xwsframe Frame;
	xwsconnconfig Connection;

	xrtWsFrameInit(&Frame);
	xrtWsConnConfigInit(&Connection);
	return (Frame.Opcode == XWS_OPCODE_CONTINUATION) &&
		(Connection.MessageLimit == XWS_CONN_MESSAGE_LIMIT_DEFAULT) ? 0 : 1;
}
