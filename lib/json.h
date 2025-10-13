


/**************** json object structure / json对象结构 ****************/

/*
 * struct json_list - the value of json list
 * @next: the next list
 * @description: LJSON uses it to manage json objects and memory blocks.
 */
/*
 * struct json_list - 链表节点
 * @next: 下一个对象的链表节点
 * @description: LJSON使用它管理JSON对象和内存块的节点
 */
struct json_list {
    struct json_list *next;
};

/*
 * json_type_t - the json object type
 * @description: LJSON supports not only standard types, but also extended types (JSON HEX...).
 */
/*
 * json_type_t - json对象的类型
 * @description: LJSON支持的对象比cJSON多，例如长整数，十六机制的数
 */
typedef enum {
    JSON_NULL = 0,              /* It doesn't has value variable: null */
    JSON_BOOL,                  /* Its value variable is vbool: true, false */
    JSON_INT,                   /* Its value variable is vint */
    JSON_HEX,                   /* Its value variable is vhex */
    JSON_LINT,                  /* Its value variable is vlint */
    JSON_LHEX,                  /* Its value variable is vlhex */
    JSON_DOUBLE,                /* Its value variable is vdbl */
    JSON_STRING,                /* Its value variable is vstr */
    JSON_ARRAY,                 /* Its value variable is head */
    JSON_OBJECT                 /* Its value variable is head */
} json_type_t;

/*
 * json_strinfo_t - the information of json string object value or type-key value
 * @type: the json object type (json_type_t), it is only valid when used as a key
 * @escaped: whether the string contains characters that need to be escaped
 * @alloced: whether the string is alloced, it is only valid for SAX APIs
 * @len: the length of string
 * @description: LJSON uses string with information to accelerate printing.
 */
/*
 * json_strinfo_t - json对象的键或字符串类型的值的信息
 * @type: json对象的类型，只在作为json对象的键时才有效
 * @escaped: 是否含转义字符
 * @alloced: 是否是在堆中分配，只在SAX接口中有效
 * @len: 字符串长度
 * @description: LJSON使用此结构就知道了字符串长度，可以加快数据处理
 */
typedef struct {
    uint32_t type:4;
    uint32_t escaped:1;
    uint32_t alloced:1;
    uint32_t reserved:2;
    uint32_t len:24;
} json_strinfo_t;

/*
 * json_string_t - the json string with information
 * @str: the value of string
 * @info: the information of string
 * @description: LJSON uses string with information to accelerate printing.
 */
/*
 * json_string_t - 带信息的字符串
 * @str: 字符串数据
 * @info: 字符串信息
 * @description: LJSON使用此结构就知道了字符串长度，可以加快数据处理
 */
typedef struct {
    char *str;
    json_strinfo_t info;
} json_string_t;

/*
 * json_number_t - the json number object value
 * @description: LJSON supports decimal and hexadecimal, integer and long long integer.
 */
/*
 * json_number_t - json数字类型的值
 * @description: LJSON支持长整数和十六进制数
 */
typedef union {
    bool vbool;
    int32_t vint;
    uint32_t vhex;
    int64_t vlint;
    uint64_t vlhex;
    double vdbl;
} json_number_t;

/*
 * json_value_t - the json object value
 * @vnum: the numerical value
 * @vstr: the string value
 * @head: the DOM array or object value, LJSON manages child json objects through the list head
 * @description: LJSON uses union to manage the value of different objects to save memory.
 */
/*
 * json_value_t - json对象的值
 * @vnum: 数字类型的值
 * @vstr: 字符串类型的值
 * @head: 集合对象的子节点挂载的链表头，指向最后一个元素(非空时)或自己(空时)
 * @description: LJSON使用union管理对象的值从而节省内存空间
 */
typedef union {
    json_number_t vnum;
    char *vstr;
    struct json_list head;
} json_value_t;

/*
 * json_object - the json object
 * @list: the list value, LJSON associates `list` to the `head` of parent json object
 *   or the `list` of brother json objects
 * @key: the json object key, only the child objects of JSON_OBJECT have key
 * @ikey: the information of key (contains json object type)
 * @istr: the information of value.str
 * @value: the json object value
 * @description: LJSON uses union to manage the value of different objects to save memory.
 */
/*
 * json_object - json对象
 * @list: 链表节点，指向下一个对象或父对象的链表头
 * @key: json对象的键值，只有JSON_OBJECT的子对象才有键值
 * @ikey: key字符串信息(含json类型)
 * @istr: value.str字符串信息
 * @value: json对象的值
 * @description: LJSON使用更紧凑的内存结构以节省内存
 */
typedef struct {
    struct json_list list;
    char *key;
    json_strinfo_t ikey;
    json_strinfo_t istr;
    json_value_t value;
} json_object;

/*
 * json_item_t - the json item which contains json object and hash code
 * @hash: the hash code of key, only child json objects of JSON_OBJECT has the value
 * @json: the json object
 * @description: LJSON uses hash code to accelerate access to member of JSON_OBJECT.
 */
/*
 * json_item_t - 包含json对象和hash值的结构
 * @hash: 只有JSON_OBJECT才有key值，才有key的hash值
 * @json: json对象
 * @description: LJSON使用hash值来加快获取JSON_OBJECT下的子对象
 */
typedef struct {
    uint32_t hash;
    json_object *json;
} json_item_t;

/*
 * json_items_t - the json items array
 * @conflicted: whether the key hashes are conflicted in items, only for JSON_OBJECT parent
 * @total: the total size of items
 * @count: the total number of child json objects of JSON_ARRAY or JSON_OBJECT
 * @json: the array to store child json objects
 * @description: LJSON uses it to store all sub-objects of JSON_ARRAY or JSON_OBJECT.
 */
/*
 * json_items_t - json_item_t 的管理结构
 * @conflicted: JSON_OBJECT下的子对象的key的hash值是否有冲突
 * @total: 数组的容量
 * @count: 数组的当前数量
 * @json: 存储json对象的数组
 * @description: LJSON使用它存储JSON_ARRAY或JSON_OBJECT下的所有子对象，使用json_get_items接口获取
 */
typedef struct {
    uint32_t conflicted:1;
    uint32_t reserved:31;
    uint32_t total;
    uint32_t count;
    json_item_t *items;
} json_items_t;



/**************** json DOM printer / DOM打印器 ****************/

/*
 * json_print_choice_t - the choice to print
 * @size: INOUT, the length of data
 * @p: INOUT, the data, it can be NULL while size is not 0
 * @description: it is used for reusing cache to accelerate printing speed
 */
/*
 * json_print_ptr_t - 打印缓冲
 * @size: INOUT, 数据的长度
 * @p: INOUT, 数据指针，当size不为0时可以为 NULL
 * @description: 用于复用缓存以加速打印速度，此时LJSON内部可能都不会进行内存分配。
 *    打印到字符串时，开始时传入json_print_ptr_t，print结束时也传入json_print_ptr_t，此时不用释放
 *    打印返回的字符串，会一直复用json_print_ptr_t的p成员，最后不再使用时释放p成员一次即可。
 */
typedef struct {
    size_t size;
    char *p;
} json_print_ptr_t;

