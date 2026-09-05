/*
 * 范例：containers/avl —— 侵入式 AVL 树：零分配索引与有序迭代
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtAVLInit        初始化树（只是句柄，不分配任何东西）
 *   xrtAVLNodeInit    初始化内嵌节点（挂树前必须做一次）
 *   xrtAVLInsert      按比较函数插入；返回树内既有节点（重复键）
 *   xrtAVLIterBegin/Next  中序（升序）迭代，外置迭代器
 * 模块宏：XRT_MODULE_AVL
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/containers/avl/main.c -lws2_32 -liphlpapi
 * 预期输出（按 ID 升序，与插入顺序无关）：
 *   id=10 name=alpha
 *   id=20 name=beta
 *   id=30 name=gamma
 *
 * 侵入式 AVL 与 avl_tree（容器式）的区别：
 *   - 节点内嵌在业务对象里（Index 字段），插入/删除零分配；
 *   - 业务对象放在哪（栈、池、数组）由调用方决定，树只管链接；
 *   - 一个对象可挂多棵索引树（再加节点字段即可），
 *     "按 ID 一棵、按名称一棵"是常见用法。
 * 代价：比较函数要自己从节点反查宿主（见 exampleCompare）。
 */

#include <stdio.h>
#include <stddef.h>

#include <xrt.h>



/* 外部会话对象直接嵌入树节点，不产生额外分配。 */
typedef struct examplesession {
	int ID;
	const char* Name;
	xavlnode Index;          /* 侵入式链接：挂在 ID 索引树上 */
} examplesession;



/* 从侵入式链接恢复完整会话（节点地址 - 字段偏移）。 */
static examplesession* exampleSession(xavlnode* pNode)
{
	return (examplesession*)((bytes)pNode - offsetof(examplesession, Index));
}



/*
 * 比较函数：签名是 (查找键, 树节点, 用户数据)。
 * 节点先反查回宿主会话，再按 ID 比大小；
 * 返回 -1/0/1 三态（用减法式两步比较避免溢出）。
 */
static int exampleCompare(const void* pKey, const xavlnode* pNode, ptr pUserData)
{
	const examplesession* pSession = (const examplesession*)(
		(cbytes)pNode - offsetof(examplesession, Index)
	);
	int iID = *(const int*)pKey;

	(void)pUserData;
	return (iID > pSession->ID) - (iID < pSession->ID);
}



int main(void)
{
	xavl tSessions;          /* 树句柄：本身零分配 */
	xavliter tIterator;
	examplesession pSessions[] = {
		{ 30, "gamma", { 0 } },
		{ 10, "alpha", { 0 } },
		{ 20, "beta", { 0 } }
	};

	if ( !xrtAVLInit(&tSessions) ) {
		return 1;
	}

	/*
	 * 逐个挂树：NodeInit 清零链接 → Insert 按比较函数就位。
	 * Insert 返回非 NULL 表示"树里已有同键节点"（返回那个旧节点），
	 * 本例键唯一，返回 NULL 才算成功。
	 * 全程没有任何堆分配——树只重排已有对象的链接。
	 */
	for ( size_t i = 0; i < 3; i++ ) {
		xrtAVLNodeInit(&pSessions[i].Index);
		if (
			xrtAVLInsert(
				&tSessions,
				&pSessions[i].Index,
				&pSessions[i].ID,
				exampleCompare,
				NULL,
				NULL
			) == NULL
		) {
			return 2;
		}
	}

	/*
	 * 中序迭代：无论插入顺序 30,10,20，输出恒为升序 10,20,30——
	 * 这就是"索引"的价值：插入顺序与有序视图解耦。
	 */
	xrtAVLIterBegin(&tSessions, &tIterator);
	while ( true ) {
		xavlnode* pNode = xrtAVLIterNext(&tIterator);
		examplesession* pSession;

		if ( pNode == NULL ) {
			break;
		}
		pSession = exampleSession(pNode);
		printf("id=%d name=%s\n", pSession->ID, pSession->Name);
	}
	return 0;
}
