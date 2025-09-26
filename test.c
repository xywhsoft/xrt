


#include "xrt.h"
#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#endif



#include "test/test_base.h"
#include "test/test_charset.h"
#include "test/test_math.h"
#include "test/test_string.h"
#include "test/test_path.h"
#include "test/test_time.h"
#include "test/test_file.h"
#include "test/test_xid.h"
#include "test/test_network.h"
#include "test/test_other.h"
#include "test/test_array_ptr.h"
//#include "test/test_array_struct.h"





int main(int argc, char** argv)
{
	#if defined(_WIN32) || defined(_WIN64)
		SetConsoleOutputCP(65001);
	#endif
	
	xrtGlobalData* xCore = xrtInit();
	xCore->DebugMode = TRUE;
	printf("测试开始\n\n");
	
	
	
	/* Base 库测试 */
	// Test_Base(xCore);
	
	/* Charset 库测试 */
	// Test_Charset(xCore);
	
	/* Math 库测试 */
	// Test_Math(xCore);
	
	/* String 库测试 */
	// Test_String(xCore);
	
	/* Path 库测试 */
	// Test_Path(xCore);
	
	/* Time 库测试 */
	// Test_Time(xCore);
	
	/* File 库测试 */
	// Test_File(xCore);
	
	/* XID 库测试 */
	// Test_XID(xCore);
	
	/* 网络库测试 */
	// Test_Network(xCore);
	
	/* 指针数组测试 */
	Test_Array_Ptr(xCore);
	
	/* 自定义测试 */
	// Test_Other(xCore);
	
	
	
	printf("\n------------------------------------\n\n\n\n测试结束\n\n");
	xrtUnit();
	return 0;
}