/*
 * json_print_choice_t - the choice to print
 * @str_len: OUT, the length of returned printed string when printing to string
 * @plus_size: IN, increased memory size during reallocation when printing to string,
 *   or the write buffer size when printing to file,
 *   its default value is `JSON_PRINT_SIZE_PLUS_DEF`(1024)
 * @item_size: IN, the json object size when transfering to a string,
 *   its default value is `JSON_FORMAT_ITEM_SIZE_DEF`(32) when format_flag is true,
 *   or `JSON_UNFORMAT_ITEM_SIZE_DEF`(24) when format_flag is false
 * @item_total: IN, the total json objects, it will be calculated automatically in DOM print,
 *   it is better to set the value by users in SAX print
 * @format_flag: IN, set formatted printing (true) or compressed printing(false)
 * @ptr: INOUT, pre-allocated memory for printing
 * @path: IN, when the path is set, it prints to file while printing,
 *   otherwise it directly print to string
 */
/*
 * json_print_choice_t - 打印参数设置
 * @str_len: OUT, 打印到字符串时返回的字符串长度
 * @plus_size: IN, 打印到字符串时重新分配内存时增加的内存大小，或打印到文件时的写入缓冲区大小，默认值为 `JSON_PRINT_SIZE_PLUS_DEF`(1024)
 * @item_size: IN, 将 JSON 对象转换为字符串时的大小，默认值为 `JSON_FORMAT_ITEM_SIZE_DEF`(32)（当 format_flag 为 true 时）或 `JSON_UNFORMAT_ITEM_SIZE_DEF`(24)（当 format_flag 为 false 时）
 * @item_total: IN, JSON 对象的总数，DOM 打印时会自动计算，SAX 打印时最好由用户设置
 * @format_flag: IN, 设置格式化打印（true）或压缩打印（false）
 * @ptr: INOUT, 预分配的内存用于打印
 * @path: IN, 当 path 设置时，打印到文件，否则直接打印到字符串
 * @description: 如果设置了ptr，外部缓冲进入打印内部函数后，将缓冲交给内部接口，自身置空；当打印完成后：
 *   如果打印到字符串，ptr被置为了返回的字符串的值，最后使用json_memory_free释放ptr->p或返回的字符串之一；
 *   如果打印到文件，ptr被置为了内部读写缓冲的值，最后使用json_memory_free释放ptr->p
 */
typedef struct {
    size_t str_len; /* return string length if it is printed to string. */
    size_t plus_size;
    size_t item_size;
    int item_total;
    bool format_flag;
    json_print_ptr_t *ptr;
    const char *path;
} json_print_choice_t;

/**************** json pool editor / 内存块json的编辑接口 ****************/

/*
 * json_mem_node_t - the block memory node
 * @list: the list value, LJSON associates `list` to the `head` of json_mem_mgr_t
 *   or the `list` of brother json_mem_node_t
 * @size: the memory size
 * @ptr: the memory pointer
 * @cur: the current memory pointer
 * @description: LJSON can use the block memory to accelerate memory allocation and save memory space.
 */
/*
 * json_mem_node_t - 块内存节点
 * @list: 链表节点，LJSON将 `list` 关联到 `json_mem_mgr_t` 的 `head` 或兄弟 `json_mem_node_t` 的 `list`
 * @size: 内存大小
 * @ptr: 内存指针
 * @cur: 当前内存指针
 * @description: LJSON使用块内存来加速内存分配并节省内存空间，内存块只能统一申请统一释放，无法单独释放某个对象
 */
typedef struct {
    struct json_list list;
    size_t size;
    char *ptr;
    char *cur;
} json_mem_node_t;

/*
 * json_mem_mgr_t - the node to manage block memory node
 * @head: the list head, LJSON manages block memory nodes through the list head
 * @mem_size: the default memory size to allocate, its default value is JSON_POOL_MEM_SIZE_DEF(8192)
 * @cur_node: the current block memory node
 * @description: the manage node manages a group of block memory nodes.
 */
/*
 * json_mem_mgr_t - 管理块内存节点的结构
 * @head: 链表头，LJSON通过链表头管理块内存节点
 * @mem_size: 默认分配的内存大小，默认值为 `JSON_POOL_MEM_SIZE_DEF`(8192)
 * @cur_node: 当前块内存节点
 * @description: 该管理节点管理一组块内存节点
 */
typedef struct {
    struct json_list head;
    size_t mem_size;
    json_mem_node_t *cur_node;
} json_mem_mgr_t;

/*
 * json_mem_t - the head to manage all types of block memory
 * @valid: IN, is there already memory allocation available to speed up
 *   frequent parsing of small JSON files
 * @obj_mgr: the node to manage json_object
 * @key_mgr: the node to manage the value of key
 * @str_mgr: the node to manage the value of JSON_STRING object
 * @description: The reason for dividing into multiple management nodes is that
 * there is a memory address alignment requirement for json_object.
 */
/*
 * json_mem_t - 管理所有类型块内存的结构
 * @valid: IN, 是否已经有内存分配可用于加速频繁解析小型JSON文件
 * @obj_mgr: 管理 `json_object` 的节点
 * @key_mgr: 管理键值的节点
 * @str_mgr: 管理 `JSON_STRING` 对象值的节点
 * @description: 划分为多个管理节点的原因是 `json_object` 有内存地址对齐要求
 *   valid设为false时，JSON解析函数内部会重新初始化mem，所以此时mem一定不要有挂载内存节点
 */
typedef struct {
    bool valid;
    json_mem_mgr_t obj_mgr;
    json_mem_mgr_t key_mgr;
    json_mem_mgr_t str_mgr;
} json_mem_t;





/**************** json SAX printer / SAX解析器 ****************/

/*
 * json_sax_cmd_t - the beginning and end of JSON_ARRAY or JSON_OBJECT object
 * @description: We know that parentheses have two sides, `JSON_SAX_START` indicates left side,
 *   and `JSON_SAX_FINISH` indicates right side.
 */
/*
 * json_sax_cmd_t - SAX APIs中指示JSON_ARRAY或JSON_OBJECT的开始和结束
 * @description: 集合类型是括号包起来的，JSON_SAX_START表示左边括号, JSON_SAX_FINISH指示右边括号
 */
typedef enum {
    JSON_SAX_START = 0,
    JSON_SAX_FINISH
} json_sax_cmd_t;

/*
 * json_sax_print_hd - the handle of SAX printer
 * @description: It is a pointer of `json_sax_print_t`
 */
/*
 * json_sax_parser_t - SAX打印句柄
 * @description: 实际就是 `json_sax_print_t` 的指针
 */
typedef void* json_sax_print_hd;

/*
 * json_sax_print_start - Start the SAX printer
 * @choice: INOUT, the print choice
 * @return: NULL on failure, a pointer (the handle of SAX print) on success
 */
/*
 * json_sax_print_start - 启动SAX打印器
 * @choice: INOUT, 打印选项
 * @return: 失败返回NULL；成功返回指针（SAX打印器的句柄）
 */
json_sax_print_hd json_sax_print_start(json_print_choice_t *choice);

/*
 * json_sax_print_format_start - Start the formatted SAX printer to string
 * @item_total: IN, the total json objects, it is better to set the value by users
 * @ptr: IN, pre-allocated memory for printing
 * @return: NULL on failure, a pointer (the handle of SAX print) on success
 */
/*
 * json_sax_print_format_start - 启动格式化的SAX打印器，输出到字符串
 * @item_total: IN, json对象的总数，最好由用户设置
 * @ptr: IN, 预分配的内存用于打印
 * @return: 失败返回NULL；成功返回指针（SAX打印器的句柄）
 */
