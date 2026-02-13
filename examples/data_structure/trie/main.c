/*
 * XRT Example - Trie (Prefix Tree)
 * XRT 范例 - 字典树（前缀树）
 *
 * Description / 说明:
 *   EN: Demonstrates trie data structure implementation for efficient string storage
 *       and prefix-based searching. Includes insertion, deletion, prefix matching,
 *       autocomplete functionality, and memory optimization techniques.
 *       Shows applications in dictionary implementation and IP routing.
 *   CN: 演示字典树数据结构实现，用于高效的字符串存储和基于前缀的搜索。
 *       包括插入、删除、前缀匹配、自动完成功能和内存优化技术。
 *       展示在字典实现和IP路由中的应用。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/data_structure_trie.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/data_structure_trie -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Alphabet-based trie nodes
 *   - Prefix matching and autocomplete
 *   - Memory-efficient node sharing
 *   - String dictionary operations
 *   - Word frequency counting
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <time.h>

#define TRIE_ALPHABET_SIZE 26  // For lowercase English letters / 用于小写英文字母

// Trie node structure
// 字典树节点结构
typedef struct TrieNode {
	struct TrieNode* apChildren[TRIE_ALPHABET_SIZE];  // Children nodes / 子节点
	int bIsEndOfWord;	// Marks end of word / 标记单词结尾
	int iWordCount;		// Number of words passing through this node / 通过此节点的单词数
	int iFrequency;		// Word frequency (for autocomplete) / 单词频率（用于自动完成）
} TrieNode;

// Trie structure
// 字典树结构
typedef struct {
	TrieNode* pRoot;
	size_t iWordCount;
} Trie;

// Create new trie node
// 创建新字典树节点
TrieNode* TrieNodeCreate()
{
	TrieNode* pNode = xrtMalloc(sizeof(TrieNode));
	memset(pNode, 0, sizeof(TrieNode));
	return pNode;
}

// Create trie
// 创建字典树
Trie* TrieCreate()
{
	Trie* pTrie = xrtMalloc(sizeof(Trie));
	pTrie->pRoot = TrieNodeCreate();
	pTrie->iWordCount = 0;
	return pTrie;
}

// Destroy trie node recursively
// 递归销毁字典树节点
void TrieNodeDestroy(TrieNode* pNode)
{
	if ( pNode ) {
		for ( int i = 0; i < TRIE_ALPHABET_SIZE; i++ ) {
			if ( pNode->apChildren[i] ) {
				TrieNodeDestroy(pNode->apChildren[i]);
			}
		}
		xrtFree(pNode);
	}
}

// Destroy trie
// 销毁字典树
void TrieDestroy(Trie* pTrie)
{
	if ( pTrie ) {
		TrieNodeDestroy(pTrie->pRoot);
		xrtFree(pTrie);
	}
}

// Convert character to index
// 将字符转换为索引
int CharToIndex(char c)
{
	if ( c >= 'a' && c <= 'z' ) {
		return c - 'a';
	} else if ( c >= 'A' && c <= 'Z' ) {
		return c - 'A';  // Case insensitive / 大小写不敏感
	}
	return -1;  // Invalid character / 无效字符
}

// Insert word into trie
// 向字典树插入单词
void TrieInsert(Trie* pTrie, str sWord)
{
	TrieNode* pNode = pTrie->pRoot;
	
	while ( *sWord ) {
		int iIndex = CharToIndex(*sWord);
		if ( iIndex == -1 ) {
			sWord++;  // Skip invalid characters / 跳过无效字符
			continue;
		}
		
		if ( !pNode->apChildren[iIndex] ) {
			pNode->apChildren[iIndex] = TrieNodeCreate();
		}
		
		pNode->apChildren[iIndex]->iWordCount++;
		pNode = pNode->apChildren[iIndex];
		sWord++;
	}
	
	if ( !pNode->bIsEndOfWord ) {
		pNode->bIsEndOfWord = 1;
		pNode->iFrequency = 1;
		pTrie->iWordCount++;
	} else {
		pNode->iFrequency++;  // Increment frequency for repeated words / 重复单词增加频率
	}
}

// Search for exact word
// 搜索确切单词
int TrieSearch(Trie* pTrie, str sWord)
{
	TrieNode* pNode = pTrie->pRoot;
	
	while ( *sWord ) {
		int iIndex = CharToIndex(*sWord);
		if ( iIndex == -1 || !pNode->apChildren[iIndex] ) {
			return 0;
		}
		pNode = pNode->apChildren[iIndex];
		sWord++;
	}
	
	return pNode->bIsEndOfWord;
}

// Check if prefix exists
// 检查前缀是否存在
int TrieStartsWith(Trie* pTrie, str sPrefix)
{
	TrieNode* pNode = pTrie->pRoot;
	
	while ( *sPrefix ) {
		int iIndex = CharToIndex(*sPrefix);
		if ( iIndex == -1 || !pNode->apChildren[iIndex] ) {
			return 0;
		}
		pNode = pNode->apChildren[iIndex];
		sPrefix++;
	}
	
	return 1;  // Prefix exists / 前缀存在
}

// Delete word from trie
// 从字典树删除单词
int TrieDeleteHelper(TrieNode* pNode, str sWord, size_t iDepth)
{
	if ( !pNode ) {
		return 0;
	}
	
	if ( iDepth == strlen(sWord) ) {
		if ( pNode->bIsEndOfWord ) {
			pNode->bIsEndOfWord = 0;
			pNode->iFrequency = 0;
			return 1;  // Successfully deleted / 成功删除
		}
		return 0;  // Word not found / 未找到单词
	}
	
	int iIndex = CharToIndex(sWord[iDepth]);
	if ( iIndex == -1 ) {
		return 0;
	}
	
	int iDeleted = TrieDeleteHelper(pNode->apChildren[iIndex], sWord, iDepth + 1);
	
	if ( iDeleted ) {
		pNode->apChildren[iIndex]->iWordCount--;
		
		// Remove child node if it's no longer needed
		// 如果不再需要则移除子节点
		int bHasChildren = 0;
		for ( int i = 0; i < TRIE_ALPHABET_SIZE; i++ ) {
			if ( pNode->apChildren[iIndex]->apChildren[i] ) {
				bHasChildren = 1;
				break;
			}
		}
		
		if ( !pNode->apChildren[iIndex]->bIsEndOfWord && !bHasChildren ) {
			xrtFree(pNode->apChildren[iIndex]);
			pNode->apChildren[iIndex] = NULL;
		}
	}
	
	return iDeleted;
}

int TrieDelete(Trie* pTrie, str sWord)
{
	int iResult = TrieDeleteHelper(pTrie->pRoot, sWord, 0);
	if ( iResult ) {
		pTrie->iWordCount--;
	}
	return iResult;
}

// Collect all words with given prefix
// 收集具有给定前缀的所有单词
void TrieCollectWords(TrieNode* pNode, str sPrefix, str* psResults, size_t* piCount, size_t iMaxResults)
{
	if ( *piCount >= iMaxResults ) {
		return;
	}
	
	if ( pNode->bIsEndOfWord ) {
		psResults[*piCount] = xrtCopyStr(sPrefix, strlen(sPrefix) + 1);
		(*piCount)++;
	}
	
	for ( int i = 0; i < TRIE_ALPHABET_SIZE; i++ ) {
		if ( pNode->apChildren[i] ) {
			char sNewPrefix[256];
			strcpy(sNewPrefix, sPrefix);
			sNewPrefix[strlen(sPrefix)] = 'a' + i;
			sNewPrefix[strlen(sPrefix) + 1] = '\0';
			
			TrieCollectWords(pNode->apChildren[i], sNewPrefix, psResults, piCount, iMaxResults);
		}
	}
}

// Autocomplete function
// 自动完成函数
size_t TrieAutocomplete(Trie* pTrie, str sPrefix, str* psResults, size_t iMaxResults)
{
	TrieNode* pNode = pTrie->pRoot;
	
	// Navigate to prefix node
	// 导航到前缀节点
	while ( *sPrefix ) {
		int iIndex = CharToIndex(*sPrefix);
		if ( iIndex == -1 || !pNode->apChildren[iIndex] ) {
			return 0;  // Prefix not found / 未找到前缀
		}
		pNode = pNode->apChildren[iIndex];
		sPrefix++;
	}
	
	// Collect all words with this prefix
	// 收集所有具有此前缀的单词
	size_t iCount = 0;
	char sBuffer[256];
	strcpy(sBuffer, sPrefix - strlen(sPrefix));  // Get original prefix / 获取原始前缀
	TrieCollectWords(pNode, sBuffer, psResults, &iCount, iMaxResults);
	
	return iCount;
}

// Print trie structure (simplified)
// 打印字典树结构（简化）
void TriePrintNode(TrieNode* pNode, int iLevel, char cParentChar)
{
	if ( pNode->bIsEndOfWord ) {
		for ( int i = 0; i < iLevel; i++ ) {
			printf("  ");
		}
		printf("'%c' (END, freq: %d)\n", cParentChar, pNode->iFrequency);
	}
	
	for ( int i = 0; i < TRIE_ALPHABET_SIZE; i++ ) {
		if ( pNode->apChildren[i] ) {
			for ( int j = 0; j < iLevel; j++ ) {
				printf("  ");
			}
			printf("'%c' ->\n", 'a' + i);
			TriePrintNode(pNode->apChildren[i], iLevel + 1, 'a' + i);
		}
	}
}

void TriePrint(Trie* pTrie)
{
	printf("Trie structure:\n");
	TriePrintNode(pTrie->pRoot, 0, '*');
}

// Count total nodes in trie
// 计算字典树中的总节点数
size_t TrieCountNodes(TrieNode* pNode)
{
	if ( !pNode ) {
		return 0;
	}
	
	size_t iCount = 1;  // Count current node / 计算当前节点
	for ( int i = 0; i < TRIE_ALPHABET_SIZE; i++ ) {
		iCount += TrieCountNodes(pNode->apChildren[i]);
	}
	
	return iCount;
}

// Test trie operations
// 测试字典树操作
void TestTrieOperations()
{
	printf("=== Trie Operations Test ===\n");
	printf("=== 字典树操作测试 ===\n");
	
	Trie* pTrie = TrieCreate();
	
	// Test insertions
	// 测试插入
	str arrWords[] = {"apple", "app", "application", "apply", "apt", "aptitude"};
	size_t iWordCount = sizeof(arrWords) / sizeof(arrWords[0]);
	
	printf("Inserting words:\n");
	for ( size_t i = 0; i < iWordCount; i++ ) {
		TrieInsert(pTrie, arrWords[i]);
		printf("  Inserted: %s\n", arrWords[i]);
	}
	
	printf("\nTrie structure:\n");
	TriePrint(pTrie);
	
	// Test searches
	// 测试搜索
	printf("\nTesting exact searches:\n");
	str arrSearch[] = {"apple", "app", "apt", "orange", "application"};
	for ( int i = 0; i < 5; i++ ) {
		int iResult = TrieSearch(pTrie, arrSearch[i]);
		printf("  %s: %s\n", arrSearch[i], iResult ? "Found" : "Not found");
	}
	
	// Test prefix matching
	// 测试前缀匹配
	printf("\nTesting prefix matching:\n");
	str arrPrefixes[] = {"ap", "app", "apt", "or"};
	for ( int i = 0; i < 4; i++ ) {
		int iResult = TrieStartsWith(pTrie, arrPrefixes[i]);
		printf("  Prefix '%s': %s\n", arrPrefixes[i], iResult ? "Exists" : "Not found");
	}
	
	// Test autocomplete
	// 测试自动完成
	printf("\nTesting autocomplete:\n");
	str asResults[10];
	size_t iResultCount = TrieAutocomplete(pTrie, "ap", asResults, 10);
	
	printf("Words starting with 'ap':\n");
	for ( size_t i = 0; i < iResultCount; i++ ) {
		printf("  %s\n", asResults[i]);
		xrtFree(asResults[i]);
	}
	
	// Test deletions
	// 测试删除
	printf("\nTesting deletions:\n");
	TrieDelete(pTrie, "app");
	printf("After deleting 'app':\n");
	
	iResultCount = TrieAutocomplete(pTrie, "ap", asResults, 10);
	printf("Words starting with 'ap':\n");
	for ( size_t i = 0; i < iResultCount; i++ ) {
		printf("  %s\n", asResults[i]);
		xrtFree(asResults[i]);
	}
	
	// Statistics
	// 统计信息
	printf("\nTrie statistics:\n");
	printf("  Total words: %zu\n", pTrie->iWordCount);
	printf("  Total nodes: %zu\n", TrieCountNodes(pTrie->pRoot));
	
	// Cleanup
	// 清理
	TrieDestroy(pTrie);
}

// Test dictionary application
// 测试字典应用
void TestDictionaryApplication()
{
	printf("\n=== Dictionary Application Test ===\n");
	printf("=== 字典应用测试 ===\n");
	
	Trie* pDict = TrieCreate();
	
	// Build dictionary from text
	// 从文本构建字典
	str sText = "the quick brown fox jumps over the lazy dog the fox is quick";
	
	// Simple tokenization (split by spaces)
	// 简单分词（按空格分割）
	str sToken = strtok(xrtCopyStr(sText, strlen(sText) + 1), " ");
	while ( sToken ) {
		// Convert to lowercase for consistency
		// 转换为小写以保持一致
		for ( int i = 0; sToken[i]; i++ ) {
			if ( sToken[i] >= 'A' && sToken[i] <= 'Z' ) {
				sToken[i] = sToken[i] - 'A' + 'a';
			}
		}
		
		TrieInsert(pDict, sToken);
		sToken = strtok(NULL, " ");
	}
	
	// Test dictionary operations
	// 测试字典操作
	printf("Dictionary built from text: \"%s\"\n", sText);
	
	str arrLookup[] = {"the", "fox", "quick", "cat"};
	printf("\nWord lookup:\n");
	for ( int i = 0; i < 4; i++ ) {
		int iFound = TrieSearch(pDict, arrLookup[i]);
		printf("  %s: %s\n", arrLookup[i], iFound ? "In dictionary" : "Not in dictionary");
	}
	
	// Find words with prefix
	// 查找具有前缀的单词
	str asSuggestions[5];
	size_t iSuggestionCount = TrieAutocomplete(pDict, "th", asSuggestions, 5);
	
	printf("\nWords starting with 'th':\n");
	for ( size_t i = 0; i < iSuggestionCount; i++ ) {
		printf("  %s\n", asSuggestions[i]);
		xrtFree(asSuggestions[i]);
	}
	
	TrieDestroy(pDict);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Trie (Prefix Tree) Demo\n");
	printf("XRT 字典树（前缀树）演示\n");
	printf("=========================\n\n");
	
	// Run tests
	// 运行测试
	TestTrieOperations();
	TestDictionaryApplication();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}