# XRT 范例程序实施规格文档

---
## Goal

为 XRT 库实现 **120 个范例程序**，按照 `范例程序完整计划.md` 的规范执行。补全 **111 个未完成的范例**。

## Instructions

### 核心规范
1. **目录结构**: `examples/{子库名}/{范例名}/main.c`
2. **引用方式**: 
   ```c
   #define XRT_IMPLEMENTATION
   #include "../../../singlehead/xrt.h"
   ```
3. **输出目录**: `examples/bin/{子库名}_{范例名}.exe`
4. **编码风格**: Tab缩进、匈牙利命名法、中英双语注释、函数花括号单独一行
5. **每个范例必须包含**:
   - 顶部双语注释模板（功能说明、编译命令、注意事项）
   - `xrtInit()` 初始化
   - `xrtUnit()` 清理
6. **实施方式**: 逐个实现、无人值守、每完成一个更新 spec 进度

### 编码风格规律
```
- 缩进: Tab (8空格宽)
- 命名: i=整数, f=浮点, s/str=字符串, b=布尔, p/ptr=指针, arr=数组
- 函数: xrt + 操作 + 模块名 (如 xrtMalloc, xrtStrComp)
- 注释: // English comment \n // 中文注释
- 条件编译: #if defined(_WIN32) || defined(_WIN64) ... #else ... #endif
- 花括号: 函数定义单独一行，if/for/while 同行
- 空格: if ( condition ) { ... }
- 返回值: 成功返回有效指针/TRUE，失败返回 NULL/xCore.sNull/FALSE
```

### 范例模板
```c
/*
 * XRT Example - Example Name
 * XRT 范例 - 示例名称
 *
 * Description / 说明:
 *   EN: English function description
 *   CN: 中文功能描述
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/{output}.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/{output} -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Note item 1
 *   - Note item 2
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();

	// ... example code ...

	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
```

### 实施顺序 (8个阶段)
1. **Phase 1 - 基础层** (19个): base → string → charset → path → jnum (math已完成)
2. **Phase 2 - 系统层** (14个): os → time → file → hash
3. **Phase 3 - 数据结构层** (18个): buffer → array → stack → bsmm → memunit → mempool
4. **Phase 4 - 高级数据层** (15个): avltree → dict → list → value
5. **Phase 5 - 处理引擎层** (10个): json → template
6. **Phase 6 - 并发层** (10个): thread → coroutine
7. **Phase 7 - 网络与安全层** (24个): crypto → network → http → xid
8. **Phase 8 - 经典工具层** (10个): tools

---

## Progress

### 已完成范例 (6个) ✅
| 模块 | 范例 | 状态 |
|------|------|------|
| string | copy_and_compare | ✅ 完成 |
| string | case_convert | ✅ 完成 |
| string | search_and_match | ✅ 完成 |
| string | trim_and_filter | ✅ 完成 |
| math | random | ✅ 完成 |
| math | approx | ✅ 完成 |

---

## Phase 1 - 基础层 (19个新增)

### base (4个)
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | init_and_config | ✅ 完成 | xrtInit/xrtUnit 初始化和销毁流程 |
| 2 | memory_basic | ✅ 完成 | xrtMalloc/xrtCalloc/xrtRealloc/xrtFree |
| 3 | temp_memory | ✅ 完成 | xrtTempMemory 环形临时内存机制 |
| 4 | error_handling | ✅ 完成 | xrtSetError/xrtClearError/OnError |

### string (4个新增)
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | format_and_replace | ✅ 完成 | xrtFormat/xrtReplace 字符串格式化与替换 |
| 2 | random_string | ✅ 完成 | xrtRandStr 生成随机字符串 |
| 3 | number_format | ✅ 完成 | xrtIntFormat/xrtNumFormat 数字格式化 |
| 4 | similarity | ✅ 完成 | xrtStrSim/xrtStrApprox 字符串相似度 |

