# xjwt

`xjwt` 是构建在 xrt 核心密码原语之上的 JSON Web Token (RFC 7519) / JWS (RFC 7515)
签发与验证扩展库。

支持 HS256/384/512（HMAC）、RS256/384/512（RSA PKCS1）、ES256（ECDSA P-256）三族
算法，以及 JWKS (RFC 7517) 公钥集解析。

## 用法

实现是 `xjwt.c` 单一编译单元：把它加入构建（包含目录指向本目录），
或单 TU 场景直接 `#include "xjwt.c"`。依赖的 xrt 模块闭包见 `xjwt-xrt.h`。

```c
#include "xjwt.h"

/* 签发 */
xvalue* claims = xrtValueObject();
xrtValueObjectSetNew(claims, xrtStrView("sub"), xrtValueString("user123"));
char* token = xjwtHs256(claims, "secret", 3600);

/* 验证 */
xjwtcheck check;
xjwtCheckInit(&check);
check.Issuer = "myapp";
xvalue* out = xjwtVerify(token, "secret", &check);
if ( out != NULL ) {
    /* claims 已验签、已校验 exp/iss/aud */
    xrtValueRelease(out);
}
xrtValueRelease(claims);
xrtFree(token);

/* 高频验证：公钥解析一次，反复使用（线程安全，缓存只读） */
xjwtkey* key = xjwtKeyParse(rsaPublicPem);
out = xjwtVerifyKey(token, key, &check);
xjwtKeyFree(key);
```

## 构建

```sh
gcc -std=c11 -I<path-to-xjwt> -I<path-to-xrt>/single \
    your_app.c xjwt.c
```

`xjwt.c` 是 unity TU（内含 src/ 四个分片），无需单独编译 src/ 下的文件。

## 模块

- `xjwt_core` — Base64URL 编解码、token 组装/拆解
- `xjwt_sig` — HMAC/RSA PKCS1/ECDSA P-256 签名与验签、算法混淆防护
- `xjwt_main` — claims 校验、Sign/Verify/Decode 主入口
- `xjwt_ext` — RSA 私钥（PKCS#8/PKCS#1）与 EC 密钥（SEC1/PKCS#8）PEM 解析、
  RS/ES 签发、JWKS (RFC 7517) 解析与按 kid 选钥验签

## 状态

- HS256/384/512：完整实现（签发 + 验签 + claims 校验）
- RS256/384/512：完整实现（PKCS#8 与 PKCS#1 私钥签发 + 公钥验签）
- ES256：完整实现（SEC1 与 PKCS#8 私钥签发 + 公钥验签；签名按 RFC 7518 §3.4
  采用 DER 编码，与 OpenSSL/Jose 库互认）
- JWKS：完整实现（RSA n/e 与 EC x/y 解析、kid 严格匹配选钥验签；
  最多 16 把密钥，超出整体拒绝）
- 公钥缓存：`xjwtKeyParse` 解析一次，`xjwtVerifyKey` 高频验证免重复 PEM 解析
- 互操作：与 OpenSSL 3.x 双向验证通过；aud 支持字符串与数组（RFC 7519 §4.1.3）
- 错误报告：所有失败路径均设错误码；`xjwtLastError()` 一行读取本域错误
- 安全基线：exp/nbf 非整数拒绝、alg=none 与算法混淆拒绝（含 JWKS 路径）、
  kid JSON 转义、签名输出容量校验（不截断）

## 测试

```sh
cd tests
python gen_keys.py   # 重新生成 openssl 参考密钥夹具（test_keys.h，含 RSA-8192）
gcc -std=c11 -I.. -I../../single -o test_jwt test_jwt.c \
    -lws2_32 -lbcrypt -ladvapi32 -liphlpapi   # Windows；Linux 去掉 -l 参数
./test_jwt           # 117 项全绿
```

测试覆盖：HS/RS/ES 三族签发-验签往返、过期/iss/aud 校验、alg=none 与算法混淆
拒绝、篡改签名拒绝、错误公钥拒绝、PKCS#1/PKCS#8 密钥封装、openssl 互操作令牌
（RS256 与 ES256）、JWKS 解析/严格 kid 选钥/未知 kid 拒绝/超 16 密钥拒绝、
xjwtDecodeHeader、公钥缓存路径，以及两轮审计修复回归：字符串/浮点 exp·nbf 拒绝、
RSA-8192 签发、kid JSON 转义与超长往返、JWKS 非 P-256 曲线跳过、算法族与密钥
类型交叉校验、签名容量不足拒绝、畸形 JWKS 稳态零泄漏（计数分配器实测）、
超长 kid 三路拒绝、非对象 claims 双向拒绝、畸形输入 fuzz 2000 例。