static inline json_sax_print_hd json_sax_print_format_start(int item_total, json_print_ptr_t *ptr)
{
    json_print_choice_t choice;

    memset(&choice, 0, sizeof(choice));
    choice.format_flag = true;
    choice.item_total = item_total;
    choice.ptr = ptr;
    return json_sax_print_start(&choice);
}

/*
 * json_sax_print_unformat_start - Start the compressed SAX printer to string
 * @item_total: IN, the total json objects, it is better to set the value by users
 * @ptr: IN, pre-allocated memory for printing
 * @return: NULL on failure, a pointer (the handle of SAX print) on success
 */
/*
 * json_sax_print_unformat_start - 启动压缩的SAX打印器，输出到字符串
 * @item_total: IN, json对象的总数，最好由用户设置
 * @ptr: IN, 预分配的内存用于打印
 * @return: 失败返回NULL；成功返回指针（SAX打印器的句柄）
 */
static inline json_sax_print_hd json_sax_print_unformat_start(int item_total, json_print_ptr_t *ptr)
{
    json_print_choice_t choice;

    memset(&choice, 0, sizeof(choice));
    choice.format_flag = false;
    choice.item_total = item_total;
    choice.ptr = ptr;
    return json_sax_print_start(&choice);
}

/*
 * json_sax_fprint_format_start - Start the formatted SAX printer to file
 * @item_total: IN, the total json objects, it is better to set the value by users
 * @path: IN, the file to store the printed string
 * @ptr: IN, pre-allocated memory for printing
 * @return: NULL on failure, a pointer (the handle of SAX print) on success
 */
/*
 * json_sax_fprint_format_start - 启动格式化的SAX打印器，输出到文件
 * @item_total: IN, json对象的总数，最好由用户设置
 * @path: IN, 存储打印字符串的文件路径
 * @ptr: IN, 预分配的内存用于打印
 * @return: 失败返回NULL；成功返回指针（SAX打印器的句柄）
 */
static inline json_sax_print_hd json_sax_fprint_format_start(int item_total, const char *path, json_print_ptr_t *ptr)
{
    json_print_choice_t choice;

    memset(&choice, 0, sizeof(choice));
    choice.format_flag = true;
    choice.item_total = item_total;
    choice.path = path;
    choice.ptr = ptr;
    return json_sax_print_start(&choice);
}

/*
 * json_sax_fprint_unformat_start - Start the compressed SAX printer to file
 * @item_total: IN, the total json objects, it is better to set the value by users
 * @path: IN, the file to store the printed string
 * @ptr: IN, pre-allocated memory for printing
 * @return: NULL on failure, a pointer (the handle of SAX print) on success
 */
/*
 * json_sax_fprint_unformat_start - 启动压缩的SAX打印器，输出到文件
 * @item_total: IN, json对象的总数，最好由用户设置
 * @path: IN, 存储打印字符串的文件路径
 * @ptr: IN, 预分配的内存用于打印
 * @return: 失败返回NULL；成功返回指针（SAX打印器的句柄）
 */
static inline json_sax_print_hd json_sax_fprint_unformat_start(int item_total, const char *path, json_print_ptr_t *ptr)
{
    json_print_choice_t choice;

    memset(&choice, 0, sizeof(choice));
    choice.format_flag = false;
    choice.item_total = item_total;
    choice.path = path;
    choice.ptr = ptr;
    return json_sax_print_start(&choice);
}

/*
 * json_sax_print_value - SAX print the json object
 * @handle: IN, the handle of SAX printer
 * @type: the json object type
 * @jkey: IN, the LJSON key, allow length not to be set first by json_string_info_update
 * @value: IN, the json object value
 * @return: -1 on failure, 0 on success
 * @description: If the parent node of the node to be printed is an object, the key string must have a value;
 *   in other cases, the key string can be filled in or not.
 *   The JSON_ARRAY and JSON_OBJECT are printed twice, once the value is `JSON_SAX_START` to start,
 *   and once the value is `JSON_SAX_FINISH` to complete.
 */
/*
 * json_sax_print_value - SAX打印json对象
 * @handle: IN, SAX打印器的句柄
 * @type: IN, json对象的类型
 * @jkey: IN, json对象的键，允许长度未预先设置
 * @value: IN, json对象的值
 * @return: 失败返回-1；成功返回0
 * @description: 如果要打印的节点的父节点是对象，则键字符串必须有值；其他情况下，键字符串可以填写或不填写
 *  JSON_ARRAY和JSON_OBJECT需要打印两次，一次值为 `JSON_SAX_START` 表示开始，一次值为 `JSON_SAX_FINISH` 表示完成
 */
int json_sax_print_value(json_sax_print_hd handle, json_type_t type, json_string_t *jkey, const void *value);
static inline int json_sax_print_null(json_sax_print_hd handle, json_string_t *jkey) { return json_sax_print_value(handle, JSON_NULL, jkey, NULL); }
static inline int json_sax_print_bool(json_sax_print_hd handle, json_string_t *jkey, bool value) { return json_sax_print_value(handle, JSON_BOOL, jkey, &value); }
static inline int json_sax_print_int(json_sax_print_hd handle, json_string_t *jkey, int32_t value) { return json_sax_print_value(handle, JSON_INT, jkey, &value); }
static inline int json_sax_print_hex(json_sax_print_hd handle, json_string_t *jkey, uint32_t value) { return json_sax_print_value(handle, JSON_HEX, jkey, &value); }
static inline int json_sax_print_lint(json_sax_print_hd handle, json_string_t *jkey, int64_t value) { return json_sax_print_value(handle, JSON_LINT, jkey, &value); }
static inline int json_sax_print_lhex(json_sax_print_hd handle, json_string_t *jkey, uint64_t value) { return json_sax_print_value(handle, JSON_LHEX, jkey, &value); }
static inline int json_sax_print_double(json_sax_print_hd handle, json_string_t *jkey, double value) { return json_sax_print_value(handle, JSON_DOUBLE, jkey, &value); }
static inline int json_sax_print_string(json_sax_print_hd handle, json_string_t *jkey, json_string_t *value) { return json_sax_print_value(handle, JSON_STRING, jkey, value); }
static inline int json_sax_print_array(json_sax_print_hd handle, json_string_t *jkey, json_sax_cmd_t value) { return json_sax_print_value(handle, JSON_ARRAY, jkey, &value); }
static inline int json_sax_print_object(json_sax_print_hd handle, json_string_t *jkey, json_sax_cmd_t value) { return json_sax_print_value(handle, JSON_OBJECT, jkey, &value); }

/* C11泛型选择(Generic selection) */
#define json_sax_print_array_item(handle, value)  _Generic((value), \
bool            : json_sax_print_bool                             , \
int32_t         : json_sax_print_int                              , \
uint32_t        : json_sax_print_hex                              , \
int64_t         : json_sax_print_lint                             , \
uint64_t        : json_sax_print_lhex                             , \
double          : json_sax_print_double                           , \
json_string_t*  : json_sax_print_string)(handle, NULL, value)

#define json_sax_print_array_null(handle)         json_sax_print_null  (handle, NULL, NULL)
#define json_sax_print_array_start(handle, jkey)  json_sax_print_array (handle, jkey, JSON_SAX_START)
#define json_sax_print_array_finish(handle)       json_sax_print_array (handle, NULL, JSON_SAX_FINISH)

