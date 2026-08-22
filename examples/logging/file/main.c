#include <xrt.h>

#include <stdio.h>
#include <string.h>



/* 自定义文件格式器演示底层协议不依赖文本或 JSON 模块。 */
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



/* 创建滚动文件 Sink 并提交一条记录。 */
int main(void)
{
	xlogfileoptions Options;
	xlogfileconfig Config;
	xlogrecord Record;
	xlogsink* pSink;

	if ( !xrtLogFileOptionsInit(&Options, "example_logger_file.log") ) {
		return 1;
	}
	Options.MaxBytes = 1024u * 1024u;
	Options.BackupCount = 3u;
	memset(&Config, 0, sizeof(Config));
	Config.Options = Options;
	Config.Format = exampleLogFileFormat;
	pSink = xrtLogFile(&Config);
	if ( pSink == NULL ) {
		return 2;
	}
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