### charset (3个) - 目录不存在
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | utf_convert | ✅ 完成 | UTF-8/16/32 互转 |
| 2 | charset_detect | ✅ 完成 | xrtIsUTF8/xrtDetectCharset |
| 3 | endian_convert | ✅ 完成 | UTF-16/32 大小端序转换 |

### path (3个) - 目录不存在
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | path_parse | ✅ 完成 | xrtPathGetNameExt/GetName/GetExt/GetDir |
| 2 | path_join | ✅ 完成 | xrtPathJoin 多段路径拼接 |
| 3 | path_check | ✅ 完成 | xrtPathIsAbs/xrtPathRandom/xrtPathExists |

### jnum (2个) - 目录不存在
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | num_to_str | ✅ 完成 | xrtI32ToStr/I64ToStr/NumToStr |
| 2 | str_to_num | ✅ 完成 | xrtStrToI32/I64/Num/xrtParseNum |

---

## Phase 2 - 系统层 (14个新增)

### os (1个)
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | run_program | ✅ 完成 | xrtRun/xrtStart/xrtChain |

### time (7个)
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | basic_datetime | ✅ 完成 | xrtNow/Date/Time, xrtDateSerial/TimeSerial |
| 2 | time_extract | ✅ 完成 | xrtYear/Month/Day/Hour/Minute/Second/Weekday |
| 3 | time_calc | ✅ 完成 | xrtDateAdd/xrtDateDiff |
| 4 | time_format | ✅ 完成 | xrtTimeFormat/xrtTimeParse |
| 5 | time_compare | ✅ 完成 | xrtIsSameDay/Month/Year, xrtTimeInRange |
| 6 | timezone | ✅ 完成 | xrtNowUTC/xrtTimezoneOffset |
| 7 | timer | ✅ 完成 | xrtTimer/xrtSleep |

### file (6个)
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | read_write | ✅ 完成 | xrtOpen/Close/Read/Write |
| 2 | file_readall | ✅ 完成 | xrtFileReadAll/WriteAll/Append |
| 3 | binary_io | ⏳ 待实现 | xrtGet/Put/xrtFileGetAll/PutAll |
| 4 | file_info | ✅ 完成 | xrtFileGetSize/GetAttr/GetAccessTime |
| 5 | file_manage | ✅ 完成 | xrtFileCopy/Move/Delete |
| 6 | dir_operations | ✅ 完成 | xrtDirCreate/CreateAll/Scan/Copy/Delete |

### hash (2个) - 目录不存在
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | hash32 | ✅ 完成 | xrtHash32/Hash32_WithSeed |
| 2 | hash64 | ✅ 完成 | xrtHash64/Micro/Nano |

---

## Phase 3 - 数据结构层 (18个新增)

### buffer (2个)
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | basic_buffer | ✅ 完成 | xrtBufferCreate/Append/Insert/Destroy |
| 2 | buffer_builder | ✅ 完成 | StringBuilder 模式 |

### array (3个)
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | struct_array | ✅ 完成 | xrtArrayCreate/Insert/Append/Get/Remove |
| 2 | ptr_array | ✅ 完成 | xrtPtrArrayCreate/Append/AddAlt/Get/Set |
| 3 | array_sort | ✅ 完成 | xrtArraySort/xrtPtrArraySort |

### stack (3个)
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | static_stack | ✅ 完成 | xrtStackCreate/Push/Pop/Top |
| 2 | dynamic_stack | ✅ 完成 | xrtDynStackCreate/Push/Pop/Top |
| 3 | ptr_stack | ✅ 完成 | xrtStackPushPtr/PopPtr/TopPtr |

### bsmm (1个)
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | block_memory | ✅ 完成 | xrtBsmmCreate/Alloc/Free/Destroy |

### memunit (1个)
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | mem_unit | ✅ 完成 | xrtMemUnitCreate/Alloc/Free/GC |