/* C11泛型选择(Generic selection) */
#define json_sax_print_object_item(handle, jkey, value)  _Generic((value), \
bool            : json_sax_print_bool                             , \
int32_t         : json_sax_print_int                              , \
uint32_t        : json_sax_print_hex                              , \
int64_t         : json_sax_print_lint                             , \
uint64_t        : json_sax_print_lhex                             , \
double          : json_sax_print_double                           , \
json_string_t*  : json_sax_print_string)(handle, jkey, value)

#define json_sax_print_object_null(handle, jkey)  json_sax_print_null  (handle, jkey, NULL)
#define json_sax_print_object_start(handle, jkey) json_sax_print_object(handle, jkey, JSON_SAX_START)
#define json_sax_print_object_finish(handle)      json_sax_print_object(handle, NULL, JSON_SAX_FINISH)

/*
 * json_sax_print_finish - Finish the SAX printer
 * @handle: IN, the handle of SAX printer
 * @length: OUT, the length of returned printed string
 * @ptr: OUT, export internal allocated memory for printing
 * @return: NULL on failure, a pointer on success
 * @description: When printing to file, the pointer is `"ok"` on success, don't free it,
 *   when printing to string, the pointer is the printed string, use `json_memory_free` to free it or ptr->p.
 */
/*
 * json_sax_print_finish - 结束SAX打印器
 * @handle: IN, SAX 打印器的句柄
 * @length: OUT, 返回的打印字符串的长度
 * @ptr: OUT, 导出内部分配的内存用于打印
 * @return: 失败返回NULL；成功时返回指针
 * @description: 当打印到文件时，成功时返回 "ok"，不要释放它；当打印到字符串时，返回打印的字符串，使用 `json_memory_free` 释放它或ptr->p
 */
char *json_sax_print_finish(json_sax_print_hd handle, size_t *length, json_print_ptr_t *ptr);



/**************** json SAX parser / SAX解析器 ****************/

/*
 * json_sax_value_t - the json object value
 * @vnum: the numerical value
 * @vstr: the string value
 * @vcmd: the SAX array or object value, only for SAX APIs
 * @description: LJSON uses union to manage the value of different objects to save memory.
 */
/*
 * json_value_t - json对象的值
 * @vnum: 数字类型的值
 * @vstr: 字符串类型的值
 * @vcmd: SAX APIs指示集合对象的开始和结束
 * @description: LJSON使用union管理对象的值从而节省内存空间
 */
typedef union {
    json_number_t vnum;
    json_string_t vstr;
    json_sax_cmd_t vcmd;
} json_sax_value_t;

/*
 * json_sax_parser_t - the description passed by SAX parser to the callback function
 * @total: the size of depth array
 * @index: the current index of JSON type and key
 * @array: the json depth information, which stores json object type and key
 * @value: the json object value
 * @description: LJSON SAX parsing will maintain a depth information used for state machine.
 */
/*
 * json_sax_parser_t - SAX解析选项
 * @read_size: IN, 从文件解析时的读取缓冲区大小，默认值为 `JSON_PARSE_READ_SIZE_DEF`(8192)
 * @str_len: IN, 从字符串解析时字符串的大小，最好在从字符串解析时设置
 * @str: IN, 要解析的字符串，`path` 和 `str` 只能有一个有值
 * @path: IN, 要解析的文件，当 path 设置时，边读取边解析数据，否则直接从字符串解析
 * @cb: IN, 处理 SAX 解析器传递结果的回调函数
 */
typedef struct {
    int total;
    int index;
    json_string_t *array;
    json_sax_value_t value;
} json_sax_parser_t;

/*
 * json_sax_cb_t - the callback function for SAX parsing
 * @description: Users need to fill the callback function, returning `JSON_SAX_PARSE_CONTINUE`
 * indicates continuing parsing, returning `JSON_SAX_PARSE_STOP` indicates stoping parsing
 */
/*
 * json_sax_parse_choice_t - SAX的回调函数
 * @description: 用户使用SAX接口时需要设置回调函数, 返回 `JSON_SAX_PARSE_CONTINUE` 表示继续解析；
 *   返回 `JSON_SAX_PARSE_STOP` 表示停止解析
 */
typedef enum {
    JSON_SAX_PARSE_CONTINUE = 0,
    JSON_SAX_PARSE_STOP
} json_sax_ret_t;
typedef json_sax_ret_t (*json_sax_cb_t)(json_sax_parser_t *parser);





/**************** configuration ****************/

#define json_dtoa                       jnum_dtoa

/* Whether to use manual loop unfolding */
#ifndef JSON_MANUAL_LOOP_UNFOLD
#define JSON_MANUAL_LOOP_UNFOLD         1
#endif

/* Whether to allow C-like single-line comments and multi-line comments */
#ifndef JSON_PARSE_SKIP_COMMENT
#define JSON_PARSE_SKIP_COMMENT         0
#endif

/* Whether to allow comma in last element of array or object */
#ifndef JSON_PARSE_LAST_COMMA
#define JSON_PARSE_LAST_COMMA           1
#endif

/* Whether to allow empty key */
#ifndef JSON_PARSE_EMPTY_KEY
#define JSON_PARSE_EMPTY_KEY            0
#endif

/* Whether to allow special characters such as newline in the string */
#ifndef JSON_PARSE_SPECIAL_CHAR
#define JSON_PARSE_SPECIAL_CHAR         1
#endif

/* Whether to allow single quoted string/key and unquoted key */
#ifndef JSON_PARSE_SPECIAL_QUOTES
#define JSON_PARSE_SPECIAL_QUOTES       0
#endif

/* Whether to allow HEX number */
#ifndef JSON_PARSE_HEX_NUM
#define JSON_PARSE_HEX_NUM              1
#endif

/* Whether to allow special number such as starting with '.', '+', '0' */
#ifndef JSON_PARSE_SPECIAL_NUM
#define JSON_PARSE_SPECIAL_NUM          1
#endif

/* Whether to allow special double such as nan, inf, -inf */
#ifndef JSON_PARSE_SPECIAL_DOUBLE
#define JSON_PARSE_SPECIAL_DOUBLE       1
#endif

/* Whether to allow json starting with non-array and non-object */
#ifndef JSON_PARSE_SINGLE_VALUE
#define JSON_PARSE_SINGLE_VALUE         1
#endif

/* Whether to allow characters other than spaces after finishing parsing */
#ifndef JSON_PARSE_FINISHED_CHAR
#define JSON_PARSE_FINISHED_CHAR        0
#endif

/**************** debug ****************/

/* error print */
#define JSON_ERROR_PRINT_ENABLE         1

#if JSON_ERROR_PRINT_ENABLE
#define JsonErr(fmt, ...) do {                                      \
    printf("[JsonErr][%s:%d] ", __func__, __LINE__);                \
    printf(fmt, ##__VA_ARGS__);                                     \
} while(0)

#define JsonPareseErr(s)      do {                                  \
    if (parse_ptr->str) {                                           \
        char ptmp[32] = {0};                                        \
        strncpy(ptmp, parse_ptr->str + parse_ptr->offset, 31);      \
        JsonErr("====%s====\n%s\n", s, ptmp);                       \
    } else {                                                        \
        JsonErr("%s\n", s);                                         \
    }                                                               \
} while(0)

