/* 此文件由 tools/generate_features.py 生成，请勿直接修改。 */
#ifndef XRT_FEATURES_H
#define XRT_FEATURES_H

/* regex 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_REGEX)
#ifndef XRT_FEATURE_REGEX
#define XRT_FEATURE_REGEX
#endif
#ifndef XRT_MODULE_REGEX_REPLACE
#define XRT_MODULE_REGEX_REPLACE
#endif
#ifndef XRT_MODULE_REGEX_SPLIT
#define XRT_MODULE_REGEX_SPLIT
#endif
#ifndef XRT_MODULE_REGEX_SET
#define XRT_MODULE_REGEX_SET
#endif
#endif

/* regex_set 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_REGEX_SET)
#ifndef XRT_FEATURE_REGEX_SET
#define XRT_FEATURE_REGEX_SET
#endif
#ifndef XRT_MODULE_REGEX_CORE
#define XRT_MODULE_REGEX_CORE
#endif
#endif

/* regex_split 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_REGEX_SPLIT)
#ifndef XRT_FEATURE_REGEX_SPLIT
#define XRT_FEATURE_REGEX_SPLIT
#endif
#ifndef XRT_MODULE_REGEX_MATCH
#define XRT_MODULE_REGEX_MATCH
#endif
#ifndef XRT_MODULE_STRING_SPLIT
#define XRT_MODULE_STRING_SPLIT
#endif
#endif

/* regex_replace 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_REGEX_REPLACE)
#ifndef XRT_FEATURE_REGEX_REPLACE
#define XRT_FEATURE_REGEX_REPLACE
#endif
#ifndef XRT_MODULE_REGEX_MATCH
#define XRT_MODULE_REGEX_MATCH
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#endif

/* regex_match 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_REGEX_MATCH)
#ifndef XRT_FEATURE_REGEX_MATCH
#define XRT_FEATURE_REGEX_MATCH
#endif
#ifndef XRT_MODULE_REGEX_CORE
#define XRT_MODULE_REGEX_CORE
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#endif

/* regex_core 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_REGEX_CORE)
#ifndef XRT_FEATURE_REGEX_CORE
#define XRT_FEATURE_REGEX_CORE
#endif
#endif

/* pattern 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_PATTERN)
#ifndef XRT_FEATURE_PATTERN
#define XRT_FEATURE_PATTERN
#endif
#endif

/* ptr_stack 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_PTR_STACK)
#ifndef XRT_FEATURE_PTR_STACK
#define XRT_FEATURE_PTR_STACK
#endif
#ifndef XRT_MODULE_STACK
#define XRT_MODULE_STACK
#endif
#endif

/* block_stack 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_BLOCK_STACK)
#ifndef XRT_FEATURE_BLOCK_STACK
#define XRT_FEATURE_BLOCK_STACK
#endif
#ifndef XRT_MODULE_ARRAY
#define XRT_MODULE_ARRAY
#endif
#endif

/* stack 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_STACK)
#ifndef XRT_FEATURE_STACK
#define XRT_FEATURE_STACK
#endif
#ifndef XRT_MODULE_ARRAY
#define XRT_MODULE_ARRAY
#endif
#endif

/* ptr_fixed_stack 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_PTR_FIXED_STACK)
#ifndef XRT_FEATURE_PTR_FIXED_STACK
#define XRT_FEATURE_PTR_FIXED_STACK
#endif
#ifndef XRT_MODULE_FIXED_STACK
#define XRT_MODULE_FIXED_STACK
#endif
#endif

/* fixed_stack 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_FIXED_STACK)
#ifndef XRT_FEATURE_FIXED_STACK
#define XRT_FEATURE_FIXED_STACK
#endif
#endif

/* process_terminal 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_PROCESS_TERMINAL)
#ifndef XRT_FEATURE_PROCESS_TERMINAL
#define XRT_FEATURE_PROCESS_TERMINAL
#endif
#ifndef XRT_MODULE_PROCESS
#define XRT_MODULE_PROCESS
#endif
#endif

/* process_file 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_PROCESS_FILE)
#ifndef XRT_FEATURE_PROCESS_FILE
#define XRT_FEATURE_PROCESS_FILE
#endif
#ifndef XRT_MODULE_PROCESS
#define XRT_MODULE_PROCESS
#endif
#ifndef XRT_MODULE_FILE
#define XRT_MODULE_FILE
#endif
#endif

/* process_future 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_PROCESS_FUTURE)
#ifndef XRT_FEATURE_PROCESS_FUTURE
#define XRT_FEATURE_PROCESS_FUTURE
#endif
#ifndef XRT_MODULE_PROCESS
#define XRT_MODULE_PROCESS
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#endif

/* process_pipeline 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_PROCESS_PIPELINE)
#ifndef XRT_FEATURE_PROCESS_PIPELINE
#define XRT_FEATURE_PROCESS_PIPELINE
#endif
#ifndef XRT_MODULE_PROCESS_RUN
#define XRT_MODULE_PROCESS_RUN
#endif
#endif

/* process_run 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_PROCESS_RUN)
#ifndef XRT_FEATURE_PROCESS_RUN
#define XRT_FEATURE_PROCESS_RUN
#endif
#ifndef XRT_MODULE_PROCESS
#define XRT_MODULE_PROCESS
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_CANCEL
#define XRT_MODULE_CANCEL
#endif
#endif

/* process_open 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_PROCESS_OPEN)
#ifndef XRT_FEATURE_PROCESS_OPEN
#define XRT_FEATURE_PROCESS_OPEN
#endif
#ifndef XRT_MODULE_PROCESS
#define XRT_MODULE_PROCESS
#endif
#endif

/* process 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_PROCESS)
#ifndef XRT_FEATURE_PROCESS
#define XRT_FEATURE_PROCESS
#endif
#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#ifndef XRT_MODULE_COND
#define XRT_MODULE_COND
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#endif

/* signal 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_SIGNAL)
#ifndef XRT_FEATURE_SIGNAL
#define XRT_FEATURE_SIGNAL
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifndef XRT_MODULE_ONCE
#define XRT_MODULE_ONCE
#endif
#ifndef XRT_MODULE_COND
#define XRT_MODULE_COND
#endif
#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#endif

/* environment 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_ENVIRONMENT)
#ifndef XRT_FEATURE_ENVIRONMENT
#define XRT_FEATURE_ENVIRONMENT
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#endif

/* logger_ring 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_LOGGER_RING)
#ifndef XRT_FEATURE_LOGGER_RING
#define XRT_FEATURE_LOGGER_RING
#endif
#ifndef XRT_MODULE_LOGGER_CORE
#define XRT_MODULE_LOGGER_CORE
#endif
#ifndef XRT_MODULE_QUEUE_MPSC
#define XRT_MODULE_QUEUE_MPSC
#endif
#ifndef XRT_MODULE_QUEUE_MPMC
#define XRT_MODULE_QUEUE_MPMC
#endif
#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#endif

/* logger_async 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_LOGGER_ASYNC)
#ifndef XRT_FEATURE_LOGGER_ASYNC
#define XRT_FEATURE_LOGGER_ASYNC
#endif
#ifndef XRT_MODULE_LOGGER_CORE
#define XRT_MODULE_LOGGER_CORE
#endif
#ifndef XRT_MODULE_COND
#define XRT_MODULE_COND
#endif
#ifndef XRT_MODULE_EVENT
#define XRT_MODULE_EVENT
#endif
#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#endif

/* logger_file_json 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_LOGGER_FILE_JSON)
#ifndef XRT_FEATURE_LOGGER_FILE_JSON
#define XRT_FEATURE_LOGGER_FILE_JSON
#endif
#ifndef XRT_MODULE_LOGGER_FILE
#define XRT_MODULE_LOGGER_FILE
#endif
#ifndef XRT_MODULE_LOGGER_FORMAT_JSON
#define XRT_MODULE_LOGGER_FORMAT_JSON
#endif
#endif

/* logger_file_text 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_LOGGER_FILE_TEXT)
#ifndef XRT_FEATURE_LOGGER_FILE_TEXT
#define XRT_FEATURE_LOGGER_FILE_TEXT
#endif
#ifndef XRT_MODULE_LOGGER_FILE
#define XRT_MODULE_LOGGER_FILE
#endif
#ifndef XRT_MODULE_LOGGER_FORMAT_TEXT
#define XRT_MODULE_LOGGER_FORMAT_TEXT
#endif
#endif

/* logger_file 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_LOGGER_FILE)
#ifndef XRT_FEATURE_LOGGER_FILE
#define XRT_FEATURE_LOGGER_FILE
#endif
#ifndef XRT_MODULE_LOGGER_CORE
#define XRT_MODULE_LOGGER_CORE
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_FILE
#define XRT_MODULE_FILE
#endif
#endif

/* logger_format_json_buffer 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_LOGGER_FORMAT_JSON_BUFFER)
#ifndef XRT_FEATURE_LOGGER_FORMAT_JSON_BUFFER
#define XRT_FEATURE_LOGGER_FORMAT_JSON_BUFFER
#endif
#ifndef XRT_MODULE_LOGGER_FORMAT_JSON
#define XRT_MODULE_LOGGER_FORMAT_JSON
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#endif

/* logger_console 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_LOGGER_CONSOLE)
#ifndef XRT_FEATURE_LOGGER_CONSOLE
#define XRT_FEATURE_LOGGER_CONSOLE
#endif
#ifndef XRT_MODULE_LOGGER_CORE
#define XRT_MODULE_LOGGER_CORE
#endif
#ifndef XRT_MODULE_LOGGER_FORMAT_TEXT
#define XRT_MODULE_LOGGER_FORMAT_TEXT
#endif
#ifndef XRT_MODULE_CONSOLE
#define XRT_MODULE_CONSOLE
#endif
#endif

/* logger_format_json 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_LOGGER_FORMAT_JSON)
#ifndef XRT_FEATURE_LOGGER_FORMAT_JSON
#define XRT_FEATURE_LOGGER_FORMAT_JSON
#endif
#ifndef XRT_MODULE_LOGGER_CORE
#define XRT_MODULE_LOGGER_CORE
#endif
#ifndef XRT_MODULE_JSON_ESCAPE
#define XRT_MODULE_JSON_ESCAPE
#endif
#ifndef XRT_MODULE_NUMBER_INTEGER
#define XRT_MODULE_NUMBER_INTEGER
#endif
#ifndef XRT_MODULE_NUMBER_FLOAT
#define XRT_MODULE_NUMBER_FLOAT
#endif
#endif

/* logger_format_text_buffer 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_LOGGER_FORMAT_TEXT_BUFFER)
#ifndef XRT_FEATURE_LOGGER_FORMAT_TEXT_BUFFER
#define XRT_FEATURE_LOGGER_FORMAT_TEXT_BUFFER
#endif
#ifndef XRT_MODULE_LOGGER_FORMAT_TEXT
#define XRT_MODULE_LOGGER_FORMAT_TEXT
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#endif

/* logger_format_text 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_LOGGER_FORMAT_TEXT)
#ifndef XRT_FEATURE_LOGGER_FORMAT_TEXT
#define XRT_FEATURE_LOGGER_FORMAT_TEXT
#endif
#ifndef XRT_MODULE_LOGGER_CORE
#define XRT_MODULE_LOGGER_CORE
#endif
#ifndef XRT_MODULE_NUMBER_INTEGER
#define XRT_MODULE_NUMBER_INTEGER
#endif
#ifndef XRT_MODULE_NUMBER_FLOAT
#define XRT_MODULE_NUMBER_FLOAT
#endif
#endif

/* logger_printf 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_LOGGER_PRINTF)
#ifndef XRT_FEATURE_LOGGER_PRINTF
#define XRT_FEATURE_LOGGER_PRINTF
#endif
#ifndef XRT_MODULE_LOGGER_CORE
#define XRT_MODULE_LOGGER_CORE
#endif
#ifndef XRT_MODULE_STRING_FORMAT
#define XRT_MODULE_STRING_FORMAT
#endif
#endif

/* logger_core 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_LOGGER_CORE)
#ifndef XRT_FEATURE_LOGGER_CORE
#define XRT_FEATURE_LOGGER_CORE
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#endif

/* console 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CONSOLE)
#ifndef XRT_FEATURE_CONSOLE
#define XRT_FEATURE_CONSOLE
#endif
#endif

/* xid 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_XID)
#ifndef XRT_FEATURE_XID
#define XRT_FEATURE_XID
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#endif

/* template_file 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TEMPLATE_FILE)
#ifndef XRT_FEATURE_TEMPLATE_FILE
#define XRT_FEATURE_TEMPLATE_FILE
#endif
#ifndef XRT_MODULE_TEMPLATE_CORE
#define XRT_MODULE_TEMPLATE_CORE
#endif
#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#endif

/* template_extension 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TEMPLATE_EXTENSION)
#ifndef XRT_FEATURE_TEMPLATE_EXTENSION
#define XRT_FEATURE_TEMPLATE_EXTENSION
#endif
#ifndef XRT_MODULE_TEMPLATE_COMPOSE
#define XRT_MODULE_TEMPLATE_COMPOSE
#endif
#ifndef XRT_MODULE_MAP
#define XRT_MODULE_MAP
#endif
#endif

/* template_compose 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TEMPLATE_COMPOSE)
#ifndef XRT_FEATURE_TEMPLATE_COMPOSE
#define XRT_FEATURE_TEMPLATE_COMPOSE
#endif
#ifndef XRT_MODULE_TEMPLATE_CONTROL
#define XRT_MODULE_TEMPLATE_CONTROL
#endif
#ifndef XRT_MODULE_MAP
#define XRT_MODULE_MAP
#endif
#endif

/* template_control 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TEMPLATE_CONTROL)
#ifndef XRT_FEATURE_TEMPLATE_CONTROL
#define XRT_FEATURE_TEMPLATE_CONTROL
#endif
#ifndef XRT_MODULE_TEMPLATE_CORE
#define XRT_MODULE_TEMPLATE_CORE
#endif
#endif

/* template_core 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TEMPLATE_CORE)
#ifndef XRT_FEATURE_TEMPLATE_CORE
#define XRT_FEATURE_TEMPLATE_CORE
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#ifndef XRT_MODULE_ARRAY
#define XRT_MODULE_ARRAY
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#ifndef XRT_MODULE_NUMBER_FORMAT
#define XRT_MODULE_NUMBER_FORMAT
#endif
#ifndef XRT_MODULE_TIME_TEXT
#define XRT_MODULE_TIME_TEXT
#endif
#endif

/* xson 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_XSON)
#ifndef XRT_FEATURE_XSON
#define XRT_FEATURE_XSON
#endif
#ifndef XRT_MODULE_XSON_FILE
#define XRT_MODULE_XSON_FILE
#endif
#endif

/* xson_file 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_XSON_FILE)
#ifndef XRT_FEATURE_XSON_FILE
#define XRT_FEATURE_XSON_FILE
#endif
#ifndef XRT_MODULE_XSON_READ
#define XRT_MODULE_XSON_READ
#endif
#ifndef XRT_MODULE_XSON_WRITE
#define XRT_MODULE_XSON_WRITE
#endif
#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#endif

/* xson_write 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_XSON_WRITE)
#ifndef XRT_FEATURE_XSON_WRITE
#define XRT_FEATURE_XSON_WRITE
#endif
#ifndef XRT_MODULE_XSON_CORE
#define XRT_MODULE_XSON_CORE
#endif
#ifndef XRT_MODULE_JSON_ESCAPE
#define XRT_MODULE_JSON_ESCAPE
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#ifndef XRT_MODULE_TIME_TEXT
#define XRT_MODULE_TIME_TEXT
#endif
#ifndef XRT_MODULE_NUMBER_INTEGER
#define XRT_MODULE_NUMBER_INTEGER
#endif
#ifndef XRT_MODULE_NUMBER_FLOAT
#define XRT_MODULE_NUMBER_FLOAT
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#endif

/* xson_read 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_XSON_READ)
#ifndef XRT_FEATURE_XSON_READ
#define XRT_FEATURE_XSON_READ
#endif
#ifndef XRT_MODULE_XSON_CORE
#define XRT_MODULE_XSON_CORE
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#ifndef XRT_MODULE_TIME_TEXT
#define XRT_MODULE_TIME_TEXT
#endif
#ifndef XRT_MODULE_NUMBER_INTEGER
#define XRT_MODULE_NUMBER_INTEGER
#endif
#ifndef XRT_MODULE_NUMBER_FLOAT
#define XRT_MODULE_NUMBER_FLOAT
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#endif

/* xson_core 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_XSON_CORE)
#ifndef XRT_FEATURE_XSON_CORE
#define XRT_FEATURE_XSON_CORE
#endif
#endif

/* json 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_JSON)
#ifndef XRT_FEATURE_JSON
#define XRT_FEATURE_JSON
#endif
#ifndef XRT_MODULE_JSON_FILE
#define XRT_MODULE_JSON_FILE
#endif
#endif

/* json_file 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_JSON_FILE)
#ifndef XRT_FEATURE_JSON_FILE
#define XRT_FEATURE_JSON_FILE
#endif
#ifndef XRT_MODULE_JSON_READ
#define XRT_MODULE_JSON_READ
#endif
#ifndef XRT_MODULE_JSON_WRITE
#define XRT_MODULE_JSON_WRITE
#endif
#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#endif

/* json_write 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_JSON_WRITE)
#ifndef XRT_FEATURE_JSON_WRITE
#define XRT_FEATURE_JSON_WRITE
#endif
#ifndef XRT_MODULE_JSON_ESCAPE
#define XRT_MODULE_JSON_ESCAPE
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_NUMBER_INTEGER
#define XRT_MODULE_NUMBER_INTEGER
#endif
#ifndef XRT_MODULE_NUMBER_FLOAT
#define XRT_MODULE_NUMBER_FLOAT
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#endif

/* json_read 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_JSON_READ)
#ifndef XRT_FEATURE_JSON_READ
#define XRT_FEATURE_JSON_READ
#endif
#ifndef XRT_MODULE_JSON_CORE
#define XRT_MODULE_JSON_CORE
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_NUMBER_INTEGER
#define XRT_MODULE_NUMBER_INTEGER
#endif
#ifndef XRT_MODULE_NUMBER_FLOAT
#define XRT_MODULE_NUMBER_FLOAT
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#endif

/* json_escape 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_JSON_ESCAPE)
#ifndef XRT_FEATURE_JSON_ESCAPE
#define XRT_FEATURE_JSON_ESCAPE
#endif
#ifndef XRT_MODULE_JSON_CORE
#define XRT_MODULE_JSON_CORE
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#endif

/* json_core 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_JSON_CORE)
#ifndef XRT_FEATURE_JSON_CORE
#define XRT_FEATURE_JSON_CORE
#endif
#endif

/* value_graph 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_VALUE_GRAPH)
#ifndef XRT_FEATURE_VALUE_GRAPH
#define XRT_FEATURE_VALUE_GRAPH
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#endif

/* value_collection 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_VALUE_COLLECTION)
#ifndef XRT_FEATURE_VALUE_COLLECTION
#define XRT_FEATURE_VALUE_COLLECTION
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#endif

/* value_container 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_VALUE_CONTAINER)
#ifndef XRT_FEATURE_VALUE_CONTAINER
#define XRT_FEATURE_VALUE_CONTAINER
#endif
#ifndef XRT_MODULE_VALUE
#define XRT_MODULE_VALUE
#endif
#ifndef XRT_MODULE_PTR_ARRAY
#define XRT_MODULE_PTR_ARRAY
#endif
#ifndef XRT_MODULE_INT_MAP
#define XRT_MODULE_INT_MAP
#endif
#ifndef XRT_MODULE_MAP
#define XRT_MODULE_MAP
#endif
#ifndef XRT_MODULE_SET
#define XRT_MODULE_SET
#endif
#endif

/* value 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_VALUE)
#ifndef XRT_FEATURE_VALUE
#define XRT_FEATURE_VALUE
#endif
#ifndef XRT_MODULE_HASH64
#define XRT_MODULE_HASH64
#endif
#endif

/* set 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_SET)
#ifndef XRT_FEATURE_SET
#define XRT_FEATURE_SET
#endif
#ifndef XRT_MODULE_HASH64
#define XRT_MODULE_HASH64
#endif
#endif

/* map 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_MAP)
#ifndef XRT_FEATURE_MAP
#define XRT_FEATURE_MAP
#endif
#ifndef XRT_MODULE_HASH64
#define XRT_MODULE_HASH64
#endif
#endif

/* int_map 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_INT_MAP)
#ifndef XRT_FEATURE_INT_MAP
#define XRT_FEATURE_INT_MAP
#endif
#ifndef XRT_MODULE_AVL_TREE
#define XRT_MODULE_AVL_TREE
#endif
#endif

/* avl_tree 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_AVL_TREE)
#ifndef XRT_FEATURE_AVL_TREE
#define XRT_FEATURE_AVL_TREE
#endif
#ifndef XRT_MODULE_AVL
#define XRT_MODULE_AVL
#endif
#ifndef XRT_MODULE_POOL
#define XRT_MODULE_POOL
#endif
#endif

/* avl 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_AVL)
#ifndef XRT_FEATURE_AVL
#define XRT_FEATURE_AVL
#endif
#endif

/* slot_map 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_SLOT_MAP)
#ifndef XRT_FEATURE_SLOT_MAP
#define XRT_FEATURE_SLOT_MAP
#endif
#ifndef XRT_MODULE_ARRAY
#define XRT_MODULE_ARRAY
#endif
#endif

/* ptr_array 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_PTR_ARRAY)
#ifndef XRT_FEATURE_PTR_ARRAY
#define XRT_FEATURE_PTR_ARRAY
#endif
#ifndef XRT_MODULE_ARRAY
#define XRT_MODULE_ARRAY
#endif
#endif

/* buffer_base64 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_BUFFER_BASE64)
#ifndef XRT_FEATURE_BUFFER_BASE64
#define XRT_FEATURE_BUFFER_BASE64
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#endif

/* buffer_hex 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_BUFFER_HEX)
#ifndef XRT_FEATURE_BUFFER_HEX
#define XRT_FEATURE_BUFFER_HEX
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_CODEC_HEX
#define XRT_MODULE_CODEC_HEX
#endif
#endif

/* io_line 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_IO_LINE)
#ifndef XRT_FEATURE_IO_LINE
#define XRT_FEATURE_IO_LINE
#endif
#ifndef XRT_MODULE_IO
#define XRT_MODULE_IO
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#endif

/* io_file 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_IO_FILE)
#ifndef XRT_FEATURE_IO_FILE
#define XRT_FEATURE_IO_FILE
#endif
#ifndef XRT_MODULE_IO
#define XRT_MODULE_IO
#endif
#ifndef XRT_MODULE_FILE
#define XRT_MODULE_FILE
#endif
#endif

/* io_buffer 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_IO_BUFFER)
#ifndef XRT_FEATURE_IO_BUFFER
#define XRT_FEATURE_IO_BUFFER
#endif
#ifndef XRT_MODULE_IO
#define XRT_MODULE_IO
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#endif

/* io 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_IO)
#ifndef XRT_FEATURE_IO
#define XRT_FEATURE_IO
#endif
#endif

/* list 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_LIST)
#ifndef XRT_FEATURE_LIST
#define XRT_FEATURE_LIST
#endif
#endif

/* memory_pool 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_MEMORY_POOL)
#ifndef XRT_FEATURE_MEMORY_POOL
#define XRT_FEATURE_MEMORY_POOL
#endif
#ifndef XRT_MODULE_POOL
#define XRT_MODULE_POOL
#endif
#endif

/* pool 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_POOL)
#ifndef XRT_FEATURE_POOL
#define XRT_FEATURE_POOL
#endif
#ifndef XRT_MODULE_POOL_PAGE
#define XRT_MODULE_POOL_PAGE
#endif
#endif

/* pool_page 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_POOL_PAGE)
#ifndef XRT_FEATURE_POOL_PAGE
#define XRT_FEATURE_POOL_PAGE
#endif
#endif

/* file_fifo 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_FILE_FIFO)
#ifndef XRT_FEATURE_FILE_FIFO
#define XRT_FEATURE_FILE_FIFO
#endif
#ifndef XRT_MODULE_FILE
#define XRT_MODULE_FILE
#endif
#endif

/* file_root 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_FILE_ROOT)
#ifndef XRT_FEATURE_FILE_ROOT
#define XRT_FEATURE_FILE_ROOT
#endif
#ifndef XRT_MODULE_FILE_LINK
#define XRT_MODULE_FILE_LINK
#endif
#endif

/* dir_temp 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_DIR_TEMP)
#ifndef XRT_FEATURE_DIR_TEMP
#define XRT_FEATURE_DIR_TEMP
#endif
#ifndef XRT_MODULE_DIR
#define XRT_MODULE_DIR
#endif
#ifndef XRT_MODULE_FILE_TEMP
#define XRT_MODULE_FILE_TEMP
#endif
#endif

/* file_text 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_FILE_TEXT)
#ifndef XRT_FEATURE_FILE_TEXT
#define XRT_FEATURE_FILE_TEXT
#endif
#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#ifndef XRT_MODULE_CHARSET_DETECT
#define XRT_MODULE_CHARSET_DETECT
#endif
#endif

/* file_tree_async 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_FILE_TREE_ASYNC)
#ifndef XRT_FEATURE_FILE_TREE_ASYNC
#define XRT_FEATURE_FILE_TREE_ASYNC
#endif
#ifndef XRT_MODULE_FILE_TREE
#define XRT_MODULE_FILE_TREE
#endif
#ifndef XRT_MODULE_FILE_ASYNC_COMMON
#define XRT_MODULE_FILE_ASYNC_COMMON
#endif
#endif

/* file_tree 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_FILE_TREE)
#ifndef XRT_FEATURE_FILE_TREE
#define XRT_FEATURE_FILE_TREE
#endif
#ifndef XRT_MODULE_FILE_WALK
#define XRT_MODULE_FILE_WALK
#endif
#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#ifndef XRT_MODULE_FILE_LINK
#define XRT_MODULE_FILE_LINK
#endif
#endif

/* file_link 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_FILE_LINK)
#ifndef XRT_FEATURE_FILE_LINK
#define XRT_FEATURE_FILE_LINK
#endif
#ifndef XRT_MODULE_FILE
#define XRT_MODULE_FILE
#endif
#endif

/* file_walk 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_FILE_WALK)
#ifndef XRT_FEATURE_FILE_WALK
#define XRT_FEATURE_FILE_WALK
#endif
#ifndef XRT_MODULE_DIR
#define XRT_MODULE_DIR
#endif
#endif

/* dir_async 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_DIR_ASYNC)
#ifndef XRT_FEATURE_DIR_ASYNC
#define XRT_FEATURE_DIR_ASYNC
#endif
#ifndef XRT_MODULE_DIR
#define XRT_MODULE_DIR
#endif
#ifndef XRT_MODULE_FILE_ASYNC_COMMON
#define XRT_MODULE_FILE_ASYNC_COMMON
#endif
#endif

/* file_async_manage 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_FILE_ASYNC_MANAGE)
#ifndef XRT_FEATURE_FILE_ASYNC_MANAGE
#define XRT_FEATURE_FILE_ASYNC_MANAGE
#endif
#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#ifndef XRT_MODULE_FILE_ASYNC_COMMON
#define XRT_MODULE_FILE_ASYNC_COMMON
#endif
#endif

/* file_async_whole 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_FILE_ASYNC_WHOLE)
#ifndef XRT_FEATURE_FILE_ASYNC_WHOLE
#define XRT_FEATURE_FILE_ASYNC_WHOLE
#endif
#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#ifndef XRT_MODULE_FILE_ASYNC_COMMON
#define XRT_MODULE_FILE_ASYNC_COMMON
#endif
#endif

/* file_async 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_FILE_ASYNC)
#ifndef XRT_FEATURE_FILE_ASYNC
#define XRT_FEATURE_FILE_ASYNC
#endif
#ifndef XRT_MODULE_FILE
#define XRT_MODULE_FILE
#endif
#ifndef XRT_MODULE_FILE_ASYNC_COMMON
#define XRT_MODULE_FILE_ASYNC_COMMON
#endif
#endif

/* file_async_common 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_FILE_ASYNC_COMMON)
#ifndef XRT_FEATURE_FILE_ASYNC_COMMON
#define XRT_FEATURE_FILE_ASYNC_COMMON
#endif
#ifndef XRT_MODULE_TASK_POOL
#define XRT_MODULE_TASK_POOL
#endif
#endif

/* file_map 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_FILE_MAP)
#ifndef XRT_FEATURE_FILE_MAP
#define XRT_FEATURE_FILE_MAP
#endif
#ifndef XRT_MODULE_FILE
#define XRT_MODULE_FILE
#endif
#endif

/* file_lock 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_FILE_LOCK)
#ifndef XRT_FEATURE_FILE_LOCK
#define XRT_FEATURE_FILE_LOCK
#endif
#ifndef XRT_MODULE_FILE
#define XRT_MODULE_FILE
#endif
#endif

/* path_safe 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_PATH_SAFE)
#ifndef XRT_FEATURE_PATH_SAFE
#define XRT_FEATURE_PATH_SAFE
#endif
#ifndef XRT_MODULE_PATH
#define XRT_MODULE_PATH
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#endif

/* time_text 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TIME_TEXT)
#ifndef XRT_FEATURE_TIME_TEXT
#define XRT_FEATURE_TIME_TEXT
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#endif

/* time_local 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TIME_LOCAL)
#ifndef XRT_FEATURE_TIME_LOCAL
#define XRT_FEATURE_TIME_LOCAL
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#endif

/* rwlock 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_RWLOCK)
#ifndef XRT_FEATURE_RWLOCK
#define XRT_FEATURE_RWLOCK
#endif
#ifndef XRT_MODULE_SYNC
#define XRT_MODULE_SYNC
#endif
#endif

/* sem 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_SEM)
#ifndef XRT_FEATURE_SEM
#define XRT_FEATURE_SEM
#endif
#ifndef XRT_MODULE_SYNC
#define XRT_MODULE_SYNC
#endif
#ifndef XRT_MODULE_WAIT
#define XRT_MODULE_WAIT
#endif
#endif

/* net_udp_sync 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_UDP_SYNC)
#ifndef XRT_FEATURE_NET_UDP_SYNC
#define XRT_FEATURE_NET_UDP_SYNC
#endif
#ifndef XRT_MODULE_NET_UDP_FUTURE
#define XRT_MODULE_NET_UDP_FUTURE
#endif
#ifndef XRT_MODULE_NET_SYNC
#define XRT_MODULE_NET_SYNC
#endif
#endif

/* net_udp_future 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_UDP_FUTURE)
#ifndef XRT_FEATURE_NET_UDP_FUTURE
#define XRT_FEATURE_NET_UDP_FUTURE
#endif
#ifndef XRT_MODULE_NET_UDP
#define XRT_MODULE_NET_UDP
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#endif

/* net_udp 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_UDP)
#ifndef XRT_FEATURE_NET_UDP
#define XRT_FEATURE_NET_UDP
#endif
#ifndef XRT_MODULE_NET_ENGINE
#define XRT_MODULE_NET_ENGINE
#endif
#endif

/* net_tcp_dial_sync 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_TCP_DIAL_SYNC)
#ifndef XRT_FEATURE_NET_TCP_DIAL_SYNC
#define XRT_FEATURE_NET_TCP_DIAL_SYNC
#endif
#ifndef XRT_MODULE_NET_TCP_DIAL_FUTURE
#define XRT_MODULE_NET_TCP_DIAL_FUTURE
#endif
#ifndef XRT_MODULE_NET_TCP_SYNC
#define XRT_MODULE_NET_TCP_SYNC
#endif
#endif

/* net_tcp_sync 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_TCP_SYNC)
#ifndef XRT_FEATURE_NET_TCP_SYNC
#define XRT_FEATURE_NET_TCP_SYNC
#endif
#ifndef XRT_MODULE_NET_TCP_FUTURE
#define XRT_MODULE_NET_TCP_FUTURE
#endif
#ifndef XRT_MODULE_NET_SYNC
#define XRT_MODULE_NET_SYNC
#endif
#endif

/* net_tcp_future 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_TCP_FUTURE)
#ifndef XRT_FEATURE_NET_TCP_FUTURE
#define XRT_FEATURE_NET_TCP_FUTURE
#endif
#ifndef XRT_MODULE_NET_TCP
#define XRT_MODULE_NET_TCP
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#ifndef XRT_MODULE_NET_BUFFER
#define XRT_MODULE_NET_BUFFER
#endif
#endif

/* net_tcp_dial_future 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_TCP_DIAL_FUTURE)
#ifndef XRT_FEATURE_NET_TCP_DIAL_FUTURE
#define XRT_FEATURE_NET_TCP_DIAL_FUTURE
#endif
#ifndef XRT_MODULE_NET_TCP_DIAL
#define XRT_MODULE_NET_TCP_DIAL
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#ifndef XRT_MODULE_FUTURE_BRIDGE
#define XRT_MODULE_FUTURE_BRIDGE
#endif
#endif

/* tls_stream_dial_future 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_STREAM_DIAL_FUTURE)
#ifndef XRT_FEATURE_TLS_STREAM_DIAL_FUTURE
#define XRT_FEATURE_TLS_STREAM_DIAL_FUTURE
#endif
#ifndef XRT_MODULE_TLS_STREAM_DIAL
#define XRT_MODULE_TLS_STREAM_DIAL
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#ifndef XRT_MODULE_FUTURE_BRIDGE
#define XRT_MODULE_FUTURE_BRIDGE
#endif
#endif

/* tls_stream_dial 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_STREAM_DIAL)
#ifndef XRT_FEATURE_TLS_STREAM_DIAL
#define XRT_FEATURE_TLS_STREAM_DIAL
#endif
#ifndef XRT_MODULE_TLS_STREAM
#define XRT_MODULE_TLS_STREAM
#endif
#ifndef XRT_MODULE_NET_TCP_DIAL
#define XRT_MODULE_NET_TCP_DIAL
#endif
#endif

/* tls_stream_listener_sync 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_STREAM_LISTENER_SYNC)
#ifndef XRT_FEATURE_TLS_STREAM_LISTENER_SYNC
#define XRT_FEATURE_TLS_STREAM_LISTENER_SYNC
#endif
#ifndef XRT_MODULE_TLS_STREAM_LISTENER_FUTURE
#define XRT_MODULE_TLS_STREAM_LISTENER_FUTURE
#endif
#endif

/* tls_stream_listener_future 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_STREAM_LISTENER_FUTURE)
#ifndef XRT_FEATURE_TLS_STREAM_LISTENER_FUTURE
#define XRT_FEATURE_TLS_STREAM_LISTENER_FUTURE
#endif
#ifndef XRT_MODULE_TLS_STREAM_LISTENER
#define XRT_MODULE_TLS_STREAM_LISTENER
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#endif

/* tls_stream_listener 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_STREAM_LISTENER)
#ifndef XRT_FEATURE_TLS_STREAM_LISTENER
#define XRT_FEATURE_TLS_STREAM_LISTENER
#endif
#ifndef XRT_MODULE_TLS_STREAM
#define XRT_MODULE_TLS_STREAM
#endif
#ifndef XRT_MODULE_NET_TCP
#define XRT_MODULE_NET_TCP
#endif
#endif

/* tls_stream_future 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_STREAM_FUTURE)
#ifndef XRT_FEATURE_TLS_STREAM_FUTURE
#define XRT_FEATURE_TLS_STREAM_FUTURE
#endif
#ifndef XRT_MODULE_TLS_STREAM
#define XRT_MODULE_TLS_STREAM
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#ifndef XRT_MODULE_NET_BUFFER
#define XRT_MODULE_NET_BUFFER
#endif
#endif

/* net_tcp_server_sync 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_TCP_SERVER_SYNC)
#ifndef XRT_FEATURE_NET_TCP_SERVER_SYNC
#define XRT_FEATURE_NET_TCP_SERVER_SYNC
#endif
#ifndef XRT_MODULE_NET_TCP_SERVER_FUTURE
#define XRT_MODULE_NET_TCP_SERVER_FUTURE
#endif
#ifndef XRT_MODULE_NET_SYNC
#define XRT_MODULE_NET_SYNC
#endif
#endif

/* net_sync 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_SYNC)
#ifndef XRT_FEATURE_NET_SYNC
#define XRT_FEATURE_NET_SYNC
#endif
#ifndef XRT_MODULE_NET_ENGINE
#define XRT_MODULE_NET_ENGINE
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#endif

/* net_tcp_server_future 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_TCP_SERVER_FUTURE)
#ifndef XRT_FEATURE_NET_TCP_SERVER_FUTURE
#define XRT_FEATURE_NET_TCP_SERVER_FUTURE
#endif
#ifndef XRT_MODULE_NET_TCP_SERVER
#define XRT_MODULE_NET_TCP_SERVER
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#endif

/* net_tcp_server 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_TCP_SERVER)
#ifndef XRT_FEATURE_NET_TCP_SERVER
#define XRT_FEATURE_NET_TCP_SERVER
#endif
#ifndef XRT_MODULE_NET_TCP
#define XRT_MODULE_NET_TCP
#endif
#endif

/* net_tcp_file 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_TCP_FILE)
#ifndef XRT_FEATURE_NET_TCP_FILE
#define XRT_FEATURE_NET_TCP_FILE
#endif
#ifndef XRT_MODULE_NET_TCP
#define XRT_MODULE_NET_TCP
#endif
#ifndef XRT_MODULE_FILE
#define XRT_MODULE_FILE
#endif
#endif

/* task_group_net 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TASK_GROUP_NET)
#ifndef XRT_FEATURE_TASK_GROUP_NET
#define XRT_FEATURE_TASK_GROUP_NET
#endif
#ifndef XRT_MODULE_TASK_GROUP
#define XRT_MODULE_TASK_GROUP
#endif
#ifndef XRT_MODULE_TASK_NET
#define XRT_MODULE_TASK_NET
#endif
#endif

/* task_net 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TASK_NET)
#ifndef XRT_FEATURE_TASK_NET
#define XRT_FEATURE_TASK_NET
#endif
#ifndef XRT_MODULE_TASK
#define XRT_MODULE_TASK
#endif
#ifndef XRT_MODULE_NET_ENGINE
#define XRT_MODULE_NET_ENGINE
#endif
#ifndef XRT_MODULE_FUTURE_BRIDGE
#define XRT_MODULE_FUTURE_BRIDGE
#endif
#endif

/* net_file 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_FILE)
#ifndef XRT_FEATURE_NET_FILE
#define XRT_FEATURE_NET_FILE
#endif
#ifndef XRT_MODULE_FILE
#define XRT_MODULE_FILE
#endif
#ifndef XRT_MODULE_NET_ENGINE
#define XRT_MODULE_NET_ENGINE
#endif
#if defined(__linux__) && !defined(__ANDROID__)
#ifndef XRT_MODULE_NET_PORT_URING
#define XRT_MODULE_NET_PORT_URING
#endif
#endif
#endif

/* tls_identity_ed25519 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_IDENTITY_ED25519)
#ifndef XRT_FEATURE_TLS_IDENTITY_ED25519
#define XRT_FEATURE_TLS_IDENTITY_ED25519
#endif
#ifndef XRT_MODULE_TLS_IDENTITY
#define XRT_MODULE_TLS_IDENTITY
#endif
#ifndef XRT_MODULE_CRYPTO_ED25519_SIGN
#define XRT_MODULE_CRYPTO_ED25519_SIGN
#endif
#endif

/* tls_identity_p384 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_IDENTITY_P384)
#ifndef XRT_FEATURE_TLS_IDENTITY_P384
#define XRT_FEATURE_TLS_IDENTITY_P384
#endif
#ifndef XRT_MODULE_TLS_IDENTITY_EC
#define XRT_MODULE_TLS_IDENTITY_EC
#endif
#ifndef XRT_MODULE_CRYPTO_P384
#define XRT_MODULE_CRYPTO_P384
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_P384_SIGN_DER
#define XRT_MODULE_CRYPTO_ECDSA_P384_SIGN_DER
#endif
#ifndef XRT_MODULE_CRYPTO_HMAC_SHA256
#define XRT_MODULE_CRYPTO_HMAC_SHA256
#endif
#endif

/* tls_identity_p256 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_IDENTITY_P256)
#ifndef XRT_FEATURE_TLS_IDENTITY_P256
#define XRT_FEATURE_TLS_IDENTITY_P256
#endif
#ifndef XRT_MODULE_TLS_IDENTITY_EC
#define XRT_MODULE_TLS_IDENTITY_EC
#endif
#ifndef XRT_MODULE_CRYPTO_P256
#define XRT_MODULE_CRYPTO_P256
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_P256_SIGN_DER
#define XRT_MODULE_CRYPTO_ECDSA_P256_SIGN_DER
#endif
#ifndef XRT_MODULE_CRYPTO_HMAC_SHA512
#define XRT_MODULE_CRYPTO_HMAC_SHA512
#endif
#endif

/* tls_identity_ec 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_IDENTITY_EC)
#ifndef XRT_FEATURE_TLS_IDENTITY_EC
#define XRT_FEATURE_TLS_IDENTITY_EC
#endif
#ifndef XRT_MODULE_TLS_IDENTITY
#define XRT_MODULE_TLS_IDENTITY
#endif
#endif

/* net_port_uring 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_PORT_URING)
#ifndef XRT_FEATURE_NET_PORT_URING
#define XRT_FEATURE_NET_PORT_URING
#endif
#ifndef XRT_MODULE_NET_PORT
#define XRT_MODULE_NET_PORT
#endif
#endif

/* net_proxy_dial 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_PROXY_DIAL)
#ifndef XRT_FEATURE_NET_PROXY_DIAL
#define XRT_FEATURE_NET_PROXY_DIAL
#endif
#ifndef XRT_MODULE_NET_PROXY_HANDSHAKE
#define XRT_MODULE_NET_PROXY_HANDSHAKE
#endif
#ifndef XRT_MODULE_NET_TCP_DIAL
#define XRT_MODULE_NET_TCP_DIAL
#endif
#endif

/* net_tcp_dial 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_TCP_DIAL)
#ifndef XRT_FEATURE_NET_TCP_DIAL
#define XRT_FEATURE_NET_TCP_DIAL
#endif
#ifndef XRT_MODULE_NET_TCP
#define XRT_MODULE_NET_TCP
#endif
#ifndef XRT_MODULE_NET_RESOLVER
#define XRT_MODULE_NET_RESOLVER
#endif
#endif

/* net_proxy_http_connect 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_PROXY_HTTP_CONNECT)
#ifndef XRT_FEATURE_NET_PROXY_HTTP_CONNECT
#define XRT_FEATURE_NET_PROXY_HTTP_CONNECT
#endif
#ifndef XRT_MODULE_NET_PROXY_HANDSHAKE
#define XRT_MODULE_NET_PROXY_HANDSHAKE
#endif
#ifndef XRT_MODULE_HTTP1_HEAD
#define XRT_MODULE_HTTP1_HEAD
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#endif

/* net_proxy_socks5 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_PROXY_SOCKS5)
#ifndef XRT_FEATURE_NET_PROXY_SOCKS5
#define XRT_FEATURE_NET_PROXY_SOCKS5
#endif
#ifndef XRT_MODULE_NET_PROXY_HANDSHAKE
#define XRT_MODULE_NET_PROXY_HANDSHAKE
#endif
#endif

/* net_proxy_handshake 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_PROXY_HANDSHAKE)
#ifndef XRT_FEATURE_NET_PROXY_HANDSHAKE
#define XRT_FEATURE_NET_PROXY_HANDSHAKE
#endif
#ifndef XRT_MODULE_NET_PROXY
#define XRT_MODULE_NET_PROXY
#endif
#ifndef XRT_MODULE_NET_BUFFER
#define XRT_MODULE_NET_BUFFER
#endif
#endif

/* net_proxy 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_PROXY)
#ifndef XRT_FEATURE_NET_PROXY
#define XRT_FEATURE_NET_PROXY
#endif
#ifndef XRT_MODULE_NET
#define XRT_MODULE_NET
#endif
#endif

/* net_frame_length 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_FRAME_LENGTH)
#ifndef XRT_FEATURE_NET_FRAME_LENGTH
#define XRT_FEATURE_NET_FRAME_LENGTH
#endif
#ifndef XRT_MODULE_NET_FRAME
#define XRT_MODULE_NET_FRAME
#endif
#endif

/* net_frame_line 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_FRAME_LINE)
#ifndef XRT_FEATURE_NET_FRAME_LINE
#define XRT_FEATURE_NET_FRAME_LINE
#endif
#ifndef XRT_MODULE_NET_FRAME
#define XRT_MODULE_NET_FRAME
#endif
#endif

/* net_frame 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_FRAME)
#ifndef XRT_FEATURE_NET_FRAME
#define XRT_FEATURE_NET_FRAME
#endif
#ifndef XRT_MODULE_NET_BUFFER
#define XRT_MODULE_NET_BUFFER
#endif
#endif

/* net_resolver_future 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_RESOLVER_FUTURE)
#ifndef XRT_FEATURE_NET_RESOLVER_FUTURE
#define XRT_FEATURE_NET_RESOLVER_FUTURE
#endif
#ifndef XRT_MODULE_NET_RESOLVER
#define XRT_MODULE_NET_RESOLVER
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#ifndef XRT_MODULE_FUTURE_BRIDGE
#define XRT_MODULE_FUTURE_BRIDGE
#endif
#endif

/* net_resolver 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_RESOLVER)
#ifndef XRT_FEATURE_NET_RESOLVER
#define XRT_FEATURE_NET_RESOLVER
#endif
#ifndef XRT_MODULE_NET_DNS
#define XRT_MODULE_NET_DNS
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifndef XRT_MODULE_HASH64
#define XRT_MODULE_HASH64
#endif
#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#ifndef XRT_MODULE_COND
#define XRT_MODULE_COND
#endif
#endif

/* net_dns 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_DNS)
#ifndef XRT_FEATURE_NET_DNS
#define XRT_FEATURE_NET_DNS
#endif
#ifndef XRT_MODULE_NET
#define XRT_MODULE_NET
#endif
#endif

/* net_interface_text 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_INTERFACE_TEXT)
#ifndef XRT_FEATURE_NET_INTERFACE_TEXT
#define XRT_FEATURE_NET_INTERFACE_TEXT
#endif
#ifndef XRT_MODULE_NET_INTERFACE
#define XRT_MODULE_NET_INTERFACE
#endif
#ifndef XRT_MODULE_CODEC_HEX
#define XRT_MODULE_CODEC_HEX
#endif
#endif

/* net_interface 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_INTERFACE)
#ifndef XRT_FEATURE_NET_INTERFACE
#define XRT_FEATURE_NET_INTERFACE
#endif
#ifndef XRT_MODULE_NET
#define XRT_MODULE_NET
#endif
#endif

/* task_group_coroutine 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TASK_GROUP_COROUTINE)
#ifndef XRT_FEATURE_TASK_GROUP_COROUTINE
#define XRT_FEATURE_TASK_GROUP_COROUTINE
#endif
#ifndef XRT_MODULE_TASK_GROUP
#define XRT_MODULE_TASK_GROUP
#endif
#ifndef XRT_MODULE_TASK_COROUTINE
#define XRT_MODULE_TASK_COROUTINE
#endif
#endif

/* task_coroutine 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TASK_COROUTINE)
#ifndef XRT_FEATURE_TASK_COROUTINE
#define XRT_FEATURE_TASK_COROUTINE
#endif
#ifndef XRT_MODULE_TASK
#define XRT_MODULE_TASK
#endif
#ifndef XRT_MODULE_COROUTINE_SCHEDULER
#define XRT_MODULE_COROUTINE_SCHEDULER
#endif
#endif

/* future_coroutine 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_FUTURE_COROUTINE)
#ifndef XRT_FEATURE_FUTURE_COROUTINE
#define XRT_FEATURE_FUTURE_COROUTINE
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#ifndef XRT_MODULE_COROUTINE_SCHEDULER
#define XRT_MODULE_COROUTINE_SCHEDULER
#endif
#endif

/* coroutine_event 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_COROUTINE_EVENT)
#ifndef XRT_FEATURE_COROUTINE_EVENT
#define XRT_FEATURE_COROUTINE_EVENT
#endif
#ifndef XRT_MODULE_COROUTINE_SCHEDULER
#define XRT_MODULE_COROUTINE_SCHEDULER
#endif
#endif

/* task_group_pool 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TASK_GROUP_POOL)
#ifndef XRT_FEATURE_TASK_GROUP_POOL
#define XRT_FEATURE_TASK_GROUP_POOL
#endif
#ifndef XRT_MODULE_TASK_GROUP
#define XRT_MODULE_TASK_GROUP
#endif
#ifndef XRT_MODULE_TASK_POOL
#define XRT_MODULE_TASK_POOL
#endif
#endif

/* executor 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_EXECUTOR)
#ifndef XRT_FEATURE_EXECUTOR
#define XRT_FEATURE_EXECUTOR
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifndef XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_TEMP_MEMORY
#endif
#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#ifndef XRT_MODULE_COND
#define XRT_MODULE_COND
#endif
#endif

/* task_pool 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TASK_POOL)
#ifndef XRT_FEATURE_TASK_POOL
#define XRT_FEATURE_TASK_POOL
#endif
#ifndef XRT_MODULE_TASK
#define XRT_MODULE_TASK
#endif
#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#endif

/* task_group 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TASK_GROUP)
#ifndef XRT_FEATURE_TASK_GROUP
#define XRT_FEATURE_TASK_GROUP
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#endif

/* task 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TASK)
#ifndef XRT_FEATURE_TASK
#define XRT_FEATURE_TASK
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#ifndef XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_TEMP_MEMORY
#endif
#endif

/* future_combine 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_FUTURE_COMBINE)
#ifndef XRT_FEATURE_FUTURE_COMBINE
#define XRT_FEATURE_FUTURE_COMBINE
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#endif

/* future_continue 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_FUTURE_CONTINUE)
#ifndef XRT_FEATURE_FUTURE_CONTINUE
#define XRT_FEATURE_FUTURE_CONTINUE
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#endif

/* future_bridge 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_FUTURE_BRIDGE)
#ifndef XRT_FEATURE_FUTURE_BRIDGE
#define XRT_FEATURE_FUTURE_BRIDGE
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#endif

/* future 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_FUTURE)
#ifndef XRT_FEATURE_FUTURE
#define XRT_FEATURE_FUTURE
#endif
#ifndef XRT_MODULE_CANCEL
#define XRT_MODULE_CANCEL
#endif
#endif

/* thread_key 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_THREAD_KEY)
#ifndef XRT_FEATURE_THREAD_KEY
#define XRT_FEATURE_THREAD_KEY
#endif
#endif

/* once 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_ONCE)
#ifndef XRT_FEATURE_ONCE
#define XRT_FEATURE_ONCE
#endif
#endif

/* tls_record_chacha 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_RECORD_CHACHA)
#ifndef XRT_FEATURE_TLS_RECORD_CHACHA
#define XRT_FEATURE_TLS_RECORD_CHACHA
#endif
#ifndef XRT_MODULE_TLS_RECORD
#define XRT_MODULE_TLS_RECORD
#endif
#ifndef XRT_MODULE_CRYPTO_CHACHA20_POLY1305
#define XRT_MODULE_CRYPTO_CHACHA20_POLY1305
#endif
#endif

/* tls_schedule_sha384 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_SCHEDULE_SHA384)
#ifndef XRT_FEATURE_TLS_SCHEDULE_SHA384
#define XRT_FEATURE_TLS_SCHEDULE_SHA384
#endif
#ifndef XRT_MODULE_TLS_SCHEDULE
#define XRT_MODULE_TLS_SCHEDULE
#endif
#ifndef XRT_MODULE_CRYPTO_HKDF_SHA512
#define XRT_MODULE_CRYPTO_HKDF_SHA512
#endif
#endif

/* tls_key_exchange_p384 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_KEY_EXCHANGE_P384)
#ifndef XRT_FEATURE_TLS_KEY_EXCHANGE_P384
#define XRT_FEATURE_TLS_KEY_EXCHANGE_P384
#endif
#ifndef XRT_MODULE_TLS_KEY_EXCHANGE
#define XRT_MODULE_TLS_KEY_EXCHANGE
#endif
#ifndef XRT_MODULE_CRYPTO_P384_KEYPAIR
#define XRT_MODULE_CRYPTO_P384_KEYPAIR
#endif
#endif

/* tls_key_exchange_p256 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_KEY_EXCHANGE_P256)
#ifndef XRT_FEATURE_TLS_KEY_EXCHANGE_P256
#define XRT_FEATURE_TLS_KEY_EXCHANGE_P256
#endif
#ifndef XRT_MODULE_TLS_KEY_EXCHANGE
#define XRT_MODULE_TLS_KEY_EXCHANGE
#endif
#ifndef XRT_MODULE_CRYPTO_P256_KEYPAIR
#define XRT_MODULE_CRYPTO_P256_KEYPAIR
#endif
#endif

/* tls_key_exchange_x448 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_KEY_EXCHANGE_X448)
#ifndef XRT_FEATURE_TLS_KEY_EXCHANGE_X448
#define XRT_FEATURE_TLS_KEY_EXCHANGE_X448
#endif
#ifndef XRT_MODULE_TLS_KEY_EXCHANGE
#define XRT_MODULE_TLS_KEY_EXCHANGE
#endif
#ifndef XRT_MODULE_CRYPTO_X448_KEYPAIR
#define XRT_MODULE_CRYPTO_X448_KEYPAIR
#endif
#endif

/* x509_store_system 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_X509_STORE_SYSTEM)
#ifndef XRT_FEATURE_X509_STORE_SYSTEM
#define XRT_FEATURE_X509_STORE_SYSTEM
#endif
#ifndef XRT_MODULE_X509_STORE
#define XRT_MODULE_X509_STORE
#endif
#if defined(_WIN32)
#endif
#if defined(__APPLE__) && defined(__MACH__)
#endif
#if (defined(__linux__) && !defined(__ANDROID__)) || \
	(defined(__ANDROID__)) || \
	(defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)) || \
	(!defined(_WIN32) && !defined(__linux__) && !defined(__ANDROID__) && !(defined(__APPLE__) && defined(__MACH__)) && !defined(__FreeBSD__) && !defined(__OpenBSD__) && !defined(__NetBSD__) && !defined(__DragonFly__))
#ifndef XRT_MODULE_X509_STORE_FILE
#define XRT_MODULE_X509_STORE_FILE
#endif
#ifndef XRT_MODULE_DIR
#define XRT_MODULE_DIR
#endif
#endif
#endif

/* dir 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_DIR)
#ifndef XRT_FEATURE_DIR
#define XRT_FEATURE_DIR
#endif
#ifndef XRT_MODULE_FILE
#define XRT_MODULE_FILE
#endif
#endif

/* x509_store_file 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_X509_STORE_FILE)
#ifndef XRT_FEATURE_X509_STORE_FILE
#define XRT_FEATURE_X509_STORE_FILE
#endif
#ifndef XRT_MODULE_X509_STORE
#define XRT_MODULE_X509_STORE
#endif
#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#endif

/* file_whole 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_FILE_WHOLE)
#ifndef XRT_FEATURE_FILE_WHOLE
#define XRT_FEATURE_FILE_WHOLE
#endif
#ifndef XRT_MODULE_FILE_TEMP
#define XRT_MODULE_FILE_TEMP
#endif
#endif

/* file_temp 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_FILE_TEMP)
#ifndef XRT_FEATURE_FILE_TEMP
#define XRT_FEATURE_FILE_TEMP
#endif
#ifndef XRT_MODULE_FILE
#define XRT_MODULE_FILE
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#endif

/* file 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_FILE)
#ifndef XRT_FEATURE_FILE
#define XRT_FEATURE_FILE
#endif
#ifndef XRT_MODULE_PATH_SYSTEM
#define XRT_MODULE_PATH_SYSTEM
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#endif

/* path_system 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_PATH_SYSTEM)
#ifndef XRT_FEATURE_PATH_SYSTEM
#define XRT_FEATURE_PATH_SYSTEM
#endif
#ifndef XRT_MODULE_PATH
#define XRT_MODULE_PATH
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#endif

/* path 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_PATH)
#ifndef XRT_FEATURE_PATH
#define XRT_FEATURE_PATH
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#endif

/* x509_crl_policy 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_X509_CRL_POLICY)
#ifndef XRT_FEATURE_X509_CRL_POLICY
#define XRT_FEATURE_X509_CRL_POLICY
#endif
#ifndef XRT_MODULE_X509_CRL_PROFILE
#define XRT_MODULE_X509_CRL_PROFILE
#endif
#ifndef XRT_MODULE_X509_CRL_VERIFY
#define XRT_MODULE_X509_CRL_VERIFY
#endif
#ifndef XRT_MODULE_X509_NAME
#define XRT_MODULE_X509_NAME
#endif
#endif

/* x509_crl_profile 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_X509_CRL_PROFILE)
#ifndef XRT_FEATURE_X509_CRL_PROFILE
#define XRT_FEATURE_X509_CRL_PROFILE
#endif
#ifndef XRT_MODULE_X509_CRL
#define XRT_MODULE_X509_CRL
#endif
#ifndef XRT_MODULE_X509_DISTRIBUTION
#define XRT_MODULE_X509_DISTRIBUTION
#endif
#endif

/* x509_distribution 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_X509_DISTRIBUTION)
#ifndef XRT_FEATURE_X509_DISTRIBUTION
#define XRT_FEATURE_X509_DISTRIBUTION
#endif
#ifndef XRT_MODULE_X509_PROFILE
#define XRT_MODULE_X509_PROFILE
#endif
#endif

/* x509_crl_verify 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_X509_CRL_VERIFY)
#ifndef XRT_FEATURE_X509_CRL_VERIFY
#define XRT_FEATURE_X509_CRL_VERIFY
#endif
#ifndef XRT_MODULE_X509_CRL
#define XRT_MODULE_X509_CRL
#endif
#ifndef XRT_MODULE_X509_VERIFY
#define XRT_MODULE_X509_VERIFY
#endif
#endif

/* x509_crl 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_X509_CRL)
#ifndef XRT_FEATURE_X509_CRL
#define XRT_FEATURE_X509_CRL
#endif
#ifndef XRT_MODULE_X509_PARSE
#define XRT_MODULE_X509_PARSE
#endif
#endif

/* math 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_MATH)
#ifndef XRT_FEATURE_MATH
#define XRT_FEATURE_MATH
#endif
#endif

/* crypto_x448_keypair 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_X448_KEYPAIR)
#ifndef XRT_FEATURE_CRYPTO_X448_KEYPAIR
#define XRT_FEATURE_CRYPTO_X448_KEYPAIR
#endif
#ifndef XRT_MODULE_CRYPTO_X448
#define XRT_MODULE_CRYPTO_X448
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#endif

/* crypto_x448 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_X448)
#ifndef XRT_FEATURE_CRYPTO_X448
#define XRT_FEATURE_CRYPTO_X448
#endif
#ifndef XRT_MODULE_CRYPTO_CORE
#define XRT_MODULE_CRYPTO_CORE
#endif
#endif

/* crypto_ed25519_keypair 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_ED25519_KEYPAIR)
#ifndef XRT_FEATURE_CRYPTO_ED25519_KEYPAIR
#define XRT_FEATURE_CRYPTO_ED25519_KEYPAIR
#endif
#ifndef XRT_MODULE_CRYPTO_ED25519
#define XRT_MODULE_CRYPTO_ED25519
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#endif

/* crypto_ed25519_sign 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_ED25519_SIGN)
#ifndef XRT_FEATURE_CRYPTO_ED25519_SIGN
#define XRT_FEATURE_CRYPTO_ED25519_SIGN
#endif
#ifndef XRT_MODULE_CRYPTO_ED25519
#define XRT_MODULE_CRYPTO_ED25519
#endif
#endif

/* crypto_chacha20_poly1305 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_CHACHA20_POLY1305)
#ifndef XRT_FEATURE_CRYPTO_CHACHA20_POLY1305
#define XRT_FEATURE_CRYPTO_CHACHA20_POLY1305
#endif
#ifndef XRT_MODULE_CRYPTO_CHACHA20
#define XRT_MODULE_CRYPTO_CHACHA20
#endif
#ifndef XRT_MODULE_CRYPTO_POLY1305
#define XRT_MODULE_CRYPTO_POLY1305
#endif
#endif

/* crypto_poly1305 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_POLY1305)
#ifndef XRT_FEATURE_CRYPTO_POLY1305
#define XRT_FEATURE_CRYPTO_POLY1305
#endif
#ifndef XRT_MODULE_CRYPTO_CORE
#define XRT_MODULE_CRYPTO_CORE
#endif
#endif

/* crypto_chacha20 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_CHACHA20)
#ifndef XRT_FEATURE_CRYPTO_CHACHA20
#define XRT_FEATURE_CRYPTO_CHACHA20
#endif
#ifndef XRT_MODULE_CRYPTO_CORE
#define XRT_MODULE_CRYPTO_CORE
#endif
#endif

/* crypto_hkdf_sha512 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_HKDF_SHA512)
#ifndef XRT_FEATURE_CRYPTO_HKDF_SHA512
#define XRT_FEATURE_CRYPTO_HKDF_SHA512
#endif
#ifndef XRT_MODULE_CRYPTO_HMAC_SHA512
#define XRT_MODULE_CRYPTO_HMAC_SHA512
#endif
#endif

/* crypto_pbkdf2_sha512 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_PBKDF2_SHA512)
#ifndef XRT_FEATURE_CRYPTO_PBKDF2_SHA512
#define XRT_FEATURE_CRYPTO_PBKDF2_SHA512
#endif
#ifndef XRT_MODULE_CRYPTO_HMAC_SHA512
#define XRT_MODULE_CRYPTO_HMAC_SHA512
#endif
#endif

/* crypto_pbkdf2_sha256 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_PBKDF2_SHA256)
#ifndef XRT_FEATURE_CRYPTO_PBKDF2_SHA256
#define XRT_FEATURE_CRYPTO_PBKDF2_SHA256
#endif
#ifndef XRT_MODULE_CRYPTO_HMAC_SHA256
#define XRT_MODULE_CRYPTO_HMAC_SHA256
#endif
#endif

/* crypto_md5 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_MD5)
#ifndef XRT_FEATURE_CRYPTO_MD5
#define XRT_FEATURE_CRYPTO_MD5
#endif
#ifndef XRT_MODULE_CRYPTO_CORE
#define XRT_MODULE_CRYPTO_CORE
#endif
#endif

/* crypto_sha512_256 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_SHA512_256)
#ifndef XRT_FEATURE_CRYPTO_SHA512_256
#define XRT_FEATURE_CRYPTO_SHA512_256
#endif
#ifndef XRT_MODULE_CRYPTO_SHA512
#define XRT_MODULE_CRYPTO_SHA512
#endif
#endif

/* crypto_ecdsa_p384_sign_der 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_ECDSA_P384_SIGN_DER)
#ifndef XRT_FEATURE_CRYPTO_ECDSA_P384_SIGN_DER
#define XRT_FEATURE_CRYPTO_ECDSA_P384_SIGN_DER
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_P384_SIGN
#define XRT_MODULE_CRYPTO_ECDSA_P384_SIGN
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_SIGN_DER
#define XRT_MODULE_CRYPTO_ECDSA_SIGN_DER
#endif
#endif

/* crypto_ecdsa_p256_sign_der 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_ECDSA_P256_SIGN_DER)
#ifndef XRT_FEATURE_CRYPTO_ECDSA_P256_SIGN_DER
#define XRT_FEATURE_CRYPTO_ECDSA_P256_SIGN_DER
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_P256_SIGN
#define XRT_MODULE_CRYPTO_ECDSA_P256_SIGN
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_SIGN_DER
#define XRT_MODULE_CRYPTO_ECDSA_SIGN_DER
#endif
#endif

/* crypto_ecdsa_sign_der 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_ECDSA_SIGN_DER)
#ifndef XRT_FEATURE_CRYPTO_ECDSA_SIGN_DER
#define XRT_FEATURE_CRYPTO_ECDSA_SIGN_DER
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_SIGN
#define XRT_MODULE_CRYPTO_ECDSA_SIGN
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_DER
#define XRT_MODULE_CRYPTO_ECDSA_DER
#endif
#endif

/* crypto_ecdsa_p384_sign 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_ECDSA_P384_SIGN)
#ifndef XRT_FEATURE_CRYPTO_ECDSA_P384_SIGN
#define XRT_FEATURE_CRYPTO_ECDSA_P384_SIGN
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_SIGN
#define XRT_MODULE_CRYPTO_ECDSA_SIGN
#endif
#ifndef XRT_MODULE_CRYPTO_P384
#define XRT_MODULE_CRYPTO_P384
#endif
#ifndef XRT_MODULE_CRYPTO_HMAC_SHA512
#define XRT_MODULE_CRYPTO_HMAC_SHA512
#endif
#endif

/* crypto_hmac_sha512 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_HMAC_SHA512)
#ifndef XRT_FEATURE_CRYPTO_HMAC_SHA512
#define XRT_FEATURE_CRYPTO_HMAC_SHA512
#endif
#ifndef XRT_MODULE_CRYPTO_SHA512
#define XRT_MODULE_CRYPTO_SHA512
#endif
#endif

/* crypto_ecdsa_p256_sign 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_ECDSA_P256_SIGN)
#ifndef XRT_FEATURE_CRYPTO_ECDSA_P256_SIGN
#define XRT_FEATURE_CRYPTO_ECDSA_P256_SIGN
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_SIGN
#define XRT_MODULE_CRYPTO_ECDSA_SIGN
#endif
#ifndef XRT_MODULE_CRYPTO_P256
#define XRT_MODULE_CRYPTO_P256
#endif
#ifndef XRT_MODULE_CRYPTO_HMAC_SHA256
#define XRT_MODULE_CRYPTO_HMAC_SHA256
#endif
#endif

/* crypto_ecdsa_sign 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_ECDSA_SIGN)
#ifndef XRT_FEATURE_CRYPTO_ECDSA_SIGN
#define XRT_FEATURE_CRYPTO_ECDSA_SIGN
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_MATH
#define XRT_MODULE_CRYPTO_ECDSA_MATH
#endif
#endif

/* crypto_p384_keypair 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_P384_KEYPAIR)
#ifndef XRT_FEATURE_CRYPTO_P384_KEYPAIR
#define XRT_FEATURE_CRYPTO_P384_KEYPAIR
#endif
#ifndef XRT_MODULE_CRYPTO_P384
#define XRT_MODULE_CRYPTO_P384
#endif
#ifndef XRT_MODULE_CRYPTO_NIST_KEYPAIR
#define XRT_MODULE_CRYPTO_NIST_KEYPAIR
#endif
#endif

/* crypto_p256_keypair 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_P256_KEYPAIR)
#ifndef XRT_FEATURE_CRYPTO_P256_KEYPAIR
#define XRT_FEATURE_CRYPTO_P256_KEYPAIR
#endif
#ifndef XRT_MODULE_CRYPTO_P256
#define XRT_MODULE_CRYPTO_P256
#endif
#ifndef XRT_MODULE_CRYPTO_NIST_KEYPAIR
#define XRT_MODULE_CRYPTO_NIST_KEYPAIR
#endif
#endif

/* crypto_nist_keypair 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_NIST_KEYPAIR)
#ifndef XRT_FEATURE_CRYPTO_NIST_KEYPAIR
#define XRT_FEATURE_CRYPTO_NIST_KEYPAIR
#endif
#ifndef XRT_MODULE_CRYPTO_NIST
#define XRT_MODULE_CRYPTO_NIST
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#endif

/* random_text_default 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_RANDOM_TEXT_DEFAULT)
#ifndef XRT_FEATURE_RANDOM_TEXT_DEFAULT
#define XRT_FEATURE_RANDOM_TEXT_DEFAULT
#endif
#ifndef XRT_MODULE_RANDOM_TEXT
#define XRT_MODULE_RANDOM_TEXT
#endif
#ifndef XRT_MODULE_RANDOM_DEFAULT
#define XRT_MODULE_RANDOM_DEFAULT
#endif
#endif

/* random_text 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_RANDOM_TEXT)
#ifndef XRT_FEATURE_RANDOM_TEXT
#define XRT_FEATURE_RANDOM_TEXT
#endif
#ifndef XRT_MODULE_RANDOM
#define XRT_MODULE_RANDOM
#endif
#endif

/* random_secure_text 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_RANDOM_SECURE_TEXT)
#ifndef XRT_FEATURE_RANDOM_SECURE_TEXT
#define XRT_FEATURE_RANDOM_SECURE_TEXT
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#endif

/* random_default 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_RANDOM_DEFAULT)
#ifndef XRT_FEATURE_RANDOM_DEFAULT
#define XRT_FEATURE_RANDOM_DEFAULT
#endif
#ifndef XRT_MODULE_RANDOM
#define XRT_MODULE_RANDOM
#endif
#endif

/* random 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_RANDOM)
#ifndef XRT_FEATURE_RANDOM
#define XRT_FEATURE_RANDOM
#endif
#endif

/* hash_keyed 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_HASH_KEYED)
#ifndef XRT_FEATURE_HASH_KEYED
#define XRT_FEATURE_HASH_KEYED
#endif
#endif

/* hash64 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_HASH64)
#ifndef XRT_FEATURE_HASH64
#define XRT_FEATURE_HASH64
#endif
#endif

/* hash32 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_HASH32)
#ifndef XRT_FEATURE_HASH32
#define XRT_FEATURE_HASH32
#endif
#endif

/* charset_detect 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CHARSET_DETECT)
#ifndef XRT_FEATURE_CHARSET_DETECT
#define XRT_FEATURE_CHARSET_DETECT
#endif
#ifndef XRT_MODULE_CHARSET
#define XRT_MODULE_CHARSET
#endif
#endif

/* charset 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CHARSET)
#ifndef XRT_FEATURE_CHARSET
#define XRT_FEATURE_CHARSET
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#endif

/* unicode_text 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_UNICODE_TEXT)
#ifndef XRT_FEATURE_UNICODE_TEXT
#define XRT_FEATURE_UNICODE_TEXT
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#endif

/* http1_message 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_HTTP1_MESSAGE)
#ifndef XRT_FEATURE_HTTP1_MESSAGE
#define XRT_FEATURE_HTTP1_MESSAGE
#endif
#ifndef XRT_MODULE_HTTP1_BODY
#define XRT_MODULE_HTTP1_BODY
#endif
#endif

/* http1_body 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_HTTP1_BODY)
#ifndef XRT_FEATURE_HTTP1_BODY
#define XRT_FEATURE_HTTP1_BODY
#endif
#ifndef XRT_MODULE_HTTP1_HEAD
#define XRT_MODULE_HTTP1_HEAD
#endif
#ifndef XRT_MODULE_HTTP_TRAILER
#define XRT_MODULE_HTTP_TRAILER
#endif
#endif

/* http_target 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_HTTP_TARGET)
#ifndef XRT_FEATURE_HTTP_TARGET
#define XRT_FEATURE_HTTP_TARGET
#endif
#ifndef XRT_MODULE_HTTP_HOST
#define XRT_MODULE_HTTP_HOST
#endif
#endif

/* http_param_host 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_HTTP_PARAM_HOST)
#ifndef XRT_FEATURE_HTTP_PARAM_HOST
#define XRT_FEATURE_HTTP_PARAM_HOST
#endif
#ifndef XRT_MODULE_HTTP_PARAM
#define XRT_MODULE_HTTP_PARAM
#endif
#ifndef XRT_MODULE_HTTP_HOST
#define XRT_MODULE_HTTP_HOST
#endif
#endif

/* http_trailer 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_HTTP_TRAILER)
#ifndef XRT_FEATURE_HTTP_TRAILER
#define XRT_FEATURE_HTTP_TRAILER
#endif
#ifndef XRT_MODULE_HTTP
#define XRT_MODULE_HTTP
#endif
#endif

/* http_connection 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_HTTP_CONNECTION)
#ifndef XRT_FEATURE_HTTP_CONNECTION
#define XRT_FEATURE_HTTP_CONNECTION
#endif
#ifndef XRT_MODULE_HTTP
#define XRT_MODULE_HTTP
#endif
#endif

/* http_decode 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_HTTP_DECODE)
#ifndef XRT_FEATURE_HTTP_DECODE
#define XRT_FEATURE_HTTP_DECODE
#endif
#ifndef XRT_MODULE_HTTP_ENCODING
#define XRT_MODULE_HTTP_ENCODING
#endif
#ifndef XRT_MODULE_INFLATE
#define XRT_MODULE_INFLATE
#endif
#endif

/* http_encoding 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_HTTP_ENCODING)
#ifndef XRT_FEATURE_HTTP_ENCODING
#define XRT_FEATURE_HTTP_ENCODING
#endif
#ifndef XRT_MODULE_HTTP
#define XRT_MODULE_HTTP
#endif
#endif

/* http_upgrade_write 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_HTTP_UPGRADE_WRITE)
#ifndef XRT_FEATURE_HTTP_UPGRADE_WRITE
#define XRT_FEATURE_HTTP_UPGRADE_WRITE
#endif
#ifndef XRT_MODULE_HTTP_UPGRADE
#define XRT_MODULE_HTTP_UPGRADE
#endif
#endif

/* http_te 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_HTTP_TE)
#ifndef XRT_FEATURE_HTTP_TE
#define XRT_FEATURE_HTTP_TE
#endif
#ifndef XRT_MODULE_HTTP_PARAM
#define XRT_MODULE_HTTP_PARAM
#endif
#endif

/* http_expect 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_HTTP_EXPECT)
#ifndef XRT_FEATURE_HTTP_EXPECT
#define XRT_FEATURE_HTTP_EXPECT
#endif
#ifndef XRT_MODULE_HTTP
#define XRT_MODULE_HTTP
#endif
#endif

/* http1_tls 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_HTTP1_TLS)
#ifndef XRT_FEATURE_HTTP1_TLS
#define XRT_FEATURE_HTTP1_TLS
#endif
#ifndef XRT_MODULE_HTTP1_NET
#define XRT_MODULE_HTTP1_NET
#endif
#ifndef XRT_MODULE_TLS_STREAM
#define XRT_MODULE_TLS_STREAM
#endif
#endif

/* websocket_upgrade_stream 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_WEBSOCKET_UPGRADE_STREAM)
#ifndef XRT_FEATURE_WEBSOCKET_UPGRADE_STREAM
#define XRT_FEATURE_WEBSOCKET_UPGRADE_STREAM
#endif
#ifndef XRT_MODULE_WEBSOCKET_UPGRADE
#define XRT_MODULE_WEBSOCKET_UPGRADE
#endif
#ifndef XRT_MODULE_WEBSOCKET_STREAM
#define XRT_MODULE_WEBSOCKET_STREAM
#endif
#ifndef XRT_MODULE_HTTP1_NET
#define XRT_MODULE_HTTP1_NET
#endif
#endif

/* http1_net 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_HTTP1_NET)
#ifndef XRT_FEATURE_HTTP1_NET
#define XRT_FEATURE_HTTP1_NET
#endif
#ifndef XRT_MODULE_HTTP1_HEAD
#define XRT_MODULE_HTTP1_HEAD
#endif
#ifndef XRT_MODULE_NET_BUFFER
#define XRT_MODULE_NET_BUFFER
#endif
#endif

/* websocket_upgrade_deflate 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_WEBSOCKET_UPGRADE_DEFLATE)
#ifndef XRT_FEATURE_WEBSOCKET_UPGRADE_DEFLATE
#define XRT_FEATURE_WEBSOCKET_UPGRADE_DEFLATE
#endif
#ifndef XRT_MODULE_WEBSOCKET_UPGRADE
#define XRT_MODULE_WEBSOCKET_UPGRADE
#endif
#ifndef XRT_MODULE_WEBSOCKET_DEFLATE
#define XRT_MODULE_WEBSOCKET_DEFLATE
#endif
#endif

/* websocket_upgrade 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_WEBSOCKET_UPGRADE)
#ifndef XRT_FEATURE_WEBSOCKET_UPGRADE
#define XRT_FEATURE_WEBSOCKET_UPGRADE
#endif
#ifndef XRT_MODULE_WEBSOCKET_KEYGEN
#define XRT_MODULE_WEBSOCKET_KEYGEN
#endif
#ifndef XRT_MODULE_HTTP1_HEAD
#define XRT_MODULE_HTTP1_HEAD
#endif
#ifndef XRT_MODULE_HTTP_HOST
#define XRT_MODULE_HTTP_HOST
#endif
#endif

/* http_host 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_HTTP_HOST)
#ifndef XRT_FEATURE_HTTP_HOST
#define XRT_FEATURE_HTTP_HOST
#endif
#ifndef XRT_MODULE_HTTP
#define XRT_MODULE_HTTP
#endif
#endif

/* http1_head 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_HTTP1_HEAD)
#ifndef XRT_FEATURE_HTTP1_HEAD
#define XRT_FEATURE_HTTP1_HEAD
#endif
#ifndef XRT_MODULE_HTTP_UPGRADE
#define XRT_MODULE_HTTP_UPGRADE
#endif
#endif

/* http_upgrade 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_HTTP_UPGRADE)
#ifndef XRT_FEATURE_HTTP_UPGRADE
#define XRT_FEATURE_HTTP_UPGRADE
#endif
#ifndef XRT_MODULE_HTTP
#define XRT_MODULE_HTTP
#endif
#endif

/* websocket_stream_deflate 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_WEBSOCKET_STREAM_DEFLATE)
#ifndef XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE
#define XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE
#endif
#ifndef XRT_MODULE_WEBSOCKET_STREAM
#define XRT_MODULE_WEBSOCKET_STREAM
#endif
#ifndef XRT_MODULE_WEBSOCKET_INFLATER
#define XRT_MODULE_WEBSOCKET_INFLATER
#endif
#ifndef XRT_MODULE_WEBSOCKET_DEFLATER
#define XRT_MODULE_WEBSOCKET_DEFLATER
#endif
#endif

/* tls_identity_rsa 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_IDENTITY_RSA)
#ifndef XRT_FEATURE_TLS_IDENTITY_RSA
#define XRT_FEATURE_TLS_IDENTITY_RSA
#endif
#ifndef XRT_MODULE_TLS_IDENTITY
#define XRT_MODULE_TLS_IDENTITY
#endif
#ifndef XRT_MODULE_X509_SIGNATURE
#define XRT_MODULE_X509_SIGNATURE
#endif
#ifndef XRT_MODULE_CRYPTO_RSA_PRIVATE
#define XRT_MODULE_CRYPTO_RSA_PRIVATE
#endif
#ifndef XRT_MODULE_CRYPTO_RSA_PSS_SIGN
#define XRT_MODULE_CRYPTO_RSA_PSS_SIGN
#endif
#ifndef XRT_MODULE_CRYPTO_RSA_PKCS1_SIGN
#define XRT_MODULE_CRYPTO_RSA_PKCS1_SIGN
#endif
#ifndef XRT_MODULE_CRYPTO_SHA256
#define XRT_MODULE_CRYPTO_SHA256
#endif
#ifndef XRT_MODULE_CRYPTO_SHA512
#define XRT_MODULE_CRYPTO_SHA512
#endif
#endif

/* crypto_rsa_pkcs1_sign 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_RSA_PKCS1_SIGN)
#ifndef XRT_FEATURE_CRYPTO_RSA_PKCS1_SIGN
#define XRT_FEATURE_CRYPTO_RSA_PKCS1_SIGN
#endif
#ifndef XRT_MODULE_CRYPTO_RSA_PKCS1
#define XRT_MODULE_CRYPTO_RSA_PKCS1
#endif
#ifndef XRT_MODULE_CRYPTO_RSA_PRIVATE
#define XRT_MODULE_CRYPTO_RSA_PRIVATE
#endif
#endif

/* crypto_rsa_pss_sign 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_RSA_PSS_SIGN)
#ifndef XRT_FEATURE_CRYPTO_RSA_PSS_SIGN
#define XRT_FEATURE_CRYPTO_RSA_PSS_SIGN
#endif
#ifndef XRT_MODULE_CRYPTO_RSA_PSS
#define XRT_MODULE_CRYPTO_RSA_PSS
#endif
#ifndef XRT_MODULE_CRYPTO_RSA_PRIVATE
#define XRT_MODULE_CRYPTO_RSA_PRIVATE
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#endif

/* crypto_rsa_private 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_RSA_PRIVATE)
#ifndef XRT_FEATURE_CRYPTO_RSA_PRIVATE
#define XRT_FEATURE_CRYPTO_RSA_PRIVATE
#endif
#ifndef XRT_MODULE_CRYPTO_RSA
#define XRT_MODULE_CRYPTO_RSA
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#endif

/* tls_record_aes 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_RECORD_AES)
#ifndef XRT_FEATURE_TLS_RECORD_AES
#define XRT_FEATURE_TLS_RECORD_AES
#endif
#ifndef XRT_MODULE_TLS_RECORD
#define XRT_MODULE_TLS_RECORD
#endif
#ifndef XRT_MODULE_CRYPTO_AES_GCM
#define XRT_MODULE_CRYPTO_AES_GCM
#endif
#endif

/* crypto_aes_gcm 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_AES_GCM)
#ifndef XRT_FEATURE_CRYPTO_AES_GCM
#define XRT_FEATURE_CRYPTO_AES_GCM
#endif
#ifndef XRT_MODULE_CRYPTO_AES
#define XRT_MODULE_CRYPTO_AES
#endif
#endif

/* crypto_aes 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_AES)
#ifndef XRT_FEATURE_CRYPTO_AES
#define XRT_FEATURE_CRYPTO_AES
#endif
#ifndef XRT_MODULE_CRYPTO_CORE
#define XRT_MODULE_CRYPTO_CORE
#endif
#endif

/* tls_key_exchange_x25519 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_KEY_EXCHANGE_X25519)
#ifndef XRT_FEATURE_TLS_KEY_EXCHANGE_X25519
#define XRT_FEATURE_TLS_KEY_EXCHANGE_X25519
#endif
#ifndef XRT_MODULE_TLS_KEY_EXCHANGE
#define XRT_MODULE_TLS_KEY_EXCHANGE
#endif
#ifndef XRT_MODULE_CRYPTO_X25519_KEYPAIR
#define XRT_MODULE_CRYPTO_X25519_KEYPAIR
#endif
#endif

/* crypto_x25519_keypair 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_X25519_KEYPAIR)
#ifndef XRT_FEATURE_CRYPTO_X25519_KEYPAIR
#define XRT_FEATURE_CRYPTO_X25519_KEYPAIR
#endif
#ifndef XRT_MODULE_CRYPTO_X25519
#define XRT_MODULE_CRYPTO_X25519
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#endif

/* crypto_x25519 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_X25519)
#ifndef XRT_FEATURE_CRYPTO_X25519
#define XRT_FEATURE_CRYPTO_X25519
#endif
#ifndef XRT_MODULE_CRYPTO_CURVE25519
#define XRT_MODULE_CRYPTO_CURVE25519
#endif
#endif

/* tls_schedule_sha256 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_SCHEDULE_SHA256)
#ifndef XRT_FEATURE_TLS_SCHEDULE_SHA256
#define XRT_FEATURE_TLS_SCHEDULE_SHA256
#endif
#ifndef XRT_MODULE_TLS_SCHEDULE
#define XRT_MODULE_TLS_SCHEDULE
#endif
#ifndef XRT_MODULE_CRYPTO_HKDF_SHA256
#define XRT_MODULE_CRYPTO_HKDF_SHA256
#endif
#endif

/* crypto_hkdf_sha256 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_HKDF_SHA256)
#ifndef XRT_FEATURE_CRYPTO_HKDF_SHA256
#define XRT_FEATURE_CRYPTO_HKDF_SHA256
#endif
#ifndef XRT_MODULE_CRYPTO_HMAC_SHA256
#define XRT_MODULE_CRYPTO_HMAC_SHA256
#endif
#endif

/* crypto_hmac_sha256 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_HMAC_SHA256)
#ifndef XRT_FEATURE_CRYPTO_HMAC_SHA256
#define XRT_FEATURE_CRYPTO_HMAC_SHA256
#endif
#ifndef XRT_MODULE_CRYPTO_SHA256
#define XRT_MODULE_CRYPTO_SHA256
#endif
#endif

/* tls_server_resume 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_SERVER_RESUME)
#ifndef XRT_FEATURE_TLS_SERVER_RESUME
#define XRT_FEATURE_TLS_SERVER_RESUME
#endif
#ifndef XRT_MODULE_TLS_SERVER
#define XRT_MODULE_TLS_SERVER
#endif
#ifndef XRT_MODULE_TLS_RESUME
#define XRT_MODULE_TLS_RESUME
#endif
#ifndef XRT_MODULE_TLS_PSK_WRITE
#define XRT_MODULE_TLS_PSK_WRITE
#endif
#endif

/* tls_client_resume 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_CLIENT_RESUME)
#ifndef XRT_FEATURE_TLS_CLIENT_RESUME
#define XRT_FEATURE_TLS_CLIENT_RESUME
#endif
#ifndef XRT_MODULE_TLS_CLIENT_VERIFY
#define XRT_MODULE_TLS_CLIENT_VERIFY
#endif
#ifndef XRT_MODULE_TLS_RESUME
#define XRT_MODULE_TLS_RESUME
#endif
#ifndef XRT_MODULE_TLS_PSK_WRITE
#define XRT_MODULE_TLS_PSK_WRITE
#endif
#ifndef XRT_MODULE_CRYPTO_SHA256
#define XRT_MODULE_CRYPTO_SHA256
#endif
#endif

/* tls_psk_write 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_PSK_WRITE)
#ifndef XRT_FEATURE_TLS_PSK_WRITE
#define XRT_FEATURE_TLS_PSK_WRITE
#endif
#ifndef XRT_MODULE_TLS_PSK
#define XRT_MODULE_TLS_PSK
#endif
#ifndef XRT_MODULE_TLS_HELLO_WRITE
#define XRT_MODULE_TLS_HELLO_WRITE
#endif
#endif

/* tls_psk 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_PSK)
#ifndef XRT_FEATURE_TLS_PSK
#define XRT_FEATURE_TLS_PSK
#endif
#ifndef XRT_MODULE_TLS_HELLO
#define XRT_MODULE_TLS_HELLO
#endif
#endif

/* tls_resume 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_RESUME)
#ifndef XRT_FEATURE_TLS_RESUME
#define XRT_FEATURE_TLS_RESUME
#endif
#ifndef XRT_MODULE_TLS
#define XRT_MODULE_TLS
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#ifndef XRT_MODULE_CRYPTO_CORE
#define XRT_MODULE_CRYPTO_CORE
#endif
#endif

/* tls_client_verify 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_CLIENT_VERIFY)
#ifndef XRT_FEATURE_TLS_CLIENT_VERIFY
#define XRT_FEATURE_TLS_CLIENT_VERIFY
#endif
#ifndef XRT_MODULE_TLS_CLIENT
#define XRT_MODULE_TLS_CLIENT
#endif
#ifndef XRT_MODULE_TLS_VERIFY
#define XRT_MODULE_TLS_VERIFY
#endif
#ifndef XRT_MODULE_TLS_AUTH_MESSAGES_WRITE
#define XRT_MODULE_TLS_AUTH_MESSAGES_WRITE
#endif
#ifndef XRT_MODULE_X509_VERIFY_RSA
#define XRT_MODULE_X509_VERIFY_RSA
#endif
#ifndef XRT_MODULE_X509_VERIFY_ECDSA
#define XRT_MODULE_X509_VERIFY_ECDSA
#endif
#ifndef XRT_MODULE_X509_VERIFY_ED25519
#define XRT_MODULE_X509_VERIFY_ED25519
#endif
#endif

/* x509_verify_ed25519 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_X509_VERIFY_ED25519)
#ifndef XRT_FEATURE_X509_VERIFY_ED25519
#define XRT_FEATURE_X509_VERIFY_ED25519
#endif
#ifndef XRT_MODULE_X509_VERIFY
#define XRT_MODULE_X509_VERIFY
#endif
#ifndef XRT_MODULE_CRYPTO_ED25519_VERIFY
#define XRT_MODULE_CRYPTO_ED25519_VERIFY
#endif
#endif

/* crypto_ed25519_verify 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_ED25519_VERIFY)
#ifndef XRT_FEATURE_CRYPTO_ED25519_VERIFY
#define XRT_FEATURE_CRYPTO_ED25519_VERIFY
#endif
#ifndef XRT_MODULE_CRYPTO_ED25519
#define XRT_MODULE_CRYPTO_ED25519
#endif
#endif

/* crypto_ed25519 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_ED25519)
#ifndef XRT_FEATURE_CRYPTO_ED25519
#define XRT_FEATURE_CRYPTO_ED25519
#endif
#ifndef XRT_MODULE_CRYPTO_CURVE25519
#define XRT_MODULE_CRYPTO_CURVE25519
#endif
#ifndef XRT_MODULE_CRYPTO_SHA512
#define XRT_MODULE_CRYPTO_SHA512
#endif
#endif

/* crypto_curve25519 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_CURVE25519)
#ifndef XRT_FEATURE_CRYPTO_CURVE25519
#define XRT_FEATURE_CRYPTO_CURVE25519
#endif
#ifndef XRT_MODULE_CRYPTO_CORE
#define XRT_MODULE_CRYPTO_CORE
#endif
#endif

/* x509_verify_ecdsa 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_X509_VERIFY_ECDSA)
#ifndef XRT_FEATURE_X509_VERIFY_ECDSA
#define XRT_FEATURE_X509_VERIFY_ECDSA
#endif
#ifndef XRT_MODULE_X509_VERIFY
#define XRT_MODULE_X509_VERIFY
#endif
#ifndef XRT_MODULE_X509_DIGEST
#define XRT_MODULE_X509_DIGEST
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_P256_DER
#define XRT_MODULE_CRYPTO_ECDSA_P256_DER
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_P384_DER
#define XRT_MODULE_CRYPTO_ECDSA_P384_DER
#endif
#endif

/* crypto_ecdsa_p384_der 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_ECDSA_P384_DER)
#ifndef XRT_FEATURE_CRYPTO_ECDSA_P384_DER
#define XRT_FEATURE_CRYPTO_ECDSA_P384_DER
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_P384
#define XRT_MODULE_CRYPTO_ECDSA_P384
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_VERIFY_DER
#define XRT_MODULE_CRYPTO_ECDSA_VERIFY_DER
#endif
#endif

/* crypto_ecdsa_p384 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_ECDSA_P384)
#ifndef XRT_FEATURE_CRYPTO_ECDSA_P384
#define XRT_FEATURE_CRYPTO_ECDSA_P384
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_VERIFY
#define XRT_MODULE_CRYPTO_ECDSA_VERIFY
#endif
#ifndef XRT_MODULE_CRYPTO_P384
#define XRT_MODULE_CRYPTO_P384
#endif
#endif

/* crypto_p384 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_P384)
#ifndef XRT_FEATURE_CRYPTO_P384
#define XRT_FEATURE_CRYPTO_P384
#endif
#ifndef XRT_MODULE_CRYPTO_NIST
#define XRT_MODULE_CRYPTO_NIST
#endif
#endif

/* crypto_ecdsa_p256_der 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_ECDSA_P256_DER)
#ifndef XRT_FEATURE_CRYPTO_ECDSA_P256_DER
#define XRT_FEATURE_CRYPTO_ECDSA_P256_DER
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_P256
#define XRT_MODULE_CRYPTO_ECDSA_P256
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_VERIFY_DER
#define XRT_MODULE_CRYPTO_ECDSA_VERIFY_DER
#endif
#endif

/* crypto_ecdsa_verify_der 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_ECDSA_VERIFY_DER)
#ifndef XRT_FEATURE_CRYPTO_ECDSA_VERIFY_DER
#define XRT_FEATURE_CRYPTO_ECDSA_VERIFY_DER
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_VERIFY
#define XRT_MODULE_CRYPTO_ECDSA_VERIFY
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_DER
#define XRT_MODULE_CRYPTO_ECDSA_DER
#endif
#endif

/* crypto_ecdsa_der 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_ECDSA_DER)
#ifndef XRT_FEATURE_CRYPTO_ECDSA_DER
#define XRT_FEATURE_CRYPTO_ECDSA_DER
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_CORE
#define XRT_MODULE_CRYPTO_ECDSA_CORE
#endif
#endif

/* crypto_ecdsa_p256 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_ECDSA_P256)
#ifndef XRT_FEATURE_CRYPTO_ECDSA_P256
#define XRT_FEATURE_CRYPTO_ECDSA_P256
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_VERIFY
#define XRT_MODULE_CRYPTO_ECDSA_VERIFY
#endif
#ifndef XRT_MODULE_CRYPTO_P256
#define XRT_MODULE_CRYPTO_P256
#endif
#endif

/* crypto_p256 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_P256)
#ifndef XRT_FEATURE_CRYPTO_P256
#define XRT_FEATURE_CRYPTO_P256
#endif
#ifndef XRT_MODULE_CRYPTO_NIST
#define XRT_MODULE_CRYPTO_NIST
#endif
#endif

/* crypto_ecdsa_verify 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_ECDSA_VERIFY)
#ifndef XRT_FEATURE_CRYPTO_ECDSA_VERIFY
#define XRT_FEATURE_CRYPTO_ECDSA_VERIFY
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_MATH
#define XRT_MODULE_CRYPTO_ECDSA_MATH
#endif
#endif

/* crypto_ecdsa_math 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_ECDSA_MATH)
#ifndef XRT_FEATURE_CRYPTO_ECDSA_MATH
#define XRT_FEATURE_CRYPTO_ECDSA_MATH
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_CORE
#define XRT_MODULE_CRYPTO_ECDSA_CORE
#endif
#ifndef XRT_MODULE_CRYPTO_NIST
#define XRT_MODULE_CRYPTO_NIST
#endif
#endif

/* crypto_nist 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_NIST)
#ifndef XRT_FEATURE_CRYPTO_NIST
#define XRT_FEATURE_CRYPTO_NIST
#endif
#ifndef XRT_MODULE_CRYPTO_INT31
#define XRT_MODULE_CRYPTO_INT31
#endif
#endif

/* crypto_ecdsa_core 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_ECDSA_CORE)
#ifndef XRT_FEATURE_CRYPTO_ECDSA_CORE
#define XRT_FEATURE_CRYPTO_ECDSA_CORE
#endif
#ifndef XRT_MODULE_CRYPTO_CORE
#define XRT_MODULE_CRYPTO_CORE
#endif
#endif

/* x509_verify_rsa 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_X509_VERIFY_RSA)
#ifndef XRT_FEATURE_X509_VERIFY_RSA
#define XRT_FEATURE_X509_VERIFY_RSA
#endif
#ifndef XRT_MODULE_X509_VERIFY
#define XRT_MODULE_X509_VERIFY
#endif
#ifndef XRT_MODULE_X509_DIGEST
#define XRT_MODULE_X509_DIGEST
#endif
#ifndef XRT_MODULE_CRYPTO_RSA_PSS
#define XRT_MODULE_CRYPTO_RSA_PSS
#endif
#ifndef XRT_MODULE_CRYPTO_RSA_PKCS1
#define XRT_MODULE_CRYPTO_RSA_PKCS1
#endif
#endif

/* crypto_rsa_pkcs1 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_RSA_PKCS1)
#ifndef XRT_FEATURE_CRYPTO_RSA_PKCS1
#define XRT_FEATURE_CRYPTO_RSA_PKCS1
#endif
#ifndef XRT_MODULE_CRYPTO_RSA
#define XRT_MODULE_CRYPTO_RSA
#endif
#endif

/* crypto_rsa_pss 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_RSA_PSS)
#ifndef XRT_FEATURE_CRYPTO_RSA_PSS
#define XRT_FEATURE_CRYPTO_RSA_PSS
#endif
#ifndef XRT_MODULE_CRYPTO_RSA
#define XRT_MODULE_CRYPTO_RSA
#endif
#ifndef XRT_MODULE_CRYPTO_SHA1
#define XRT_MODULE_CRYPTO_SHA1
#endif
#ifndef XRT_MODULE_CRYPTO_SHA224
#define XRT_MODULE_CRYPTO_SHA224
#endif
#ifndef XRT_MODULE_CRYPTO_SHA256
#define XRT_MODULE_CRYPTO_SHA256
#endif
#ifndef XRT_MODULE_CRYPTO_SHA512
#define XRT_MODULE_CRYPTO_SHA512
#endif
#endif

/* crypto_rsa 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_RSA)
#ifndef XRT_FEATURE_CRYPTO_RSA
#define XRT_FEATURE_CRYPTO_RSA
#endif
#ifndef XRT_MODULE_CRYPTO_INT31
#define XRT_MODULE_CRYPTO_INT31
#endif
#endif

/* crypto_int31 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_INT31)
#ifndef XRT_FEATURE_CRYPTO_INT31
#define XRT_FEATURE_CRYPTO_INT31
#endif
#ifndef XRT_MODULE_CRYPTO_CORE
#define XRT_MODULE_CRYPTO_CORE
#endif
#endif

/* x509_digest 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_X509_DIGEST)
#ifndef XRT_FEATURE_X509_DIGEST
#define XRT_FEATURE_X509_DIGEST
#endif
#ifndef XRT_MODULE_X509_SIGNATURE
#define XRT_MODULE_X509_SIGNATURE
#endif
#ifndef XRT_MODULE_CRYPTO_SHA1
#define XRT_MODULE_CRYPTO_SHA1
#endif
#ifndef XRT_MODULE_CRYPTO_SHA224
#define XRT_MODULE_CRYPTO_SHA224
#endif
#ifndef XRT_MODULE_CRYPTO_SHA256
#define XRT_MODULE_CRYPTO_SHA256
#endif
#ifndef XRT_MODULE_CRYPTO_SHA512
#define XRT_MODULE_CRYPTO_SHA512
#endif
#endif

/* crypto_sha512 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_SHA512)
#ifndef XRT_FEATURE_CRYPTO_SHA512
#define XRT_FEATURE_CRYPTO_SHA512
#endif
#ifndef XRT_MODULE_CRYPTO_CORE
#define XRT_MODULE_CRYPTO_CORE
#endif
#endif

/* crypto_sha224 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_SHA224)
#ifndef XRT_FEATURE_CRYPTO_SHA224
#define XRT_FEATURE_CRYPTO_SHA224
#endif
#ifndef XRT_MODULE_CRYPTO_SHA256
#define XRT_MODULE_CRYPTO_SHA256
#endif
#endif

/* crypto_sha256 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_SHA256)
#ifndef XRT_FEATURE_CRYPTO_SHA256
#define XRT_FEATURE_CRYPTO_SHA256
#endif
#ifndef XRT_MODULE_CRYPTO_CORE
#define XRT_MODULE_CRYPTO_CORE
#endif
#endif

/* tls_verify 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_VERIFY)
#ifndef XRT_FEATURE_TLS_VERIFY
#define XRT_FEATURE_TLS_VERIFY
#endif
#ifndef XRT_MODULE_TLS_NEGOTIATE
#define XRT_MODULE_TLS_NEGOTIATE
#endif
#ifndef XRT_MODULE_X509_STORE
#define XRT_MODULE_X509_STORE
#endif
#ifndef XRT_MODULE_X509_IDENTITY
#define XRT_MODULE_X509_IDENTITY
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifndef XRT_MODULE_CRYPTO_CORE
#define XRT_MODULE_CRYPTO_CORE
#endif
#endif

/* x509_identity 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_X509_IDENTITY)
#ifndef XRT_FEATURE_X509_IDENTITY
#define XRT_FEATURE_X509_IDENTITY
#endif
#ifndef XRT_MODULE_X509_PROFILE
#define XRT_MODULE_X509_PROFILE
#endif
#ifndef XRT_MODULE_NET
#define XRT_MODULE_NET
#endif
#endif

/* x509_store 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_X509_STORE)
#ifndef XRT_FEATURE_X509_STORE
#define XRT_FEATURE_X509_STORE
#endif
#ifndef XRT_MODULE_X509_PATH_BUILD
#define XRT_MODULE_X509_PATH_BUILD
#endif
#ifndef XRT_MODULE_PEM
#define XRT_MODULE_PEM
#endif
#endif

/* pem 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_PEM)
#ifndef XRT_FEATURE_PEM
#define XRT_FEATURE_PEM
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#endif

/* x509_path_build 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_X509_PATH_BUILD)
#ifndef XRT_FEATURE_X509_PATH_BUILD
#define XRT_FEATURE_X509_PATH_BUILD
#endif
#ifndef XRT_MODULE_X509_PATH
#define XRT_MODULE_X509_PATH
#endif
#endif

/* x509_path 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_X509_PATH)
#ifndef XRT_FEATURE_X509_PATH
#define XRT_FEATURE_X509_PATH
#endif
#ifndef XRT_MODULE_X509_NAME_CONSTRAINTS
#define XRT_MODULE_X509_NAME_CONSTRAINTS
#endif
#ifndef XRT_MODULE_X509_VERIFY
#define XRT_MODULE_X509_VERIFY
#endif
#endif

/* x509_verify 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_X509_VERIFY)
#ifndef XRT_FEATURE_X509_VERIFY
#define XRT_FEATURE_X509_VERIFY
#endif
#ifndef XRT_MODULE_X509_SIGNATURE
#define XRT_MODULE_X509_SIGNATURE
#endif
#endif

/* x509_signature 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_X509_SIGNATURE)
#ifndef XRT_FEATURE_X509_SIGNATURE
#define XRT_FEATURE_X509_SIGNATURE
#endif
#ifndef XRT_MODULE_X509_PARSE
#define XRT_MODULE_X509_PARSE
#endif
#endif

/* x509_name_constraints 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_X509_NAME_CONSTRAINTS)
#ifndef XRT_FEATURE_X509_NAME_CONSTRAINTS
#define XRT_FEATURE_X509_NAME_CONSTRAINTS
#endif
#ifndef XRT_MODULE_X509_PROFILE
#define XRT_MODULE_X509_PROFILE
#endif
#ifndef XRT_MODULE_X509_NAME
#define XRT_MODULE_X509_NAME
#endif
#endif

/* x509_name 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_X509_NAME)
#ifndef XRT_FEATURE_X509_NAME
#define XRT_FEATURE_X509_NAME
#endif
#ifndef XRT_MODULE_X509_PARSE
#define XRT_MODULE_X509_PARSE
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#endif

/* x509_profile 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_X509_PROFILE)
#ifndef XRT_FEATURE_X509_PROFILE
#define XRT_FEATURE_X509_PROFILE
#endif
#ifndef XRT_MODULE_X509_PARSE
#define XRT_MODULE_X509_PARSE
#endif
#endif

/* websocket_stream_tls 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_WEBSOCKET_STREAM_TLS)
#ifndef XRT_FEATURE_WEBSOCKET_STREAM_TLS
#define XRT_FEATURE_WEBSOCKET_STREAM_TLS
#endif
#ifndef XRT_MODULE_WEBSOCKET_STREAM
#define XRT_MODULE_WEBSOCKET_STREAM
#endif
#ifndef XRT_MODULE_TLS_STREAM
#define XRT_MODULE_TLS_STREAM
#endif
#endif

/* tls_stream 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_STREAM)
#ifndef XRT_FEATURE_TLS_STREAM
#define XRT_FEATURE_TLS_STREAM
#endif
#ifndef XRT_MODULE_NET_TCP
#define XRT_MODULE_NET_TCP
#endif
#ifndef XRT_MODULE_TLS_CLIENT
#define XRT_MODULE_TLS_CLIENT
#endif
#ifndef XRT_MODULE_TLS_SERVER
#define XRT_MODULE_TLS_SERVER
#endif
#endif

/* tls_server 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_SERVER)
#ifndef XRT_FEATURE_TLS_SERVER
#define XRT_FEATURE_TLS_SERVER
#endif
#ifndef XRT_MODULE_TLS_SESSION
#define XRT_MODULE_TLS_SESSION
#endif
#ifndef XRT_MODULE_TLS_HELLO_WRITE
#define XRT_MODULE_TLS_HELLO_WRITE
#endif
#ifndef XRT_MODULE_TLS_HANDSHAKE_READER
#define XRT_MODULE_TLS_HANDSHAKE_READER
#endif
#ifndef XRT_MODULE_TLS_MESSAGES_WRITE
#define XRT_MODULE_TLS_MESSAGES_WRITE
#endif
#ifndef XRT_MODULE_TLS_AUTH_MESSAGES_WRITE
#define XRT_MODULE_TLS_AUTH_MESSAGES_WRITE
#endif
#ifndef XRT_MODULE_TLS_SCHEDULE
#define XRT_MODULE_TLS_SCHEDULE
#endif
#ifndef XRT_MODULE_TLS_KEY_EXCHANGE
#define XRT_MODULE_TLS_KEY_EXCHANGE
#endif
#ifndef XRT_MODULE_TLS_IDENTITY
#define XRT_MODULE_TLS_IDENTITY
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#ifndef XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_TEMP_MEMORY
#endif
#endif

/* tls_identity 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_IDENTITY)
#ifndef XRT_FEATURE_TLS_IDENTITY
#define XRT_FEATURE_TLS_IDENTITY
#endif
#ifndef XRT_MODULE_TLS_NEGOTIATE
#define XRT_MODULE_TLS_NEGOTIATE
#endif
#ifndef XRT_MODULE_X509_PARSE
#define XRT_MODULE_X509_PARSE
#endif
#ifndef XRT_MODULE_CRYPTO_CORE
#define XRT_MODULE_CRYPTO_CORE
#endif
#endif

/* x509_parse 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_X509_PARSE)
#ifndef XRT_FEATURE_X509_PARSE
#define XRT_FEATURE_X509_PARSE
#endif
#ifndef XRT_MODULE_ASN1_DER
#define XRT_MODULE_ASN1_DER
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#endif

/* asn1_der 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_ASN1_DER)
#ifndef XRT_FEATURE_ASN1_DER
#define XRT_FEATURE_ASN1_DER
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#endif

/* buffer 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_BUFFER)
#ifndef XRT_FEATURE_BUFFER
#define XRT_FEATURE_BUFFER
#endif
#ifndef XRT_MODULE_ARRAY
#define XRT_MODULE_ARRAY
#endif
#endif

/* array 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_ARRAY)
#ifndef XRT_FEATURE_ARRAY
#define XRT_FEATURE_ARRAY
#endif
#endif

/* tls_auth_messages_write 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_AUTH_MESSAGES_WRITE)
#ifndef XRT_FEATURE_TLS_AUTH_MESSAGES_WRITE
#define XRT_FEATURE_TLS_AUTH_MESSAGES_WRITE
#endif
#ifndef XRT_MODULE_TLS_AUTH_MESSAGES
#define XRT_MODULE_TLS_AUTH_MESSAGES
#endif
#endif

/* tls_auth_messages 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_AUTH_MESSAGES)
#ifndef XRT_FEATURE_TLS_AUTH_MESSAGES
#define XRT_FEATURE_TLS_AUTH_MESSAGES
#endif
#ifndef XRT_MODULE_TLS_MESSAGES
#define XRT_MODULE_TLS_MESSAGES
#endif
#endif

/* tls_messages_write 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_MESSAGES_WRITE)
#ifndef XRT_FEATURE_TLS_MESSAGES_WRITE
#define XRT_FEATURE_TLS_MESSAGES_WRITE
#endif
#ifndef XRT_MODULE_TLS_MESSAGES
#define XRT_MODULE_TLS_MESSAGES
#endif
#endif

/* tls_client 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_CLIENT)
#ifndef XRT_FEATURE_TLS_CLIENT
#define XRT_FEATURE_TLS_CLIENT
#endif
#ifndef XRT_MODULE_TLS_SESSION
#define XRT_MODULE_TLS_SESSION
#endif
#ifndef XRT_MODULE_TLS_HELLO_WRITE
#define XRT_MODULE_TLS_HELLO_WRITE
#endif
#ifndef XRT_MODULE_TLS_HANDSHAKE_READER
#define XRT_MODULE_TLS_HANDSHAKE_READER
#endif
#ifndef XRT_MODULE_TLS_MESSAGES
#define XRT_MODULE_TLS_MESSAGES
#endif
#ifndef XRT_MODULE_TLS_SCHEDULE
#define XRT_MODULE_TLS_SCHEDULE
#endif
#ifndef XRT_MODULE_TLS_KEY_EXCHANGE
#define XRT_MODULE_TLS_KEY_EXCHANGE
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#endif

/* tls_handshake_reader 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_HANDSHAKE_READER)
#ifndef XRT_FEATURE_TLS_HANDSHAKE_READER
#define XRT_FEATURE_TLS_HANDSHAKE_READER
#endif
#ifndef XRT_MODULE_TLS_HANDSHAKE
#define XRT_MODULE_TLS_HANDSHAKE
#endif
#endif

/* tls_hello_write 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_HELLO_WRITE)
#ifndef XRT_FEATURE_TLS_HELLO_WRITE
#define XRT_FEATURE_TLS_HELLO_WRITE
#endif
#ifndef XRT_MODULE_TLS_HELLO
#define XRT_MODULE_TLS_HELLO
#endif
#endif

/* tls_session 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_SESSION)
#ifndef XRT_FEATURE_TLS_SESSION
#define XRT_FEATURE_TLS_SESSION
#endif
#ifndef XRT_MODULE_TLS_CONTEXT
#define XRT_MODULE_TLS_CONTEXT
#endif
#ifndef XRT_MODULE_TLS_RECORD
#define XRT_MODULE_TLS_RECORD
#endif
#ifndef XRT_MODULE_TLS_MESSAGES
#define XRT_MODULE_TLS_MESSAGES
#endif
#ifndef XRT_MODULE_TLS_SCHEDULE
#define XRT_MODULE_TLS_SCHEDULE
#endif
#ifndef XRT_MODULE_NET_BUFFER
#define XRT_MODULE_NET_BUFFER
#endif
#ifndef XRT_MODULE_CRYPTO_CORE
#define XRT_MODULE_CRYPTO_CORE
#endif
#endif

/* tls_schedule 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_SCHEDULE)
#ifndef XRT_FEATURE_TLS_SCHEDULE
#define XRT_FEATURE_TLS_SCHEDULE
#endif
#ifndef XRT_MODULE_TLS
#define XRT_MODULE_TLS
#endif
#ifndef XRT_MODULE_CRYPTO_CORE
#define XRT_MODULE_CRYPTO_CORE
#endif
#endif

/* tls_messages 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_MESSAGES)
#ifndef XRT_FEATURE_TLS_MESSAGES
#define XRT_FEATURE_TLS_MESSAGES
#endif
#ifndef XRT_MODULE_TLS_HELLO
#define XRT_MODULE_TLS_HELLO
#endif
#endif

/* tls_record 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_RECORD)
#ifndef XRT_FEATURE_TLS_RECORD
#define XRT_FEATURE_TLS_RECORD
#endif
#ifndef XRT_MODULE_TLS
#define XRT_MODULE_TLS
#endif
#ifndef XRT_MODULE_CRYPTO_CORE
#define XRT_MODULE_CRYPTO_CORE
#endif
#endif

/* tls_context 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_CONTEXT)
#ifndef XRT_FEATURE_TLS_CONTEXT
#define XRT_FEATURE_TLS_CONTEXT
#endif
#ifndef XRT_MODULE_TLS_POLICY
#define XRT_MODULE_TLS_POLICY
#endif
#endif

/* tls_policy 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_POLICY)
#ifndef XRT_FEATURE_TLS_POLICY
#define XRT_FEATURE_TLS_POLICY
#endif
#ifndef XRT_MODULE_TLS_NEGOTIATE
#define XRT_MODULE_TLS_NEGOTIATE
#endif
#ifndef XRT_MODULE_TLS_KEY_EXCHANGE
#define XRT_MODULE_TLS_KEY_EXCHANGE
#endif
#endif

/* tls_key_exchange 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_KEY_EXCHANGE)
#ifndef XRT_FEATURE_TLS_KEY_EXCHANGE
#define XRT_FEATURE_TLS_KEY_EXCHANGE
#endif
#ifndef XRT_MODULE_TLS
#define XRT_MODULE_TLS
#endif
#endif

/* tls_negotiate 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_NEGOTIATE)
#ifndef XRT_FEATURE_TLS_NEGOTIATE
#define XRT_FEATURE_TLS_NEGOTIATE
#endif
#ifndef XRT_MODULE_TLS_HELLO
#define XRT_MODULE_TLS_HELLO
#endif
#endif

/* tls_hello 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_HELLO)
#ifndef XRT_FEATURE_TLS_HELLO
#define XRT_FEATURE_TLS_HELLO
#endif
#ifndef XRT_MODULE_TLS_HANDSHAKE
#define XRT_MODULE_TLS_HANDSHAKE
#endif
#endif

/* tls_handshake 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS_HANDSHAKE)
#ifndef XRT_FEATURE_TLS_HANDSHAKE
#define XRT_FEATURE_TLS_HANDSHAKE
#endif
#ifndef XRT_MODULE_TLS
#define XRT_MODULE_TLS
#endif
#endif

/* tls 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TLS)
#ifndef XRT_FEATURE_TLS
#define XRT_FEATURE_TLS
#endif
#endif

/* websocket_stream_ref 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_WEBSOCKET_STREAM_REF)
#ifndef XRT_FEATURE_WEBSOCKET_STREAM_REF
#define XRT_FEATURE_WEBSOCKET_STREAM_REF
#endif
#ifndef XRT_MODULE_WEBSOCKET_STREAM
#define XRT_MODULE_WEBSOCKET_STREAM
#endif
#endif

/* websocket_stream 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_WEBSOCKET_STREAM)
#ifndef XRT_FEATURE_WEBSOCKET_STREAM
#define XRT_FEATURE_WEBSOCKET_STREAM
#endif
#ifndef XRT_MODULE_WEBSOCKET_MESSAGE
#define XRT_MODULE_WEBSOCKET_MESSAGE
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#ifndef XRT_MODULE_NET_TCP
#define XRT_MODULE_NET_TCP
#endif
#endif

/* net_tcp 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_TCP)
#ifndef XRT_FEATURE_NET_TCP
#define XRT_FEATURE_NET_TCP
#endif
#ifndef XRT_MODULE_NET_ENGINE
#define XRT_MODULE_NET_ENGINE
#endif
#endif

/* net_engine 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_ENGINE)
#ifndef XRT_FEATURE_NET_ENGINE
#define XRT_FEATURE_NET_ENGINE
#endif
#ifndef XRT_MODULE_NET_PORT
#define XRT_MODULE_NET_PORT
#endif
#ifndef XRT_MODULE_NET_BUFFER
#define XRT_MODULE_NET_BUFFER
#endif
#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#ifndef XRT_MODULE_QUEUE_MPSC
#define XRT_MODULE_QUEUE_MPSC
#endif
#if defined(_WIN32)
#ifndef XRT_MODULE_NET_PORT_IOCP
#define XRT_MODULE_NET_PORT_IOCP
#endif
#ifndef XRT_MODULE_NET_PORT_SELECT
#define XRT_MODULE_NET_PORT_SELECT
#endif
#endif
#if defined(__linux__) && !defined(__ANDROID__)
#ifndef XRT_MODULE_NET_PORT_EPOLL
#define XRT_MODULE_NET_PORT_EPOLL
#endif
#ifndef XRT_MODULE_NET_PORT_SELECT
#define XRT_MODULE_NET_PORT_SELECT
#endif
#endif
#if defined(__ANDROID__)
#ifndef XRT_MODULE_NET_PORT_EPOLL
#define XRT_MODULE_NET_PORT_EPOLL
#endif
#ifndef XRT_MODULE_NET_PORT_SELECT
#define XRT_MODULE_NET_PORT_SELECT
#endif
#endif
#if defined(__APPLE__) && defined(__MACH__)
#ifndef XRT_MODULE_NET_PORT_KQUEUE
#define XRT_MODULE_NET_PORT_KQUEUE
#endif
#ifndef XRT_MODULE_NET_PORT_SELECT
#define XRT_MODULE_NET_PORT_SELECT
#endif
#endif
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
#ifndef XRT_MODULE_NET_PORT_KQUEUE
#define XRT_MODULE_NET_PORT_KQUEUE
#endif
#ifndef XRT_MODULE_NET_PORT_SELECT
#define XRT_MODULE_NET_PORT_SELECT
#endif
#endif
#if (!defined(_WIN32) && !defined(__linux__) && !defined(__ANDROID__) && !(defined(__APPLE__) && defined(__MACH__)) && !defined(__FreeBSD__) && !defined(__OpenBSD__) && !defined(__NetBSD__) && !defined(__DragonFly__))
#ifndef XRT_MODULE_NET_PORT_SELECT
#define XRT_MODULE_NET_PORT_SELECT
#endif
#endif
#endif

/* net_port_kqueue 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_PORT_KQUEUE)
#ifndef XRT_FEATURE_NET_PORT_KQUEUE
#define XRT_FEATURE_NET_PORT_KQUEUE
#endif
#ifndef XRT_MODULE_NET_PORT
#define XRT_MODULE_NET_PORT
#endif
#endif

/* net_port_epoll 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_PORT_EPOLL)
#ifndef XRT_FEATURE_NET_PORT_EPOLL
#define XRT_FEATURE_NET_PORT_EPOLL
#endif
#ifndef XRT_MODULE_NET_PORT
#define XRT_MODULE_NET_PORT
#endif
#endif

/* net_port_select 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_PORT_SELECT)
#ifndef XRT_FEATURE_NET_PORT_SELECT
#define XRT_FEATURE_NET_PORT_SELECT
#endif
#ifndef XRT_MODULE_NET_PORT
#define XRT_MODULE_NET_PORT
#endif
#endif

/* net_port_iocp 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_PORT_IOCP)
#ifndef XRT_FEATURE_NET_PORT_IOCP
#define XRT_FEATURE_NET_PORT_IOCP
#endif
#ifndef XRT_MODULE_NET_PORT
#define XRT_MODULE_NET_PORT
#endif
#endif

/* net_buffer 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_BUFFER)
#ifndef XRT_FEATURE_NET_BUFFER
#define XRT_FEATURE_NET_BUFFER
#endif
#ifndef XRT_MODULE_NET
#define XRT_MODULE_NET
#endif
#endif

/* net_port 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_PORT)
#ifndef XRT_FEATURE_NET_PORT
#define XRT_FEATURE_NET_PORT
#endif
#ifndef XRT_MODULE_NET_SOCKET
#define XRT_MODULE_NET_SOCKET
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifndef XRT_MODULE_WAIT
#define XRT_MODULE_WAIT
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#endif

/* net_socket 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_SOCKET)
#ifndef XRT_FEATURE_NET_SOCKET
#define XRT_FEATURE_NET_SOCKET
#endif
#ifndef XRT_MODULE_NET
#define XRT_MODULE_NET
#endif
#endif

/* net 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET)
#ifndef XRT_FEATURE_NET
#define XRT_FEATURE_NET
#endif
#endif

/* websocket_deflater 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_WEBSOCKET_DEFLATER)
#ifndef XRT_FEATURE_WEBSOCKET_DEFLATER
#define XRT_FEATURE_WEBSOCKET_DEFLATER
#endif
#ifndef XRT_MODULE_WEBSOCKET_DEFLATE
#define XRT_MODULE_WEBSOCKET_DEFLATE
#endif
#ifndef XRT_MODULE_DEFLATE
#define XRT_MODULE_DEFLATE
#endif
#endif

/* deflate 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_DEFLATE)
#ifndef XRT_FEATURE_DEFLATE
#define XRT_FEATURE_DEFLATE
#endif
#endif

/* websocket_inflater 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_WEBSOCKET_INFLATER)
#ifndef XRT_FEATURE_WEBSOCKET_INFLATER
#define XRT_FEATURE_WEBSOCKET_INFLATER
#endif
#ifndef XRT_MODULE_WEBSOCKET_DEFLATE
#define XRT_MODULE_WEBSOCKET_DEFLATE
#endif
#ifndef XRT_MODULE_INFLATE
#define XRT_MODULE_INFLATE
#endif
#endif

/* inflate 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_INFLATE)
#ifndef XRT_FEATURE_INFLATE
#define XRT_FEATURE_INFLATE
#endif
#endif

/* websocket_deflate 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_WEBSOCKET_DEFLATE)
#ifndef XRT_FEATURE_WEBSOCKET_DEFLATE
#define XRT_FEATURE_WEBSOCKET_DEFLATE
#endif
#ifndef XRT_MODULE_WEBSOCKET_EXTENSION
#define XRT_MODULE_WEBSOCKET_EXTENSION
#endif
#endif

/* websocket_extension 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_WEBSOCKET_EXTENSION)
#ifndef XRT_FEATURE_WEBSOCKET_EXTENSION
#define XRT_FEATURE_WEBSOCKET_EXTENSION
#endif
#ifndef XRT_MODULE_WEBSOCKET_HANDSHAKE
#define XRT_MODULE_WEBSOCKET_HANDSHAKE
#endif
#ifndef XRT_MODULE_HTTP_PARAM
#define XRT_MODULE_HTTP_PARAM
#endif
#endif

/* http_param 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_HTTP_PARAM)
#ifndef XRT_FEATURE_HTTP_PARAM
#define XRT_FEATURE_HTTP_PARAM
#endif
#ifndef XRT_MODULE_HTTP
#define XRT_MODULE_HTTP
#endif
#endif

/* websocket_keygen 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_WEBSOCKET_KEYGEN)
#ifndef XRT_FEATURE_WEBSOCKET_KEYGEN
#define XRT_FEATURE_WEBSOCKET_KEYGEN
#endif
#ifndef XRT_MODULE_WEBSOCKET_HANDSHAKE
#define XRT_MODULE_WEBSOCKET_HANDSHAKE
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#endif

/* random_secure 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_RANDOM_SECURE)
#ifndef XRT_FEATURE_RANDOM_SECURE
#define XRT_FEATURE_RANDOM_SECURE
#endif
#endif

/* websocket_handshake 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_WEBSOCKET_HANDSHAKE)
#ifndef XRT_FEATURE_WEBSOCKET_HANDSHAKE
#define XRT_FEATURE_WEBSOCKET_HANDSHAKE
#endif
#ifndef XRT_MODULE_HTTP
#define XRT_MODULE_HTTP
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#ifndef XRT_MODULE_CRYPTO_SHA1
#define XRT_MODULE_CRYPTO_SHA1
#endif
#endif

/* crypto_sha1 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_SHA1)
#ifndef XRT_FEATURE_CRYPTO_SHA1
#define XRT_FEATURE_CRYPTO_SHA1
#endif
#ifndef XRT_MODULE_CRYPTO_CORE
#define XRT_MODULE_CRYPTO_CORE
#endif
#endif

/* crypto_core 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CRYPTO_CORE)
#ifndef XRT_FEATURE_CRYPTO_CORE
#define XRT_FEATURE_CRYPTO_CORE
#endif
#endif

/* http 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_HTTP)
#ifndef XRT_FEATURE_HTTP
#define XRT_FEATURE_HTTP
#endif
#endif

/* websocket_message 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_WEBSOCKET_MESSAGE)
#ifndef XRT_FEATURE_WEBSOCKET_MESSAGE
#define XRT_FEATURE_WEBSOCKET_MESSAGE
#endif
#ifndef XRT_MODULE_WEBSOCKET_FRAME
#define XRT_MODULE_WEBSOCKET_FRAME
#endif
#ifndef XRT_MODULE_WEBSOCKET_CLOSE
#define XRT_MODULE_WEBSOCKET_CLOSE
#endif
#endif

/* websocket_close 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_WEBSOCKET_CLOSE)
#ifndef XRT_FEATURE_WEBSOCKET_CLOSE
#define XRT_FEATURE_WEBSOCKET_CLOSE
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#endif

/* websocket_frame 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_WEBSOCKET_FRAME)
#ifndef XRT_FEATURE_WEBSOCKET_FRAME
#define XRT_FEATURE_WEBSOCKET_FRAME
#endif
#endif

/* html_escape 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_HTML_ESCAPE)
#ifndef XRT_FEATURE_HTML_ESCAPE
#define XRT_FEATURE_HTML_ESCAPE
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#endif

/* codec_percent 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CODEC_PERCENT)
#ifndef XRT_FEATURE_CODEC_PERCENT
#define XRT_FEATURE_CODEC_PERCENT
#endif
#endif

/* codec_base64 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CODEC_BASE64)
#ifndef XRT_FEATURE_CODEC_BASE64
#define XRT_FEATURE_CODEC_BASE64
#endif
#endif

/* codec_hex 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CODEC_HEX)
#ifndef XRT_FEATURE_CODEC_HEX
#define XRT_FEATURE_CODEC_HEX
#endif
#endif

/* unicode_distance 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_UNICODE_DISTANCE)
#ifndef XRT_FEATURE_UNICODE_DISTANCE
#define XRT_FEATURE_UNICODE_DISTANCE
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#endif

/* string_glob 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_STRING_GLOB)
#ifndef XRT_FEATURE_STRING_GLOB
#define XRT_FEATURE_STRING_GLOB
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#endif

/* string_format 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_STRING_FORMAT)
#ifndef XRT_FEATURE_STRING_FORMAT
#define XRT_FEATURE_STRING_FORMAT
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#endif

/* string_split 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_STRING_SPLIT)
#ifndef XRT_FEATURE_STRING_SPLIT
#define XRT_FEATURE_STRING_SPLIT
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#endif

/* string 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_STRING)
#ifndef XRT_FEATURE_STRING
#define XRT_FEATURE_STRING
#endif
#endif

/* number_format 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NUMBER_FORMAT)
#ifndef XRT_FEATURE_NUMBER_FORMAT
#define XRT_FEATURE_NUMBER_FORMAT
#endif
#ifndef XRT_MODULE_NUMBER_INTEGER
#define XRT_MODULE_NUMBER_INTEGER
#endif
#ifndef XRT_MODULE_NUMBER_FLOAT
#define XRT_MODULE_NUMBER_FLOAT
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#endif

/* unicode 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_UNICODE)
#ifndef XRT_FEATURE_UNICODE
#define XRT_FEATURE_UNICODE
#endif
#endif

/* number_float 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NUMBER_FLOAT)
#ifndef XRT_FEATURE_NUMBER_FLOAT
#define XRT_FEATURE_NUMBER_FLOAT
#endif
#endif

/* number_integer 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NUMBER_INTEGER)
#ifndef XRT_FEATURE_NUMBER_INTEGER
#define XRT_FEATURE_NUMBER_INTEGER
#endif
#endif

/* memory_stats 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_MEMORY_STATS)
#ifndef XRT_FEATURE_MEMORY_STATS
#define XRT_FEATURE_MEMORY_STATS
#endif
#endif

/* memory_debug_report 及其直接依赖。 */
#if (defined(XRT_MODULE_ALL) && !defined(XRT_EXCLUDE_MEMORY_DEBUG)) || \
	defined(XRT_MODULE_MEMORY_DEBUG_REPORT)