### mempool (3个)
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | fs_mempool | ✅ 完成 | xrtFSMemPoolCreate/Alloc/Free |
| 2 | general_mempool | ✅ 完成 | xrtMemPoolCreate/Alloc/Free |
| 3 | mempool_gc | ✅ 完成 | xrtFSMemPoolGC/xrtMemPoolGC |

---

## Phase 4 - 高级数据层 (15个新增)

### avltree (3个)
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | basic_avltree | ✅ 完成 | xrtAVLTreeCreate/Insert/Search/Remove |
| 2 | avltree_iterator | ✅ 完成 | AVLTREE_FOREACH 迭代器遍历 |
| 3 | avltree_custom | ✅ 完成 | 自定义比较函数 |

### dict (3个)
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | basic_dict | ✅ 完成 | xrtDictCreate/Set/Get/Remove/Exists |
| 2 | dict_iterator | ✅ 完成 | DICT_FOREACH/DICT_FOREACH_TYPE |
| 3 | dict_ptr | ✅ 完成 | xrtDictSetPtr/GetPtr/RemovePtr |

### list (2个)
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | basic_list | ✅ 完成 | xrtListCreate/Set/Get/Remove/Exists |
| 2 | list_iterator | ✅ 完成 | LIST_FOREACH/LIST_FOREACH_TYPE |

### value (7个)
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | basic_types | ✅ 完成 | xvoCreateNull/Bool/Int/Float/Text/Time/Point |
| 2 | value_array | ✅ 完成 | xvoCreateArray/Append/Insert/Set/Get/Remove |
| 3 | value_table | ✅ 完成 | xvoCreateTable/Set/Get/Remove/Merge |
| 4 | value_list | ✅ 完成 | xvoCreateList/Set/Get/Merge |
| 5 | value_coll | ✅ 完成 | xvoCreateColl 集合操作 |
| 6 | refcount | ✅ 完成 | xvoAddRef/xvoUnref |
| 7 | deep_copy | ✅ 完成 | xvoCopy/xvoDeepCopy |

---

## Phase 5 - 处理引擎层 (10个新增)

### json (5个) - 目录不存在
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | parse_json | ⏳ 待实现 | xrtParseJSON |
| 2 | generate_json | ⏳ 待实现 | xrtStringifyJSON |
| 3 | sax_parser | ⏳ 待实现 | xrtJsonParseSAX |
| 4 | sax_printer | ⏳ 待实现 | xrtJsonPrintStart/PrintValue/PrintFinish |
| 5 | json_file | ⏳ 待实现 | xrtParseJSON_File/xrtStringifyJSON_File |

