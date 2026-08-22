# SSH Hostbased Auth API

`ssh_auth_hostbased` 在公共 USERAUTH 消息层上实现 RFC 4252 hostbased 方法。它复用
`ssh_hostkey` 的算法无关公钥与 signature blob，不持有私钥、不执行签名，也不决定主机信任。

`xrtSshAuthHostBasedWrite/Read` 构建和严格解析完整请求；
`xrtSshAuthHostBasedSignDataWrite` 直接构建 RFC 4252 规定的签名原文。请求算法必须和
signature blob 的算法一致，公钥 blob 的内部算法允许不同，以保留 RSA SHA-2 和证书格式的
扩展空间。

`xrtSshAuthHostNameValid` 接受由 1 到 63 字节 DNS 标签组成的 US-ASCII 主机名，允许表示根标签
的末尾点，总长度最多 254 字节。客户端用户名使用 UTF-8。专用 API 的严格规则不会封死扩展：
需要非 DNS 身份形式时仍可使用公共认证请求层写入自定义方法字段。

服务端必须在协议解析之外验证主机密钥确实属于声明主机、网络端点与主机名的关系，以及客户端
用户到目标用户的授权关系。示例见 `examples/auth_hostbased/main.c`。