#else
#define JsonErr(fmt, ...)     do {} while(0)
#define JsonPareseErr(s)      do {} while(0)
#endif

/**************** gcc builtin ****************/

#if defined(__GNUC__) || defined(__clang__)
#define UNUSED_ATTR                     __attribute__((unused))
#define FALLTHROUGH_ATTR                __attribute__((fallthrough))
#define likely(cond)                    __builtin_expect(!!(cond), 1)
#define unlikely(cond)                  __builtin_expect(!!(cond), 0)
#else
#define UNUSED_ATTR
#define FALLTHROUGH_ATTR
#define likely(cond)                    (cond)
#define unlikely(cond)                  (cond)
#endif

#if JSON_PARSE_SPECIAL_QUOTES
#define UNUSED_END_CH
#else
#define UNUSED_END_CH                   UNUSED_ATTR
#endif

/**************** definition ****************/

/* head apis */
#define json_malloc                     malloc
#define json_calloc                     calloc
#define json_realloc                    realloc
#define json_strdup                     strdup
#define json_free                       free

#define JSON_ITEM_NUM_PLUS_DEF          16
#define JSON_POOL_MEM_SIZE_DEF          8192

/* print choice size */
#define JSON_PRINT_UTF16_SUPPORT        0
#define JSON_PRINT_NUM_INIT_DEF         1024
#define JSON_PRINT_NUM_PLUS_DEF         64
#define JSON_PRINT_DEPTH_DEF            16
#define JSON_PRINT_SIZE_PLUS_DEF        8192
#define JSON_FORMAT_ITEM_SIZE_DEF       32
#define JSON_UNFORMAT_ITEM_SIZE_DEF     24

/* file parse choice size */
#define JSON_PARSE_ERROR_STR            "Z"
#define JSON_PARSE_READ_SIZE_DEF        8192
#define JSON_PARSE_NUM_DIV_DEF          8





typedef struct _json_parse_t {
    size_t size;
    size_t offset;
    bool reuse_flag;

    char *str;
    json_mem_t *mem;

    void (*skip_blank)(struct _json_parse_t *parse_ptr);
    int (*parse_string)(struct _json_parse_t *parse_ptr, char end_ch, char **ppstr,
        json_strinfo_t *pinfo, json_mem_mgr_t *mgr);
    int (*parse_value)(struct _json_parse_t *parse_ptr, json_object **root);

    json_sax_parser_t parser;
    json_sax_cb_t cb;
    json_sax_ret_t ret;
} json_parse_t;

#define IS_BLANK(c)      ((((c) + 0xdf) & 0xff) > 0xdf)
#define IS_DIGIT(c)      ((c) >= '0' && (c) <= '9')





static inline int _get_str_parse_ptr(json_parse_t *parse_ptr, int read_offset, size_t read_size UNUSED_ATTR, char **sstr)
{
    size_t offset = parse_ptr->offset + read_offset;
    *sstr = parse_ptr->str + offset;
    return (int)(parse_ptr->size - offset);
}

static inline int _get_parse_ptr(json_parse_t *parse_ptr, int read_offset, size_t read_size, char **sstr)
{
    return _get_str_parse_ptr(parse_ptr, read_offset, read_size, sstr);
}
#define _UPDATE_PARSE_OFFSET(add_offset)    parse_ptr->offset += add_offset

static inline json_type_t _json_parse_number(const char **sstr, json_number_t *vnum)
{
    const char *s = *sstr;

#if !JSON_PARSE_HEX_NUM
    if (unlikely(*s == '0' && (*(s+1) == 'x' || *(s+1) == 'X'))) {
        JsonErr("HEX can't be parsed in standard json!\n");
        return JSON_NULL;
    }
#endif

    json_type_t type;
    *sstr += xrtParseNum(s, (jnum_type_t *)&type, (jnum_value_t *)(vnum));
    return type;
}

static inline uint32_t _parse_hex4(const unsigned char *str)
{
    int i = 0;
    uint32_t val = 0;

    for (i = 0; i < 4; ++i) {
        switch (str[i]) {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            val = (val << 4) + (str[i] - '0');
            break;
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
            val = (val << 4) + 10 + (str[i] - 'a');
            break;
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
            val = (val << 4) + 10 + (str[i] - 'A');
            break;
        default:
            return 0;
        }
    }

    return val;
}

static int utf16_literal_to_utf8(const unsigned char *start_str, const unsigned char *finish_str, unsigned char **pptr)
{ /* copy from cJSON */
    const unsigned char *str = start_str;
    unsigned char *ptr = *pptr;

    int seq_len = 0;
    int i = 0;
    unsigned long uc = 0;
    unsigned int  uc1 = 0, uc2 = 0;
    unsigned char len = 0;
    unsigned char first_byte_mark = 0;

    /* converts a UTF-16 literal to UTF-8, A literal can be one or two sequences of the form \uXXXX */
    if ((finish_str - str) < 6)                             /* input ends unexpectedly */
        goto fail;
    str += 2;
    uc1 = _parse_hex4(str);                                 /* get the first utf16 sequence */
    if (((uc1 >= 0xDC00) && (uc1 <= 0xDFFF)))               /* check first_code is valid */
        goto fail;
    if ((uc1 >= 0xD800) && (uc1 <= 0xDBFF)) {               /* UTF16 surrogate pair */
        str += 4;
        seq_len = 12;                                       /* \uXXXX\uXXXX */
        if ((finish_str - str) < 6)                         /* input ends unexpectedly */
            goto fail;
        if ((str[0] != '\\') || (str[1] != 'u'))            /* missing second half of the surrogate pair */
            goto fail;
        str += 2;
        uc2 = _parse_hex4(str);                             /* get the second utf16 sequence */
        if ((uc2 < 0xDC00) || (uc2 > 0xDFFF))               /* check second_code is valid */
            goto fail;
        uc = 0x10000 + (((uc1 & 0x3FF) << 10) | (uc2 & 0x3FF)); /* calculate the unicode uc from the surrogate pair */
    } else {
        seq_len = 6;                                        /* \uXXXX */
        uc = uc1;
    }

    /* encode as UTF-8 takes at maximum 4 bytes to encode: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
    if (uc < 0x80) {                                        /* normal ascii, encoding 0xxxxxxx */
        len = 1;
    } else if (uc < 0x800) {                                /* two bytes, encoding 110xxxxx 10xxxxxx */
        len = 2;
        first_byte_mark = 0xC0;                             /* 11000000 */
    } else if (uc < 0x10000) {                              /* three bytes, encoding 1110xxxx 10xxxxxx 10xxxxxx */
        len = 3;
        first_byte_mark = 0xE0;                             /* 11100000 */
    } else if (uc <= 0x10FFFF) {                            /* four bytes, encoding 1110xxxx 10xxxxxx 10xxxxxx 10xxxxxx */
        len = 4;
        first_byte_mark = 0xF0;                             /* 11110000 */
    }  else {                                               /* invalid unicode */
        goto fail;
    }
    for (i = len - 1; i > 0; --i) {                         /* encode as utf8 */
        ptr[i] = (unsigned char)((uc | 0x80) & 0xBF);       /* 10xxxxxx */
        uc >>= 6;
    }
    if (len > 1) {                                          /* encode first byte */
        ptr[0] = (unsigned char)((uc | first_byte_mark) & 0xFF);
    } else {
        ptr[0] = (unsigned char)(uc & 0x7F);
    }

    *pptr += len;
    return seq_len;

