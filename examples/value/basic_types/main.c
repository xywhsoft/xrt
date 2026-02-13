/**
 * @file main.c
 * @brief Basic Value Types Example - xvoCreate* functions
 *        基本值类型示例 - xvoCreate* 函数
 * 
 * This example demonstrates:
 * - Creating various value types (null, bool, int, float, text, time, point)
 * - Checking value types
 * - Getting values
 * 
 * 本示例演示：
 * - 创建各种值类型（null、bool、int、float、text、time、point）
 * - 检查值类型
 * - 获取值
 * 
 * Build: tcc main.c -o ../../bin/value_basic_types.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_basic_types(void)
{
	printf("=== Test: Basic Value Types ===\n");
	printf("=== 测试：基本值类型 ===\n");
	
	xvalue vNull = xvoCreateNull();
	xvalue vBool = xvoCreateBool(true);
	xvalue vInt = xvoCreateInt(12345);
	xvalue vFloat = xvoCreateFloat(3.14159);
	xvalue vText = xvoCreateText("Hello, World!", 13, false);
	
	printf("Created values:\n");
	printf("创建的值:\n");
	printf("  vNull: type=%d\n", vNull->Type);
	printf("  vBool: type=%d, value=%s\n", vBool->Type, vBool->vBool ? "true" : "false");
	printf("  vInt: type=%d, value=%lld\n", vInt->Type, (long long)vInt->vInt);
	printf("  vFloat: type=%d, value=%.5f\n", vFloat->Type, vFloat->vFloat);
	printf("  vText: type=%d, value=%s\n", vText->Type, vText->vText);
	
	xvoUnref(vNull);
	xvoUnref(vBool);
	xvoUnref(vInt);
	xvoUnref(vFloat);
	xvoUnref(vText);
}

void test_time_value(void)
{
	printf("\n=== Test: Time Value ===\n");
	printf("=== 测试：时间值 ===\n");
	
	xvalue vTime = xvoCreateTimeSerial(2024, 6, 15, 14, 30, 45);
	printf("Created time from serial: year=2024, month=6, day=15\n");
	printf("从序列创建时间: 年=2024, 月=6, 日=15\n");
	printf("  Type: %d\n", vTime->Type);
	
	xvoUnref(vTime);
}

void test_point_value(void)
{
	printf("\n=== Test: Point Value ===\n");
	printf("=== 测试：点值 ===\n");
	
	int point[] = {100, 200};
	xvalue vPoint = xvoCreatePoint(point);
	printf("Created point: x=%d, y=%d\n", point[0], point[1]);
	printf("创建点: x=%d, y=%d\n", point[0], point[1]);
	printf("  Type: %d\n", vPoint->Type);
	
	xvoUnref(vPoint);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Basic Types Example / 基本类型示例\n");
	printf("========================================\n");
	
	test_basic_types();
	test_time_value();
	test_point_value();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
