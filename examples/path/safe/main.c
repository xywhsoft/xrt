/*
 * 范例：path/safe —— 安全入口校验：拒绝穿越与保留设备名
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtPathIsSafeEntry   一票否决式词法校验（解压/静态资源的门卫）
 * 模块宏：XRT_MODULE_PATH
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/path/safe/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   assets/icon.png: safe
 *   ../secret.txt: rejected
 *   CON.txt: rejected
 *
 * 校验规则（跨平台词法判定，不触文件系统）：
 *   - 绝对路径、.. 穿越、盘符 → 拒绝（防 Zip Slip / 路径穿越）；
 *   - Windows 保留设备名（CON/PRN/AUX/NUL/COM1...）→ 拒绝
 *     （CON.txt 在 Linux 合法，但入口校验面向不可信输入，
 *      按最严格平台统一把关，产物才可跨平台安全分发）。
 * 第二参数 bAllowDirs 控制是否允许子目录条目（本例 false 仅平铺）。
 * 用法：解包归档、静态文件服务映射用户输入前先过这道门。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	cstr arrEntries[] = {
		"assets/icon.png",     /* 合法相对条目 */
		"../secret.txt",       /* 穿越逃逸 */
		"CON.txt"              /* Windows 保留设备名 */
	};

	for ( size_t i = 0; i < sizeof(arrEntries) / sizeof(arrEntries[0]); i++ ) {
		printf("%s: %s\n", arrEntries[i],
			xrtPathIsSafeEntry(xrtStrView(arrEntries[i]), false) ?
			"safe" : "rejected");
	}
	return 0;
}