fail:
    return 0;
}

static int _json_parse_key(json_parse_t *parse_ptr, json_string_t *jkey)
{
    char *str = NULL;
    char end_ch = '\0';

    parse_ptr->skip_blank(parse_ptr);
    _get_parse_ptr(parse_ptr, 0, 2, &str);

    switch (*str) {
    case '\"':
#if JSON_PARSE_SPECIAL_QUOTES
    case '\'':
#endif
        if (unlikely(str[1] == str[0])) {
#if !JSON_PARSE_EMPTY_KEY
            JsonPareseErr("key is empty!");
            goto err;
#else
            _UPDATE_PARSE_OFFSET(2);
#endif
        } else {
            end_ch = *str;
            _UPDATE_PARSE_OFFSET(1);
            if (unlikely(parse_ptr->parse_string(parse_ptr, end_ch, &jkey->str, &jkey->info,
                &parse_ptr->mem->key_mgr) < 0)) {
                goto err;
            }
            _UPDATE_PARSE_OFFSET(1);
        }
        parse_ptr->skip_blank(parse_ptr);
        _get_parse_ptr(parse_ptr, 0, 1, &str);
        if (unlikely(*str != ':')) {
            JsonPareseErr("key is not before ':'");
            goto err;
        }
        _UPDATE_PARSE_OFFSET(1);
        break;

    default:
#if JSON_PARSE_SPECIAL_QUOTES
        if (unlikely(*str == ':')) {
#if !JSON_PARSE_EMPTY_KEY
            JsonPareseErr("key is empty!");
            goto err;
#else
            _UPDATE_PARSE_OFFSET(1);
#endif
        } else {
            end_ch = ':';
            if (unlikely(parse_ptr->parse_string(parse_ptr, end_ch, &jkey->str, &jkey->info,
                &parse_ptr->mem->key_mgr) < 0)) {
                goto err;
            }
            _UPDATE_PARSE_OFFSET(1);

            while (IS_BLANK((unsigned char)jkey->str[jkey->info.len - 1]))
                --jkey->info.len;
            jkey->str[jkey->info.len] = '\0';
        }
        break;
#else
        JsonPareseErr("key is not started with quotes!");
        goto err;
#endif
    }
    return 0;
err:
    return -1;
}

static int _json_parse_single_value(json_parse_t *parse_ptr, char *str, json_strinfo_t *kinfo,
    json_number_t *pnum, char **ppstr, json_strinfo_t *pinfo)
{
    char end_ch = '\0';
    char *bak = NULL;

    switch (*str) {
    case '\"':
#if JSON_PARSE_SPECIAL_QUOTES
    case '\'':
#endif
        kinfo->type = JSON_STRING;
        if (unlikely(str[1] == str[0])) {
            *ppstr = NULL;
            memset(pinfo, 0, sizeof(*pinfo));
            _UPDATE_PARSE_OFFSET(2);
        } else {
            end_ch = *str;
            _UPDATE_PARSE_OFFSET(1);
            if (unlikely(parse_ptr->parse_string(parse_ptr, end_ch, ppstr, pinfo,
                &parse_ptr->mem->str_mgr) < 0)) {
                goto err;
            }
            _UPDATE_PARSE_OFFSET(1);
        }
        break;

#if JSON_PARSE_SPECIAL_NUM
    case '+':
#endif
    case '-':
#if JSON_PARSE_SPECIAL_DOUBLE
        if (strncmp("inf", str + 1, 3) == 0) {
            kinfo->type = JSON_DOUBLE;
            pnum->vlhex = (*str == '-' ? 0xFFF0000000000000 : 0x7FF0000000000000);
            _UPDATE_PARSE_OFFSET(4);
            break;
        }
        FALLTHROUGH_ATTR;
#endif
#if JSON_PARSE_SPECIAL_NUM
    case '.':
#endif
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        bak = str;
        if (unlikely((kinfo->type = _json_parse_number((const char **)&str, pnum)) == JSON_NULL)) {
            JsonPareseErr("Not number!");
            goto err;
        }
        _UPDATE_PARSE_OFFSET(str - bak);
        break;

    case 'f':
        if (likely(parse_ptr->size - parse_ptr->offset >= 5 && memcmp("false", str, 5) == 0)) {
            kinfo->type = JSON_BOOL;
            pnum->vbool = false;
            _UPDATE_PARSE_OFFSET(5);
        } else {
            JsonPareseErr("invalid next ptr!");
            goto err;
        }
        break;
    case 't':
        if (likely(parse_ptr->size - parse_ptr->offset >= 4 && memcmp("true", str, 4) == 0)) {
            kinfo->type = JSON_BOOL;
            pnum->vbool = true;
            _UPDATE_PARSE_OFFSET(4);
        } else {
            JsonPareseErr("invalid next ptr!");
            goto err;
        }
        break;

    case 'n':
        if (likely(parse_ptr->size - parse_ptr->offset >= 4 && memcmp("null", str, 4) == 0)) {
            kinfo->type = JSON_NULL;
            _UPDATE_PARSE_OFFSET(4);
            break;
        }
#if JSON_PARSE_SPECIAL_DOUBLE
        if (likely(parse_ptr->size - parse_ptr->offset >= 3 && memcmp("nan", str, 3) == 0)) {
            kinfo->type = JSON_DOUBLE;
            pnum->vlhex = 0x7FF0000000000001;
            _UPDATE_PARSE_OFFSET(3);
            break;
        }
#endif
        JsonPareseErr("invalid next ptr!");
        goto err;

#if JSON_PARSE_SPECIAL_DOUBLE
    case 'i':
        if (likely(parse_ptr->size - parse_ptr->offset >= 3 && memcmp("inf", str, 3) == 0)) {
            kinfo->type = JSON_DOUBLE;
            pnum->vlhex = 0x7FF0000000000000;
            _UPDATE_PARSE_OFFSET(3);
        } else {
            JsonPareseErr("invalid next ptr!");
            goto err;
        }
        break;
#endif
    default:
        JsonPareseErr("invalid next ptr!");
        goto err;
    }

    return 0;
err:
    return -1;
}

#if JSON_PARSE_SKIP_COMMENT
static bool _skip_comment_rapid(json_parse_t *parse_ptr, int *pcnt)
{
    char *str = parse_ptr->str + parse_ptr->offset + *pcnt;
    int cnt = 0;

    switch (*(str + 1)) {
    case '/':
        str += 2;
        *pcnt += 2;
        while (1) {
            switch (*str) {
            case '\n':
                ++str;
                ++*pcnt;
                goto next;
            case '\0':
                goto end;
            default:
                ++str;
                ++*pcnt;
                break;
            }
        }
        break;
    case '*':
        cnt = 2;
        str += 2;
        while (1) {
            switch (*str) {
            case '*':
                if (*(str + 1) == '/') {
                    str += 2;
                    cnt += 2;
                    *pcnt += cnt;
                    goto next;
                } else {
                    ++str;
                    ++cnt;
                }
                break;
            case '\0':
                goto end;
            default:
                ++str;
                ++cnt;
                break;
            }
        }
        break;
    default:
        goto end;
    }
next:
    return false;
end:
    return true;

}
#endif