### template (5个) - 目录不存在
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | basic_template | ⏳ 待实现 | xteParse/xteMake |
| 2 | template_vars | ⏳ 待实现 | {$ var}/{% num}/{& time}/{? bool} |
| 3 | template_control | ⏳ 待实现 | {#if}/{#for}/{#foreach} |
| 4 | template_expr | ⏳ 待实现 | xteExprParse/xteExprEval |
| 5 | template_sub | ⏳ 待实现 | {#define}/{= sub}/{@ func} |

---

## Phase 6 - 并发层 (10个新增)

### thread (6个)
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | thread_basic | ⏳ 待实现 | xrtThreadCreate/Wait/Stop/ShouldStop |
| 2 | mutex | ⏳ 待实现 | xrtMutexCreate/Lock/TryLock/Unlock |
| 3 | semaphore | ⏳ 待实现 | xrtSemCreate/Wait/Post/PostMultiple |
| 4 | condvar | ⏳ 待实现 | xrtCondCreate/Wait/Signal/Broadcast |
| 5 | rwlock | ⏳ 待实现 | xrtRWLock |
| 6 | producer_consumer | ⏳ 待实现 | 生产者-消费者模型 |

### coroutine (4个) - 目录不存在
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | basic_coroutine | ⏳ 待实现 | xrtCoCreate/Resume/Yield/Destroy |
| 2 | scheduler | ⏳ 待实现 | xrtCoSchedCreate/Spawn/Step/Run |
| 3 | coroutine_sleep | ⏳ 待实现 | xrtCoSleep |
| 4 | userdata | ⏳ 待实现 | xrtCoSetUserData/GetUserData |

---

## Phase 7 - 网络与安全层 (23个新增)

### crypto (8个)
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | sha256 | ⏳ 待实现 | xrtSHA256 |
| 2 | sha512 | ⏳ 待实现 | xrtSHA512/xrtSHA384 |
| 3 | hmac | ⏳ 待实现 | xrtHMAC_SHA256/SHA384/SHA512 |
| 4 | aes_gcm | ⏳ 待实现 | xrtAES128GCMEncrypt/Decrypt |
| 5 | chacha20 | ⏳ 待实现 | xrtChaCha20/xrtChaCha20Poly1305 |
| 6 | key_exchange | ⏳ 待实现 | xrtX25519/xrtECDHSecp256r1 |
| 7 | hkdf | ⏳ 待实现 | xrtHKDFExtract/Expand |
| 8 | random_bytes | ⏳ 待实现 | xrtRandomBytes |

### network (8个)
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | local_info | ⏳ 待实现 | xrtGetLocalIP/MAC/Name |
| 2 | socket_basic | ⏳ 待实现 | xrtSockCreate/Bind/Listen/Accept/Send/Recv |
| 3 | tcp_echo_server | ⏳ 待实现 | xrtTcpServerCreate |
| 4 | tcp_client | ⏳ 待实现 | xrtTcpClientCreate |
| 5 | udp_echo | ⏳ 待实现 | xrtUdpServerCreate |
| 6 | udp_client | ⏳ 待实现 | xrtUdpClientCreate |
| 7 | ringbuf | ⏳ 待实现 | xrtNetRingBufInit/Write/Read/Peek |
| 8 | dns_resolve | ⏳ 待实现 | xrtNetResolve |

### http (6个) - 目录不存在
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | http_get | ⏳ 待实现 | xrtHttpGet |
| 2 | http_post | ⏳ 待实现 | xrtHttpPost |
| 3 | http_download | ⏳ 待实现 | xrtHttpGetFile |
| 4 | http_upload | ⏳ 待实现 | Multipart 文件上传 |
| 5 | http_cookies | ⏳ 待实现 | Cookie 管理 |
| 6 | http_advanced | ⏳ 待实现 | 完整 API |

### xid (1个) - 目录不存在
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | generate_xid | ⏳ 待实现 | xrtMakeXID/EncodeXID/DecodeXID |

---

## Phase 8 - 经典工具层 (9个新增)

### tools (9个)
| # | 范例 | 状态 | 说明 |
|---|------|------|------|
| 1 | tcp_chatroom | ⏳ 待实现 | TCP 多人聊天室 |
| 2 | http_server | ⏳ 待实现 | 静态文件 HTTP 服务器 |
| 3 | json_config | ⏳ 待实现 | JSON 配置文件管理器 |
| 4 | file_hasher | ⏳ 待实现 | 文件哈希计算工具 |
| 5 | password_gen | ⏳ 待实现 | 安全密码生成器 |
| 6 | kv_store | ⏳ 待实现 | 内存 KV 存储引擎 |
| 7 | template_render | ⏳ 待实现 | 模板渲染工具 |
| 8 | file_encrypt | ⏳ 待实现 | 文件加密/解密工具 |

---

## Statistics

| 统计项 | 数量 |
|--------|------|
| 计划总数 | 120 |
| 已完成 | 6 |
| 待实现 | 111 |
| 当前进度 | 5% |

---

## Last Updated
- **时间**: 待更新
- **最后完成**: 无
- **下一步**: base/init_and_config
