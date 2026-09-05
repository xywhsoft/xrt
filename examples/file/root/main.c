#include <stdio.h>
#include <string.h>

#include <xrt.h>



/*
 * 范例：file/root —— 沙箱根：不可信相对名的安全文件操作
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtRootOpen / OpenIn      打开目录句柄（可嵌套）
 *   xrtRootDirCreate / Remove 在根内建/删目录
 *   xrtRootFileOpen           在根内打开文件（相对名）
 *   xrtRootClose              关闭根
 * 模块宏：XRT_MODULE_FILE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/file/root/main.c -lws2_32 -liphlpapi
 * 用法：
 *   root [name]（默认 message.txt）
 * 预期输出：
 *   stored and removed .xrt-root-example-data/message.txt
 *
 * 沙箱语义：根内操作只接受相对名，../ 穿越、绝对路径
 *   一律拒绝——解压上传包、插件资源访问的第一道防线
 *   （词法校验 path/safe 之外的系统级保障）。
 *   根句柄在目录被改名/移动后依然有效（不靠路径字符串）。
 */


/* 使用目录能力安全处理不可信的相对文件名。 */
int main(int argc, char** argv)
{
	cstr sName = argc > 1 ? argv[1] : "message.txt";
	const cstr sDirectory = ".xrt-root-example-data";
	xfileoptions Options;
	xfile File;
	xroot Parent;
	xroot Root;

	Parent = xrtRootOpen(".");
	if ( Parent == NULL ) {
		fprintf(stderr, "%s\n", xrtErrorMessage(xrtGetError()));
		return 1;
	}
	if ( !xrtRootDirCreate(Parent, sDirectory, 0700u) ) {
		fprintf(stderr, "%s\n", xrtErrorMessage(xrtGetError()));
		(void)xrtRootClose(Parent);
		return 1;
	}
	Root = xrtRootOpenIn(Parent, sDirectory);
	if ( Root == NULL ) {
		fprintf(stderr, "%s\n", xrtErrorMessage(xrtGetError()));
		(void)xrtRootRemove(Parent, sDirectory);
		(void)xrtRootClose(Parent);
		return 1;
	}
	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE;
	File = xrtRootFileOpen(Root, sName, &Options);
	if ( File == NULL ) {
		fprintf(stderr, "%s\n", xrtErrorMessage(xrtGetError()));
		(void)xrtRootClose(Root);
		(void)xrtRootRemove(Parent, sDirectory);
		(void)xrtRootClose(Parent);
		return 1;
	}
	if ( !xrtWriteFull(File, "stored inside root\n", 19u, NULL) ||
		 !xrtClose(File) || !xrtRootRemove(Root, sName) ||
		 !xrtRootClose(Root) ||
		 !xrtRootRemove(Parent, sDirectory) ||
		 !xrtRootClose(Parent) ) {
		fprintf(stderr, "%s\n", xrtErrorMessage(xrtGetError()));
		return 1;
	}
	printf("stored and removed %s/%s\n", sDirectory, sName);
	return 0;
}