#ifndef XRT_FEATURE_MEMORY_DEBUG_REPORT
#define XRT_FEATURE_MEMORY_DEBUG_REPORT
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#endif

/* memory_debug 及其直接依赖。 */
#if (defined(XRT_MODULE_ALL) && !defined(XRT_EXCLUDE_MEMORY_DEBUG)) || \
	defined(XRT_MODULE_MEMORY_DEBUG)
#ifndef XRT_FEATURE_MEMORY_DEBUG
#define XRT_FEATURE_MEMORY_DEBUG
#endif
#endif

/* channel_coroutine 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CHANNEL_COROUTINE)
#ifndef XRT_FEATURE_CHANNEL_COROUTINE
#define XRT_FEATURE_CHANNEL_COROUTINE
#endif
#ifndef XRT_MODULE_CHANNEL
#define XRT_MODULE_CHANNEL
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifndef XRT_MODULE_COROUTINE_SCHEDULER
#define XRT_MODULE_COROUTINE_SCHEDULER
#endif
#endif

/* coroutine_scheduler 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_COROUTINE_SCHEDULER)
#ifndef XRT_FEATURE_COROUTINE_SCHEDULER
#define XRT_FEATURE_COROUTINE_SCHEDULER
#endif
#ifndef XRT_MODULE_COROUTINE
#define XRT_MODULE_COROUTINE
#endif
#endif

/* coroutine 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_COROUTINE)
#ifndef XRT_FEATURE_COROUTINE
#define XRT_FEATURE_COROUTINE
#endif
#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#ifndef XRT_MODULE_CANCEL
#define XRT_MODULE_CANCEL
#endif
#ifndef XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_TEMP_MEMORY
#endif
#endif

/* temp_memory 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TEMP_MEMORY)
#ifndef XRT_FEATURE_TEMP_MEMORY
#define XRT_FEATURE_TEMP_MEMORY
#endif
#endif

/* thread 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_THREAD)
#ifndef XRT_FEATURE_THREAD
#define XRT_FEATURE_THREAD
#endif
#ifndef XRT_MODULE_WAIT
#define XRT_MODULE_WAIT
#endif
#endif

/* channel_select_cancel 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CHANNEL_SELECT_CANCEL)
#ifndef XRT_FEATURE_CHANNEL_SELECT_CANCEL
#define XRT_FEATURE_CHANNEL_SELECT_CANCEL
#endif
#ifndef XRT_MODULE_CHANNEL_SELECT
#define XRT_MODULE_CHANNEL_SELECT
#endif
#ifndef XRT_MODULE_CANCEL
#define XRT_MODULE_CANCEL
#endif
#endif

/* channel_select 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CHANNEL_SELECT)
#ifndef XRT_FEATURE_CHANNEL_SELECT
#define XRT_FEATURE_CHANNEL_SELECT
#endif
#ifndef XRT_MODULE_CHANNEL
#define XRT_MODULE_CHANNEL
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifndef XRT_MODULE_EVENT
#define XRT_MODULE_EVENT
#endif
#endif

/* event 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_EVENT)
#ifndef XRT_FEATURE_EVENT
#define XRT_FEATURE_EVENT
#endif
#ifndef XRT_MODULE_SYNC
#define XRT_MODULE_SYNC
#endif
#ifndef XRT_MODULE_WAIT
#define XRT_MODULE_WAIT
#endif
#endif

/* channel_cancel 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CHANNEL_CANCEL)
#ifndef XRT_FEATURE_CHANNEL_CANCEL
#define XRT_FEATURE_CHANNEL_CANCEL
#endif
#ifndef XRT_MODULE_CHANNEL
#define XRT_MODULE_CHANNEL
#endif
#ifndef XRT_MODULE_CANCEL
#define XRT_MODULE_CANCEL
#endif
#endif

/* cancel 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CANCEL)
#ifndef XRT_FEATURE_CANCEL
#define XRT_FEATURE_CANCEL
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#ifndef XRT_MODULE_COND
#define XRT_MODULE_COND
#endif
#endif

/* channel 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_CHANNEL)
#ifndef XRT_FEATURE_CHANNEL
#define XRT_FEATURE_CHANNEL
#endif
#ifndef XRT_MODULE_COND
#define XRT_MODULE_COND
#endif
#endif

/* cond 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_COND)
#ifndef XRT_FEATURE_COND
#define XRT_FEATURE_COND
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#ifndef XRT_MODULE_WAIT
#define XRT_MODULE_WAIT
#endif
#endif

/* wait 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_WAIT)
#ifndef XRT_FEATURE_WAIT
#define XRT_FEATURE_WAIT
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#endif

/* time 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_TIME)
#ifndef XRT_FEATURE_TIME
#define XRT_FEATURE_TIME
#endif
#endif

/* mutex 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_MUTEX)
#ifndef XRT_FEATURE_MUTEX
#define XRT_FEATURE_MUTEX
#endif
#ifndef XRT_MODULE_SYNC
#define XRT_MODULE_SYNC
#endif
#endif

/* sync 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_SYNC)
#ifndef XRT_FEATURE_SYNC
#define XRT_FEATURE_SYNC
#endif
#endif

/* queue_mpmc 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_QUEUE_MPMC)
#ifndef XRT_FEATURE_QUEUE_MPMC
#define XRT_FEATURE_QUEUE_MPMC
#endif
#ifndef XRT_MODULE_QUEUE
#define XRT_MODULE_QUEUE
#endif
#endif

/* queue_mpsc 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_QUEUE_MPSC)
#ifndef XRT_FEATURE_QUEUE_MPSC
#define XRT_FEATURE_QUEUE_MPSC
#endif
#ifndef XRT_MODULE_QUEUE
#define XRT_MODULE_QUEUE
#endif
#endif

/* queue_spsc 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_QUEUE_SPSC)
#ifndef XRT_FEATURE_QUEUE_SPSC
#define XRT_FEATURE_QUEUE_SPSC
#endif
#ifndef XRT_MODULE_QUEUE
#define XRT_MODULE_QUEUE
#endif
#endif

/* queue 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_QUEUE)
#ifndef XRT_FEATURE_QUEUE
#define XRT_FEATURE_QUEUE
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#endif

/* spin 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_SPIN)
#ifndef XRT_FEATURE_SPIN
#define XRT_FEATURE_SPIN
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#endif

/* atomic 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_ATOMIC)
#ifndef XRT_FEATURE_ATOMIC
#define XRT_FEATURE_ATOMIC
#endif
#endif

/* error_format 及其直接依赖。 */
#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_ERROR_FORMAT)
#ifndef XRT_FEATURE_ERROR_FORMAT
#define XRT_FEATURE_ERROR_FORMAT
#endif
#endif

#endif