static inline void _skip_blank_rapid(json_parse_t *parse_ptr)
{
    unsigned char *str, *bak;

#if JSON_PARSE_SKIP_COMMENT
next:
#endif
    str = (unsigned char *)(parse_ptr->str + parse_ptr->offset);
    bak = str;
    while (1) {
        if (IS_BLANK(*str)) ++str; else break;
#if JSON_MANUAL_LOOP_UNFOLD
        if (IS_BLANK(*str)) ++str; else break;
        if (IS_BLANK(*str)) ++str; else break;
        if (IS_BLANK(*str)) ++str; else break;
        if (IS_BLANK(*str)) ++str; else break;
        if (IS_BLANK(*str)) ++str; else break;
        if (IS_BLANK(*str)) ++str; else break;
        if (IS_BLANK(*str)) ++str; else break;
#endif
    }

    _UPDATE_PARSE_OFFSET(str - bak);

#if JSON_PARSE_SKIP_COMMENT
    if (unlikely(*str == '/')) {
        int cnt = 0;
        if (!_skip_comment_rapid(parse_ptr, &cnt)) {
            _UPDATE_PARSE_OFFSET(cnt);
            goto next;
        }
        _UPDATE_PARSE_OFFSET(cnt);
    }
#endif
}

static int _parse_strcpy(char *ptr, const char *str, int nsize)
{
    const char *bak = ptr, *last = str, *end = str + nsize;
    int size = 0, seq_len = 0;

    while (str < end) {
        if (unlikely(*str++ == '\\')) {
            size = (int)(str - last - 1);
            memcpy(ptr, last, size);
            ptr += size;

            switch ((*str++)) {
            case 'b' : *ptr++ = '\b'; break;
            case 'f' : *ptr++ = '\f'; break;
            case 'n' : *ptr++ = '\n'; break;
            case 'r' : *ptr++ = '\r'; break;
            case 't' : *ptr++ = '\t'; break;
            case 'v' : *ptr++ = '\v'; break;
            case '\"': *ptr++ = '\"'; break;
            case '\'': *ptr++ = '\''; break;
            case '\\': *ptr++ = '\\'; break;
            case '/' : *ptr++ = '/' ; break;
#if JSON_PARSE_SPECIAL_CHAR
            case '\r': if (*str == '\n') ++str; break;
            case '\n': break;
#endif
            case 'u' :
                str -= 2;
                if (unlikely((seq_len = utf16_literal_to_utf8((unsigned char*)str,
                    (unsigned char*)end, (unsigned char**)&ptr)) == 0)) {
                    JsonErr("invalid utf16 code(\\u%c)!\n", str[2]);
                    return -1;
                }
                str += seq_len;
                break;
            default :
                JsonErr("invalid escape character(\\%c)!\n", str[1]);
                return -1;
            }

            last = str;
        }
    }

    size = (int)(str - last);
    memcpy(ptr, last, size);
    ptr += size;
    *ptr = '\0';
    return (int)(ptr - bak);
}

static int _parse_strlen(json_parse_t *parse_ptr, char end_ch UNUSED_END_CH, int *escaped)
{
#define PARSE_READ_SIZE    128
    char *str = NULL, *bak = NULL;
    int total = 0, rsize = 0;
    char c = '\0';

    _get_parse_ptr(parse_ptr, 0, PARSE_READ_SIZE, &str);
    bak = str;

    while (1) {
        switch ((c = *str++)) {
        case '\"':
#if JSON_PARSE_SPECIAL_QUOTES
        case '\'':
        case ':':
            if (c == end_ch)
#endif
            {
                total += (int)(str - bak - 1);
                return total;
            }
#if JSON_PARSE_SPECIAL_QUOTES
            if (c == '\"' && !*escaped)
                *escaped = -1;
            break;
#endif
        case '\\':
            if (likely(rsize != (str - bak))) {
                ++str;
                *escaped = 1;
            } else {
                --str;
                total += (int)(str - bak);
                if (unlikely((rsize = _get_parse_ptr(parse_ptr, total, PARSE_READ_SIZE, &str)) < 2)) {
                    JsonErr("last char is slash!\n");
                    goto err;
                }
                bak = str;
            }
            break;
        case '\0':
            --str;
            total += (int)(str - bak);
            if (unlikely((rsize = _get_parse_ptr(parse_ptr, total, PARSE_READ_SIZE, &str)) < 1)) {
                JsonErr("No more string!\n");
                goto err;
            }
            bak = str;
            break;
#if !JSON_PARSE_SPECIAL_CHAR
        case '\b': case '\f': case '\n': case '\r': case '\t': case '\v':
            JsonErr("tab and linebreak can't be existed in string in standard json!\n");
            goto err;
#endif
        default:
            break;
        }
    }

err:
    JsonPareseErr("str format err!");
    return -1;
}





static json_mem_t s_invalid_json_mem;





static inline int _json_sax_parse_string(json_parse_t *parse_ptr, char end_ch, char **ppstr,
    json_strinfo_t *pinfo, json_mem_mgr_t *mgr)
{
    char *ptr = NULL, *str = NULL;
    int len = 0, total = 0;
    int escaped = 0;

    *ppstr = NULL;
    memset(pinfo, 0, sizeof(*pinfo));

    if (unlikely((total = _parse_strlen(parse_ptr, end_ch, &escaped)) < 0)) {
        return -1;
    }
    len = total;
    _get_parse_ptr(parse_ptr, 0, total, &str);

    if (likely(escaped != 1)) {
        if (!(mgr == &parse_ptr->mem->key_mgr)) {
            pinfo->escaped = escaped != 0;
            pinfo->alloced = 0;
            pinfo->len = len;
            *ppstr = str;
        } else {
            if (unlikely((ptr = (char *)json_malloc(len+1)) == NULL)) {
                JsonErr("malloc failed!\n");
                return -1;
            }
            memcpy(ptr, str, len);
            ptr[len] = '\0';

            pinfo->escaped = escaped != 0;
            pinfo->alloced = 1;
            pinfo->len = len;
            *ppstr = ptr;
        }
    } else {
        if (unlikely((ptr = (char *)json_malloc(len+1)) == NULL)) {
            JsonErr("malloc failed!\n");
            return -1;
        }
        if (unlikely((len = _parse_strcpy(ptr, str, len)) < 0)) {
            JsonErr("_parse_strcpy failed!\n");
            json_free(ptr);
            goto err;
        }

        pinfo->escaped = 1;
        pinfo->alloced = 1;
        pinfo->len = len;
        *ppstr = ptr;
    }
    _UPDATE_PARSE_OFFSET(total);

    return len;
err:
    JsonPareseErr("parse string failed!");
    return -1;
}

