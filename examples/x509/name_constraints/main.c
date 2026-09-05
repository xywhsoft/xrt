#include <stdio.h>

#include <xrt.h>



/*
 * 范例：x509/name_constraints —— 名称约束扩展解析与子树遍历
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtX509NameConstraintsParse   DER → 允许/禁止子树
 *   xrtX509SubtreeRead            子树游标（Base = 通用名）
 * 模块宏：XRT_MODULE_X509
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/x509/name_constraints/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   type=2 size=12
 *
 * CA 的"辖区声明"：permittedSubtrees 限定下属证书只能
 *   给这些名字签发（本例 dNSName "example.test"，type=2）。
 *   路径验证（path 范例的 PathValidate）会用它逐级收窄
 *   合法名字空间——中间 CA 越权签 *.evil.com 会在此被拦。
 */


/* 演示解析并遍历一组独立 NameConstraints DER。 */
int main(void)
{
	static const uint8 Der[] = {
		0x30, 0x12, 0xA0, 0x10, 0x30, 0x0E, 0x82, 0x0C,
		'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 't', 'e', 's', 't'
	};
	xx509nameconstraints Constraints;
	xx509subtreecursor Cursor;
	xx509subtree Subtree;
	xx509result Result;

	if ( !xrtX509NameConstraintsParse(
		(xbytesview) { Der, sizeof(Der) }, &Constraints
	) ) {
		return 1;
	}
	Cursor = Constraints.Permitted;
	while ( (Result = xrtX509SubtreeRead(
		&Cursor, &Subtree
	)) == X509_VALUE ) {
		printf("type=%d size=%zu\n",
			(int)Subtree.Base.Type, Subtree.Base.Value.Size);
	}
	return Result == X509_DONE ? 0 : 1;
}
