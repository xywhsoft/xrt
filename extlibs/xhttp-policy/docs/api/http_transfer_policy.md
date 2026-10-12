# HTTP 传输策略

`http_retry_policy` 只决定状态是否可重试并计算封顶指数退避；`Retry-After` 字段的解析与写出仍在 xhttp。
`http_redirect_policy` 使用已有请求、状态、Location 和安全标志构建下一跳请求，过程不访问网络。
计时器、实际重新拨号、响应排空和调用次数属于 xhttp-client；此库不保存 Call 或连接。

## xrtHttpRetryStatusDefault

输入状态码，返回是否属于 408、421、425、429、500、502、503、504；无分配、无错误副作用。

## xrtHttpRetryBackoff

输入基数、上限、零起始重试序号及输出地址。上限必须非零，基数不得大于上限。
输出封顶指数延迟，不回绕；零基数始终得到零。参数错误返回 false 且保持输出。

## xrtHttpRedirectConfigInit

输出十跳上限与默认 POST 改写策略。完整存储允许未对齐，地址回绕返回参数错误且不写入。

## xrtHttpRedirectStatus

输入状态，301、302、303、307、308 返回 true，其余返回 false；无分配。

## xrtHttpRedirectRequest

输入请求、标准重定向状态、Location、安全标志和必需的错误输出。输入请求保持不变。
成功返回独立请求，由 `xrtHttpRequestDestroy` 释放，Error 为 OK。方法/正文、fragment 和跨源凭据沿用既有规则。
失败返回 NULL，Error 区分非法重定向、不可重放和不允许的 HTTPS 降级；XRT 错误链保留分配失败。
Location 数据仅在调用中借用，结果自己持有 URL；不会拨号、等待或重试。
