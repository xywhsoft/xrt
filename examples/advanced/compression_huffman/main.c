/*
 * XRT Example - Huffman Compression
 * XRT 范例 - 霍夫曼压缩
 *
 * Description / 说明:
 *   EN: Demonstrates Huffman coding compression algorithm including frequency
 *       analysis, binary tree construction, code generation, and compression/decompression.
 *       Shows entropy encoding principles and lossless data compression techniques.
 *   CN: 演示霍夫曼编码压缩算法，包括频率分析、二叉树构建、编码生成以及压缩/解压缩。
 *       展示熵编码原理和无损数据压缩技术。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/advanced_compression_huffman.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/advanced_compression_huffman -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Frequency analysis and histogram
 *   - Huffman tree construction
 *   - Variable-length encoding
 *   - Bit-level compression
 *   - Lossless compression/decompression
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <time.h>

// Huffman tree node structure
// 霍夫曼树节点结构
typedef struct HuffmanNode {
	unsigned char ucChar;		// Character (for leaf nodes) / 字符（叶节点）
	int iFrequency;				// Frequency count / 频率计数
	struct HuffmanNode* pLeft;	// Left child / 左子节点
	struct HuffmanNode* pRight;	// Right child / 右子节点
} HuffmanNode;

// Bit buffer structure for compression
// 用于压缩的位缓冲区结构
typedef struct {
	unsigned char* pucBuffer;	// Output buffer / 输出缓冲区
	size_t iBufferSize;			// Buffer size / 缓冲区大小
	size_t iBitPosition;		// Current bit position / 当前位位置
	size_t iBytePosition;		// Current byte position / 当前字节位置
} BitBuffer;

// Huffman code structure
// 霍夫曼编码结构
typedef struct {
	unsigned int uiCode;		// Huffman code / 霍夫曼编码
	int iCodeLength;			// Code length in bits / 编码长度（位）
} HuffmanCode;

// Create Huffman node
// 创建霍夫曼节点
HuffmanNode* HuffmanNodeCreate(unsigned char ucChar, int iFrequency)
{
	HuffmanNode* pNode = xrtMalloc(sizeof(HuffmanNode));
	pNode->ucChar = ucChar;
	pNode->iFrequency = iFrequency;
	pNode->pLeft = NULL;
	pNode->pRight = NULL;
	return pNode;
}

// Destroy Huffman tree
// 销毁霍夫曼树
void HuffmanTreeDestroy(HuffmanNode* pNode)
{
	if ( pNode ) {
		HuffmanTreeDestroy(pNode->pLeft);
		HuffmanTreeDestroy(pNode->pRight);
		xrtFree(pNode);
	}
}

// Compare function for priority queue
// 优先队列的比较函数
int HuffmanNodeCompare(HuffmanNode* pNode1, HuffmanNode* pNode2)
{
	return pNode1->iFrequency - pNode2->iFrequency;
}

// Build frequency table
// 构建频率表
void BuildFrequencyTable(unsigned char* pucData, size_t iSize, int* piFreqTable)
{
	memset(piFreqTable, 0, 256 * sizeof(int));
	
	for ( size_t i = 0; i < iSize; i++ ) {
		piFreqTable[pucData[i]]++;
	}
}

// Build Huffman tree
// 构建霍夫曼树
HuffmanNode* BuildHuffmanTree(int* piFreqTable)
{
	// Simple array-based priority queue (for demonstration)
	// 简单的基于数组的优先队列（用于演示）
	HuffmanNode* apNodes[512];  // Max 256 characters + internal nodes / 最多256个字符+内部节点
	int iNodeCount = 0;
	
	// Create leaf nodes for characters with non-zero frequency
	// 为频率非零的字符创建叶节点
	for ( int i = 0; i < 256; i++ ) {
		if ( piFreqTable[i] > 0 ) {
			apNodes[iNodeCount++] = HuffmanNodeCreate((unsigned char)i, piFreqTable[i]);
		}
	}
	
	// Build tree using greedy algorithm
	// 使用贪心算法构建树
	while ( iNodeCount > 1 ) {
		// Find two nodes with minimum frequency
		// 找到两个频率最小的节点
		int iMin1 = 0, iMin2 = 1;
		if ( apNodes[iMin1]->iFrequency > apNodes[iMin2]->iFrequency ) {
			iMin1 = 1; iMin2 = 0;
		}
		
		for ( int i = 2; i < iNodeCount; i++ ) {
			if ( apNodes[i]->iFrequency < apNodes[iMin1]->iFrequency ) {
				iMin2 = iMin1;
				iMin1 = i;
			} else if ( apNodes[i]->iFrequency < apNodes[iMin2]->iFrequency ) {
				iMin2 = i;
			}
		}
		
		// Create internal node
		// 创建内部节点
		HuffmanNode* pParent = HuffmanNodeCreate(0, 
			apNodes[iMin1]->iFrequency + apNodes[iMin2]->iFrequency);
		pParent->pLeft = apNodes[iMin1];
		pParent->pRight = apNodes[iMin2];
		
		// Remove the two nodes and add the new parent
		// 移除两个节点并添加新父节点
		apNodes[iMin1] = pParent;
		apNodes[iMin2] = apNodes[--iNodeCount];
	}
	
	return apNodes[0];
}

// Generate Huffman codes
// 生成霍夫曼编码
void GenerateHuffmanCodes(HuffmanNode* pNode, HuffmanCode* paCodes, 
                         unsigned int uiCode, int iDepth)
{
	if ( pNode->pLeft == NULL && pNode->pRight == NULL ) {
		// Leaf node - store code
		// 叶节点 - 存储编码
		paCodes[pNode->ucChar].uiCode = uiCode;
		paCodes[pNode->ucChar].iCodeLength = iDepth;
	} else {
		// Internal node - traverse children
		// 内部节点 - 遍历子节点
		if ( pNode->pLeft ) {
			GenerateHuffmanCodes(pNode->pLeft, paCodes, uiCode << 1, iDepth + 1);
		}
		if ( pNode->pRight ) {
			GenerateHuffmanCodes(pNode->pRight, paCodes, (uiCode << 1) | 1, iDepth + 1);
		}
	}
}

// Create bit buffer
// 创建位缓冲区
BitBuffer* BitBufferCreate(size_t iInitialSize)
{
	BitBuffer* pBuffer = xrtMalloc(sizeof(BitBuffer));
	pBuffer->pucBuffer = xrtMalloc(iInitialSize);
	pBuffer->iBufferSize = iInitialSize;
	pBuffer->iBitPosition = 0;
	pBuffer->iBytePosition = 0;
	memset(pBuffer->pucBuffer, 0, iInitialSize);
	return pBuffer;
}

// Destroy bit buffer
// 销毁位缓冲区
void BitBufferDestroy(BitBuffer* pBuffer)
{
	if ( pBuffer ) {
		if ( pBuffer->pucBuffer ) {
			xrtFree(pBuffer->pucBuffer);
		}
		xrtFree(pBuffer);
	}
}

// Write bits to buffer
// 向缓冲区写入位
void BitBufferWriteBits(BitBuffer* pBuffer, unsigned int uiCode, int iLength)
{
	for ( int i = iLength - 1; i >= 0; i-- ) {
		// Expand buffer if needed
		// 如需要则扩展缓冲区
		if ( pBuffer->iBytePosition >= pBuffer->iBufferSize ) {
			pBuffer->iBufferSize *= 2;
			pBuffer->pucBuffer = xrtRealloc(pBuffer->pucBuffer, pBuffer->iBufferSize);
		}
		
		// Write bit
		// 写入位
		if ( (uiCode >> i) & 1 ) {
			pBuffer->pucBuffer[pBuffer->iBytePosition] |= (1 << (7 - pBuffer->iBitPosition));
		}
		
		pBuffer->iBitPosition++;
		if ( pBuffer->iBitPosition == 8 ) {
			pBuffer->iBitPosition = 0;
			pBuffer->iBytePosition++;
			if ( pBuffer->iBytePosition >= pBuffer->iBufferSize ) {
				pBuffer->iBufferSize *= 2;
				pBuffer->pucBuffer = xrtRealloc(pBuffer->pucBuffer, pBuffer->iBufferSize);
				memset(pBuffer->pucBuffer + pBuffer->iBytePosition, 0, 
				       pBuffer->iBufferSize - pBuffer->iBytePosition);
			}
		}
	}
}

// Compress data using Huffman coding
// 使用霍夫曼编码压缩数据
BitBuffer* HuffmanCompress(unsigned char* pucData, size_t iSize, HuffmanCode* paCodes)
{
	BitBuffer* pBuffer = BitBufferCreate(iSize);  // Initial estimate / 初始估计
	
	for ( size_t i = 0; i < iSize; i++ ) {
		unsigned char ucChar = pucData[i];
		BitBufferWriteBits(pBuffer, paCodes[ucChar].uiCode, paCodes[ucChar].iCodeLength);
	}
	
	return pBuffer;
}

// Decompress Huffman encoded data
// 解压缩霍夫曼编码数据
unsigned char* HuffmanDecompress(BitBuffer* pCompressed, size_t iOriginalSize, 
                                HuffmanNode* pTree, size_t* piDecompressedSize)
{
	unsigned char* pucDecompressed = xrtMalloc(iOriginalSize);
	size_t iOutputPos = 0;
	
	HuffmanNode* pCurrent = pTree;
	size_t iBytePos = 0;
	int iBitPos = 0;
	
	while ( iOutputPos < iOriginalSize && iBytePos < pCompressed->iBytePosition ) {
		// Read bit
		// 读取位
		int iBit = (pCompressed->pucBuffer[iBytePos] >> (7 - iBitPos)) & 1;
		iBitPos++;
		if ( iBitPos == 8 ) {
			iBitPos = 0;
			iBytePos++;
		}
		
		// Traverse tree
		// 遍历树
		if ( iBit == 0 ) {
			pCurrent = pCurrent->pLeft;
		} else {
			pCurrent = pCurrent->pRight;
		}
		
		// If we reach a leaf node
		// 如果到达叶节点
		if ( pCurrent->pLeft == NULL && pCurrent->pRight == NULL ) {
			pucDecompressed[iOutputPos++] = pCurrent->ucChar;
			pCurrent = pTree;  // Return to root / 返回根节点
		}
	}
	
	*piDecompressedSize = iOutputPos;
	return pucDecompressed;
}

// Calculate compression ratio
// 计算压缩比
double CalculateCompressionRatio(size_t iOriginalSize, size_t iCompressedSize)
{
	return (double)(iOriginalSize - iCompressedSize) / iOriginalSize * 100;
}

// Print frequency analysis
// 打印频率分析
void PrintFrequencyAnalysis(int* piFreqTable)
{
	printf("Character frequencies:\n");
	for ( int i = 0; i < 256; i++ ) {
		if ( piFreqTable[i] > 0 ) {
			if ( i >= 32 && i <= 126 ) {
				printf("  '%c': %d\n", i, piFreqTable[i]);
			} else {
				printf("  0x%02X: %d\n", i, piFreqTable[i]);
			}
		}
	}
}

// Test Huffman compression
// 测试霍夫曼压缩
void TestHuffmanCompression()
{
	printf("=== Huffman Compression Test ===\n");
	printf("=== 霍夫曼压缩测试 ===\n");
	
	// Test data with repetitive patterns
	// 具有重复模式的测试数据
	str sTestData = "AAAAAAAAAAABBBBBBBBBBCCCCCCCCCCDDDDDDDDEEEEEEEE";
	size_t iDataSize = strlen(sTestData);
	
	printf("Original data: %s\n", sTestData);
	printf("Original size: %zu bytes\n", iDataSize);
	
	// Build frequency table
	// 构建频率表
	int aiFreqTable[256];
	BuildFrequencyTable((unsigned char*)sTestData, iDataSize, aiFreqTable);
	
	PrintFrequencyAnalysis(aiFreqTable);
	
	// Build Huffman tree
	// 构建霍夫曼树
	HuffmanNode* pTree = BuildHuffmanTree(aiFreqTable);
	
	// Generate Huffman codes
	// 生成霍夫曼编码
	HuffmanCode aCodes[256];
	memset(aCodes, 0, sizeof(aCodes));
	GenerateHuffmanCodes(pTree, aCodes, 0, 0);
	
	// Show some codes
	// 显示一些编码
	printf("\nSample Huffman codes:\n");
	for ( int i = 0; i < 256; i++ ) {
		if ( aiFreqTable[i] > 0 ) {
			printf("  '%c': ", i);
			for ( int j = aCodes[i].iCodeLength - 1; j >= 0; j-- ) {
				printf("%d", (aCodes[i].uiCode >> j) & 1);
			}
			printf(" (length: %d)\n", aCodes[i].iCodeLength);
		}
	}
	
	// Compress data
	// 压缩数据
	BitBuffer* pCompressed = HuffmanCompress((unsigned char*)sTestData, iDataSize, aCodes);
	
	printf("\nCompressed size: %zu bytes\n", pCompressed->iBytePosition);
	printf("Compression ratio: %.2f%%\n", 
	       CalculateCompressionRatio(iDataSize, pCompressed->iBytePosition));
	
	// Decompress data
	// 解压缩数据
	size_t iDecompressedSize;
	unsigned char* pucDecompressed = HuffmanDecompress(pCompressed, iDataSize, pTree, &iDecompressedSize);
	
	printf("Decompressed size: %zu bytes\n", iDecompressedSize);
	
	// Verify correctness
	// 验证正确性
	if ( iDecompressedSize == iDataSize && 
	     memcmp(pucDecompressed, sTestData, iDataSize) == 0 ) {
		printf("Decompression successful - data matches original!\n");
	} else {
		printf("Decompression failed!\n");
	}
	
	// Cleanup
	// 清理
	xrtFree(pucDecompressed);
	BitBufferDestroy(pCompressed);
	HuffmanTreeDestroy(pTree);
}

// Test with real text
// 用真实文本测试
void TestRealText()
{
	printf("\n=== Real Text Compression Test ===\n");
	printf("=== 真实文本压缩测试 ===\n");
	
	str sText = "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG "
	           "the quick brown fox jumps over the lazy dog "
	           "The Quick Brown Fox Jumps Over The Lazy Dog";
	
	size_t iSize = strlen(sText);
	printf("Text: %s\n", sText);
	printf("Size: %zu bytes\n", iSize);
	
	// Frequency analysis
	// 频率分析
	int aiFreqTable[256];
	BuildFrequencyTable((unsigned char*)sText, iSize, aiFreqTable);
	
	// Huffman compression
	// 霍夫曼压缩
	HuffmanNode* pTree = BuildHuffmanTree(aiFreqTable);
	
	HuffmanCode aCodes[256];
	memset(aCodes, 0, sizeof(aCodes));
	GenerateHuffmanCodes(pTree, aCodes, 0, 0);
	
	BitBuffer* pCompressed = HuffmanCompress((unsigned char*)sText, iSize, aCodes);
	
	printf("Compressed size: %zu bytes\n", pCompressed->iBytePosition);
	printf("Compression ratio: %.2f%%\n", 
	       CalculateCompressionRatio(iSize, pCompressed->iBytePosition));
	
	// Decompress and verify
	// 解压缩并验证
	size_t iDecompressedSize;
	unsigned char* pucDecompressed = HuffmanDecompress(pCompressed, iSize, pTree, &iDecompressedSize);
	
	if ( iDecompressedSize == iSize && 
	     memcmp(pucDecompressed, sText, iSize) == 0 ) {
		printf("Real text compression successful!\n");
	}
	
	xrtFree(pucDecompressed);
	BitBufferDestroy(pCompressed);
	HuffmanTreeDestroy(pTree);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Huffman Compression Demo\n");
	printf("XRT 霍夫曼压缩演示\n");
	printf("=========================\n\n");
	
	// Run tests
	// 运行测试
	TestHuffmanCompression();
	TestRealText();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}