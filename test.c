


#include "xrt.h"
#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#endif



#include "test/test_base.h"
#include "test/test_charset.h"
#include "test/test_os.h"
#include "test/test_math.h"
#include "test/test_string.h"
#include "test/test_path.h"
#include "test/test_time.h"
#include "test/test_file.h"
#include "test/test_thread.h"
#include "test/test_hash.h"
#include "test/test_network.h"
#include "test/test_xid.h"
#include "test/test_buffer.h"
#include "test/test_array_ptr.h"
#include "test/test_array_struct.h"
#include "test/test_bsmm.h"
#include "test/test_memunit.h"
#include "test/test_mempool_fs.h"
#include "test/test_stack_ptr.h"
#include "test/test_stack.h"
#include "test/test_dynstack_ptr.h"
#include "test/test_dynstack.h"
#include "test/test_llist.h"
#include "test/test_avltree.h"
#include "test/test_mempool.h"
#include "test/test_dict.h"
#include "test/test_list.h"
#include "test/test_value.h"
#include "test/test_json.h"
#include "test/test_template.h"
#include "test/test_other.h"



void OnError(str sError)
{
	printf("X Runtime Error : %s\n", sError);
}



int main(int argc, char** argv)
{
	#if defined(_WIN32) || defined(_WIN64)
		SetConsoleOutputCP(65001);
	#endif
	
	xrtGlobalData* xCore = xrtInit();
	xCore->OnError = OnError;
	printf("测试开始\n\n");
	
	
	
	/* Base 库测试 */
	// Test_Base(xCore);
	
	/* Charset 库测试 */
	// Test_Charset(xCore);
	
	/* OS 库测试 */
	// Test_OS(xCore);
	
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
	
	/* Thread 库测试 */
	// Test_Thread(xCore);
	
	/* Hash 库测试 */
	// Test_Hash(xCore);
	
	/* 网络库测试 */
	// Test_Network(xCore);
	
	/* XID 库测试 */
	// Test_XID(xCore);
	
	/* Buffer 库测试 */
	// Test_Buffer(xCore);
	
	/* 指针数组测试 */
	// Test_Array_Ptr(xCore);
	
	/* 结构体数组测试 */
	// Test_Array_Struct(xCore);
	
	/* 块结构内存管理器测试 */
	// Test_BSMM(xCore);
	
	/* 内存管理单元测试 */
	// Test_MemUnit(xCore);
	
	/* 固定大小内存池测试 */
	// Test_MemPoolFS(xCore);
	
	/* 静态指针栈测试 */
	// Test_Stack_Ptr(xCore);
	
	/* 静态结构体栈测试 */
	// Test_Stack(xCore);
	
	/* 动态指针栈测试 */
	// Test_DynStack_Ptr(xCore);
	
	/* 动态结构体栈测试 */
	// Test_DynStack(xCore);
	
	/* LList 库测试 */
	// Test_LList(xCore);
	
	/* AVLTree 库测试 */
	// Test_AVLTree(xCore);
	
	/* 内存池测试 */
	// Test_MemPool(xCore);
	
	/* Dict 测试 */
	// Test_Dict(xCore);
	
	/* List 测试 */
	// Test_List(xCore);
	
	/* Value 测试 */
	 Test_Value_Basic(xCore);
	
	/* JSON 测试 */
	// Test_JSON(xCore);
	
	/* Template 测试 */
	// Test_Template(xCore);
	
	/* 自定义测试 */
	// Test_Other(xCore);
	
	
	
	printf("\n------------------------------------\n\n\n\n测试结束\n\n");
	xrtUnit();
	return 0;
}


