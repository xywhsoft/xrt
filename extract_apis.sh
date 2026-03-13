#!/bin/bash
# XRT API 文档生成脚本
# 自动提取所有API信息并生成文档

cd "$(dirname "$0")"
OUTPUT_DIR="docs"
XRT_HEADER="xrt.h"

echo "=== XRT API 文档自动生成系统 ===" > api_extract.log
echo "开始时间: $(date)" >> api_extract.log
echo "" >> api_extract.log

# 创建输出目录
mkdir -p "$OUTPUT_DIR"

# 提取所有 XXAPI 函数
echo "步骤1: 提取所有 XXAPI 函数..." >> api_extract.log
grep "XXAPI.*(" "$XRT_HEADER" > all_functions.txt

# 统计函数数量
FUNC_COUNT=$(wc -l < all_functions.txt)
echo "提取到的函数总数: $FUNC_COUNT" >> api_extract.log

# 按模块分类函数
echo "" >> api_extract.log
echo "步骤2: 按模块分类函数..." >> api_extract.log

# 基础设施层
grep -E "(xrtInit|xrtUnit|xrtMalloc|xrtFree|xrtCalloc|xrtRealloc)" "$XRT_HEADER" > api/base_functions.txt
grep -E "(xrtUTF8to16|xrtUTF16to8|xrtUTF8to32|xrtUTF32to8)" "$XRT_HEADER" > api/charset_functions.txt
grep -E "(xrtHash32|xrtHash64)" "$XRT_HEADER" > api/hash_functions.txt
grep -E "(xrtRand32|xrtRand64|xrtRandSeed)" "$XRT_HEADER" > api/math_functions.txt
grep -E "(xrtNow|xrtTimeToStr|xrtDateSerial)" "$XRT_HEADER" > api/time_functions.txt

echo "基础设施层函数提取完成" >> api_extract.log

# 输出统计信息
echo "" >> api_extract.log
echo "=== 函数统计 ===" >> api_extract.log
echo "base: $(wc -l < api/base_functions.txt)" >> api_extract.log
echo "charset: $(wc -l < api/charset_functions.txt)" >> api_extract.log
echo "hash: $(wc -l < api/hash_functions.txt)" >> api_extract.log
echo "math: $(wc -l < api/math_functions.txt)" >> api_extract.log
echo "time: $(wc -l < api/time_functions.txt)" >> api_extract.log

echo "" >> api_extract.log
echo "API 提取完成！" >> api_extract.log
echo "结束时间: $(date)" >> api_extract.log

echo "✅ API 提取完成！"
cat api_extract.log
