/*
 * 范例：logging/file —— 滚动文件 Sink：自定义格式器与轮转策略
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtLogFileOptionsInit  文件选项（路径/轮转大小/备份数）
 *   xrtLogFile             创建滚动文件 Sink（可挂自定义格式器）
 *   xlogwriteproc          格式器输出回调（分片直写，零拼接缓冲）
 *   xrtLogSinkSubmit       直接向 Sink 提交（不经 Logger 分发）
 * 模块宏：XRT_MODULE_LOGGER（依赖 FILE）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/logging/file/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   wrote example_logger_file.log
 *
 * 滚动策略：单文件到 MaxBytes（本例 1MB）即轮转，
 *   保留最近 BackupCount 份（.1 .2 .3），防止日志吃满磁盘。
 * 格式器协议：输出不返回字符串而是回调 pWrite 分片推送——
 *   文件层直接落盘，不构造整行缓冲；
 *   底层协议不依赖 TEXT/JSON 模块（那两个是可选上层）。
 */

#include <xrt.h>

#include <stdio.h>
#include <string.h>



/* 自定义格式器：只写消息 + 换行（最简协议演示）。 */
static bool exampleLogFileFormat(
	const xlogrecord* pRecord,
	xlogwriteproc pWrite,
	ptr pWriteData,
	ptr pUserData
)
{
	(void)pUserData;
	return
		pWrite(
			(xbytesview){
				(cbytes)pRecord->Message.Data,
				pRecord->Message.Size
			},
			pWriteData
		) &&
		pWrite(XRT_BYTES_LITERAL("\n"), pWriteData);
}



int main(void)
{
	xlogfileoptions Options;
	xlogfileconfig Config;
	xlogrecord Record;
	xlogsink* pSink;

	if ( !xrtLogFileOptionsInit(&Options, "example_logger_file.log") ) {
		return 1;
	}
	Options.MaxBytes = 1024u * 1024u;   /* 1MB 轮转 */
	Options.BackupCount = 3u;           /* 保留 3 份历史 */

	/* 配置 = 文件选项 + 自定义格式器（NULL 则用内置默认）。 */
	memset(&Config, 0, sizeof(Config));
	Config.Options = Options;
	Config.Format = exampleLogFileFormat;
	pSink = xrtLogFile(&Config);
	if ( pSink == NULL ) {
		return 2;
	}

	/* 直接提交：Sink 可独立于 Logger 使用（简单场景零层级）。 */
	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = XRT_STR_LITERAL("file sink ready");
	if ( xrtLogSinkSubmit(pSink, &Record) != XLOG_RESULT_WRITTEN ) {
		xrtLogSinkFree(pSink);
		return 3;
	}
	xrtLogSinkFree(pSink);
	printf("wrote example_logger_file.log\n");
	return 0;
}