static int _json_sax_parse_value(json_parse_t *parse_ptr)
{
    char *str = NULL;
    json_string_t *jkey = NULL, *parent = NULL;
    json_sax_value_t *value = &parse_ptr->parser.value;
    json_string_t *tarray = NULL;
    char end_ch = '\0';
    int i = 0;

    memset(value, 0, sizeof(*value));
    parse_ptr->parser.total += JSON_ITEM_NUM_PLUS_DEF;
    if (unlikely((parse_ptr->parser.array = (json_string_t *)json_malloc(sizeof(json_string_t) * parse_ptr->parser.total)) == NULL)) {
        JsonErr("malloc failed!\n");
        return -1;
    }
    memset(parse_ptr->parser.array, 0, sizeof(json_string_t));
    goto next3;

next1:
    if (unlikely(parse_ptr->parser.index >= parse_ptr->parser.total - 1)) {
        parse_ptr->parser.total += JSON_ITEM_NUM_PLUS_DEF;
        if (unlikely((tarray = (json_string_t *)json_malloc(sizeof(json_string_t) * parse_ptr->parser.total)) == NULL)) {
            JsonErr("malloc failed!\n");
            goto err;
        }

        memcpy(tarray, parse_ptr->parser.array, sizeof(json_string_t) * (parse_ptr->parser.index + 1));
        json_free(parse_ptr->parser.array);
        parse_ptr->parser.array = tarray;
    }

    parent = parse_ptr->parser.array + parse_ptr->parser.index;
    ++parse_ptr->parser.index;
    memset(parse_ptr->parser.array + parse_ptr->parser.index, 0, sizeof(json_string_t));

next2:
    if (parent->info.type == JSON_ARRAY)
        goto next3;
    jkey = parse_ptr->parser.array + parse_ptr->parser.index;
    if (unlikely(_json_parse_key(parse_ptr, jkey) < 0)) {
        goto err;
    }

next3:
    jkey = parse_ptr->parser.array + parse_ptr->parser.index;
    parse_ptr->skip_blank(parse_ptr);
    _get_parse_ptr(parse_ptr, 0, 128, &str);

    if ((*str == '{') || (*str == '[')) {
        jkey->info.type = (*str == '[') ? JSON_ARRAY : JSON_OBJECT;
        end_ch = (*str == '[') ? ']' : '}';
        value->vcmd = JSON_SAX_START;
        _UPDATE_PARSE_OFFSET(1);
        parse_ptr->ret = parse_ptr->cb(&parse_ptr->parser);
        if (unlikely(parse_ptr->ret == JSON_SAX_PARSE_STOP)) {
            goto end;
        }

        parse_ptr->skip_blank(parse_ptr);
        _get_parse_ptr(parse_ptr, 0, 1, &str);
        if (likely(*str != end_ch)) {
            goto next1;
        } else {
            value->vcmd = JSON_SAX_FINISH;
            _UPDATE_PARSE_OFFSET(1);
        }
    } else {
        if (_json_parse_single_value(parse_ptr, str, &jkey->info, &value->vnum,
            &value->vstr.str, &value->vstr.info) < 0)
            goto err;
    }

    parse_ptr->ret = parse_ptr->cb(&parse_ptr->parser);
    if (jkey->info.type == JSON_STRING && value->vstr.info.alloced) {
        json_free(value->vstr.str);
    }
    memset(value, 0, sizeof(*value));
    if (jkey->info.alloced) {
        json_free(jkey->str);
    }
    memset(jkey, 0, sizeof(*jkey));
    if (unlikely(parse_ptr->ret == JSON_SAX_PARSE_STOP)) {
        --parse_ptr->parser.index;
        goto end;
    }

next4:
    if (likely(parse_ptr->parser.index > 0)) {
        /* parse_ptr->parser.index > 0, parent is definitely not NULL. */
        end_ch = (parent->info.type == JSON_ARRAY) ? ']' : '}';
        parse_ptr->skip_blank(parse_ptr);
        _get_parse_ptr(parse_ptr, 0, 1, &str);

        if (likely(*str == ',')) {
            _UPDATE_PARSE_OFFSET(1);
#if !JSON_PARSE_LAST_COMMA
            goto next2;
#else
            parse_ptr->skip_blank(parse_ptr);
            _get_parse_ptr(parse_ptr, 0, 1, &str);
            if (*str != end_ch)
                goto next2;
#endif
        }
        if (likely(*str == end_ch)) {
            _UPDATE_PARSE_OFFSET(1);
            --parse_ptr->parser.index;
            jkey = parse_ptr->parser.array + parse_ptr->parser.index;
            value->vcmd = JSON_SAX_FINISH;
            parse_ptr->ret = parse_ptr->cb(&parse_ptr->parser);
            memset(value, 0, sizeof(*value));
            if (jkey->info.alloced) {
                json_free(jkey->str);
            }
            memset(jkey, 0, sizeof(*jkey));
            if (unlikely(parse_ptr->ret == JSON_SAX_PARSE_STOP)) {
                --parse_ptr->parser.index;
                goto end;
            }

            if (likely(parse_ptr->parser.index > 0)) {
                parent = parse_ptr->parser.array + parse_ptr->parser.index - 1;
                goto next4;
            }
        } else {
            JsonPareseErr("invalid object or array!");
            goto err;
        }
    }

    parse_ptr->parser.index = -1;
end:
    value->vcmd = JSON_SAX_FINISH;
    for (i =parse_ptr->parser.index; i >= 0; --i) {
        parse_ptr->ret = parse_ptr->cb(&parse_ptr->parser);
        if (parse_ptr->parser.array[i].info.alloced) {
            json_free(parse_ptr->parser.array[i].str);
        }
    }
    json_free(parse_ptr->parser.array);
    memset(&parse_ptr->parser, 0, sizeof(parse_ptr->parser));

    return 0;
err:
    if (parse_ptr->parser.array) {
        for (i = 0; i < parse_ptr->parser.index; ++i) {
            if (parse_ptr->parser.array[i].info.alloced) {
                json_free(parse_ptr->parser.array[i].str);
            }
        }
        json_free(parse_ptr->parser.array);
    }
    memset(&parse_ptr->parser, 0, sizeof(parse_ptr->parser));
    return -1;
}



/*
 * json_sax_parse_str - The SAX parser from string
 * @str: IN, the string to be parsed
 * @str_len: IN, the length of str
 * @cb: IN, the callback to process result passed by the SAX parser
 * @return: -1 on failure, 0 on success
 * description: LJSON directly parses the data from the string
 */
/*
 * json_sax_parse_str - 从字符串进行SAX解析
 * @str: IN, 要解析的字符串
 * @str_len: IN, 字符串的长度
 * @cb: IN, 处理SAX解析器传递结果的回调函数
 * @return: 失败返回-1；成功返回0
 * @description: LJSON直接从字符串解析数据
 */
XXAPI int json_sax_parse_str(char *str, size_t str_len, json_sax_cb_t cb)
{
    int ret = -1;
    json_parse_t parse_val = {0};
	
    parse_val.mem = &s_invalid_json_mem;
    
	parse_val.str = str;
	parse_val.size = str_len ? str_len : strlen(str);
	parse_val.skip_blank = _skip_blank_rapid;
    
    parse_val.parse_string = _json_sax_parse_string;
    parse_val.cb = cb;

#if !JSON_PARSE_SINGLE_VALUE
    parse_val.skip_blank(&parse_val);
    if (parse_val.str[parse_val.offset] != '{' && parse_val.str[parse_val.offset] != '[') {
        JsonErr("The first object isn't object or array!\n");
        goto end;
    }
#endif

    ret = _json_sax_parse_value(&parse_val);
#if !JSON_PARSE_FINISHED_CHAR
    if (ret == 0) {
        parse_val.skip_blank(&parse_val);
        if (parse_val.str[parse_val.offset]) {
            JsonErr("Extra trailing characters!\n%s\n", parse_val.str + parse_val.offset);
            ret = -1;
        }
    }
#endif

#if !JSON_PARSE_SINGLE_VALUE
end:
#endif

    return ret;
}