/*
 * 范例：containers/avl_tree —— 容器式 AVL 树：复制存储、点查与范围迭代
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtAVLTreeInit       按元素大小 + 比较函数初始化（容器自管存储）
 *   xrtAVLTreeAdd        复制式插入：键 + 元素值（返回树内元素指针）
 *   xrtAVLTreeFind       按键点查，返回可写指针
 *   xrtAVLTreeIterFrom   从指定键开始的范围（下界）迭代
 *   xrtAVLTreeIterNext   步进返回下一个元素指针
 *   xrtAVLTreeUnit       释放整棵树（元素随树释放）
 * 模块宏：XRT_MODULE_AVL
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/containers/avl_tree/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   id=20 timeout=2000
 *   range id=20 timeout=2000
 *   range id=30 timeout=3000
 *
 * 与侵入式 AVL（avl/main.c）的分工：
 *   avl_tree 是"黑盒容器"——元素被复制进树、由树统一释放，
 *   比较函数直接拿到对象（不用反查宿主），写起来最省心；
 *   avl 是"白盒索引"——零分配、对象存储归调用方。
 * 范围迭代（IterFrom）是树相对哈希容器的独有能力：
 *   "从 ID=20 起的全部配置"这类区间查询 O(log n + k)。
 */

#include <stdio.h>

#include <xrt.h>



/* 拥有型配置对象：整条结构会被复制进树内存储。 */
typedef struct exampleconfig {
	int ID;
	int Timeout;
} exampleconfig;



/* 按配置编号比较查找键与对象（容器式：直接拿对象指针）。 */
static int exampleCompare(const void* pKey, const void* pItem, ptr pUserData)
{
	int iID = *(const int*)pKey;
	int iItemID = ((const exampleconfig*)pItem)->ID;

	(void)pUserData;
	return (iID > iItemID) - (iID < iItemID);
}



int main(void)
{
	xavltree tConfigs;
	xavltreeiter tIterator;
	exampleconfig pInput[] = {
		{ 30, 3000 },
		{ 10, 1000 },
		{ 20, 2000 }
	};
	int iSearch = 20;

	/* 初始化即绑定元素大小与比较函数——之后树全权管理存储。 */
	if ( !xrtAVLTreeInit(&tConfigs, sizeof(exampleconfig), exampleCompare, NULL) ) {
		return 1;
	}

	/*
	 * 复制式插入：参数为 树/键/元素值/出参是否新建。
	 * 返回树内元素指针（NULL = 失败）；键已存在时返回旧元素。
	 */
	for ( size_t i = 0; i < 3; i++ ) {
		if ( xrtAVLTreeAdd(&tConfigs, &pInput[i].ID, &pInput[i], NULL) == NULL ) {
			xrtAVLTreeUnit(&tConfigs);
			return 2;
		}
	}

	/* 点查 ID=20：拿到树内元素的直接指针（可读写）。 */
	{
		exampleconfig* pConfig = (exampleconfig*)xrtAVLTreeFind(&tConfigs, &iSearch);

		if ( pConfig != NULL ) {
			printf("id=%d timeout=%d\n", pConfig->ID, pConfig->Timeout);
		}
	}

	/*
	 * 范围迭代：从 ID=20（含）开始到末尾——输出 20、30。
	 * 这等价于 SQL 的 WHERE id >= 20 ORDER BY id，
	 * 哈希结构做不到，树天然支持。
	 */
	if ( xrtAVLTreeIterFrom(&tConfigs, &iSearch, &tIterator) ) {
		exampleconfig* pConfig;

		while ( (pConfig = (exampleconfig*)xrtAVLTreeIterNext(&tIterator)) != NULL ) {
			printf("range id=%d timeout=%d\n", pConfig->ID, pConfig->Timeout);
		}
	}

	/* 整树释放：所有元素存储一并归还。 */
	xrtAVLTreeUnit(&tConfigs);
	return 0;
}
