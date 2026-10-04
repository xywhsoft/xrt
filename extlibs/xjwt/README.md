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
xrtValueObjectSetNew(claims, xrtStrView("sub"),
    xrtValueString(xrtStrView("user123")));
char* token = xjwtHs256(claims, "secret", 3600);

if ( token != NULL ) {
    /* 验证 */
    xjwtcheck check;
    xjwtCheckInit(&check);
    xvalue* out = xjwtVerify(token, "secret", &check);
    if ( out != NULL ) {
        /* claims 已验签，且 exp/nbf 等标准时效字段已校验 */
        xrtValueRelease(out);
    }
    xrtFree(token);
}
xrtValueRelease(claims);
```

签发时会在内部的顶层副本中注入 `iat`、`exp` 等配置字段；调用方保留
`claims` 的所有权，签发成功或失败都不会修改它。重复使用同一个对象签发时，
每次都从原始字段开始。配置中的同名字段覆盖令牌内的值，不覆盖输入对象。

RSA/EC 高频验证可先用 `xjwtKeyParse` 解析对应公钥，再用
`xjwtVerifyKey` 验证同算法令牌；缓存密钥在最后一次验证后由 `xjwtKeyFree`
释放。HS 系列使用 `xjwtVerify` 和共享密钥字符串。

RS256/384/512 接受 RSA 模数 2048–8192 位。公钥 PEM 支持 `PUBLIC KEY`
（SPKI，`rsaEncryption` OID 与 NULL 参数）和 `RSA PUBLIC KEY`（PKCS#1）；
私钥 PEM 支持 `PRIVATE KEY`（PKCS#8）和 `RSA PRIVATE KEY`（PKCS#1）。
解析要求规范 DER、结构完整且无尾随数据；不接受 RSA-PSS 算法标识。
JWKS 中低于 2048 位或结构无效的 RSA 密钥会被跳过。

ES256 公钥使用 `PUBLIC KEY`（SPKI，`id-ecPublicKey` 与 `prime256v1`）；
私钥使用 `EC PRIVATE KEY`（SEC1）或 `PRIVATE KEY`（PKCS#8）。解析要求完整
规范 DER、有效 P-256 曲线点及有效私钥标量。SEC1 中的可选曲线必须是 P-256；
若含可选公钥，它必须与私钥标量对应。EC PEM 正文上限为 4096 字节。
JWKS 的 P-256 坐标也在解析时验证为有效曲线点；无效条目不会给后续密钥
留下 `kid` 或坐标。
PKCS#8 接受 version 0 的 `PrivateKeyInfo` 和 [RFC 5958](https://www.rfc-editor.org/rfc/rfc5958#section-2)
version 1 的 `OneAsymmetricKey`。可带 `[0] IMPLICIT Attributes`；解析器校验属性
结构和 DER 排序，但不解释属性语义。version 1 必须带 `[1]` 外层公钥，version 0
不得带该字段；RSA/EC 外层公钥还必须与私钥对应。加密私钥不在当前支持范围。

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
- ES256：完整实现（SEC1 与 PKCS#8 私钥签发 + 公钥验签；JWS 签名按 RFC 7518 §3.4
  采用 64 字节定宽 R||S，OpenSSL 的 DER 签名在生成 JWT 时需先转换）
- JWKS：完整实现（RSA n/e 与 EC x/y 解析、字符串 kid 严格匹配选钥验签；
  最多 16 把密钥，超出整体拒绝）
- 公钥缓存：`xjwtKeyParse` 解析一次，`xjwtVerifyKey` 高频验证免重复 PEM 解析
- 互操作：与 OpenSSL 3.x 双向验证通过；aud 支持字符串与数组（RFC 7519 §4.1.3）
- 错误报告：所有失败路径均设错误码；`xjwtLastError()` 一行读取本域错误
- 接口限制：exp/nbf 仅支持整数 NumericDate，拒绝其他类型；RFC 7519 允许
  非整数 NumericDate，这不是规范强制的整数限制。
- 安全基线：alg=none 与算法混淆拒绝（含 JWKS 路径）、
  kid JSON 转义、签名输出容量校验（不截断）、Base64url 非零末组补齐位拒绝

不使用 `kid` 时将 `xjwtconfig.KeyId` 留为 `NULL`；`""` 会签发显式空标识。
[RFC 7515 §4.1.4](https://www.rfc-editor.org/rfc/rfc7515.html#section-4.1.4)
要求 `kid` 为区分大小写的字符串，没有非空要求。验证时空标识只匹配显式空
JWK `kid`，不会匹配缺少 `kid` 的密钥；只有 token 未提供 `kid` 才选择首钥。
当前 C 字符串接口不支持内含 NUL 的标识，会拒绝该 token 或跳过该 JWK；
同样拒绝非字符串标识。token `kid` 须短于 256 字节，JWK `kid` 须短于 128 字节。

## 测试

```sh
python tools/test_auth_extensions.py --case jwt
python tools/test_auth_extensions.py --case jwt_time
python tools/test_auth_extensions.py --case jwt_example --case jwt_example_fault
python extlibs/xjwt/tests/test_rfc5958_openssl.py  # 独立 OpenSSL 解析、签名与验签
python tools/test_auth_coverage_inputs.py        # 测试向量变动时拒绝覆盖率报告
python tools/measure_auth_coverage.py --library jwt
```

从仓库根目录运行上述命令；仅在更新密钥夹具时执行
`python extlibs/xjwt/tests/gen_keys.py`。主测试脚本会按平台选择编译参数，
并执行完整 JWT 用例；独立互操作脚本另需 `openssl` 命令可用。HS/RS/ES 验签
（含 PEM、缓存及 JWKS 入口）、公钥缓存、
解码和 JWKS 解析逐分配点验证内存不足时返回失败并保留内存错误；缓存及解码
还核对所有分配的释放。解码失败时算法输出为 `XJWT_ALG_INVALID`，`kid` 为 `NULL`。
`xjwtDecode(token, NULL)` 只解码 claims；请求算法输出时 header 也必须成功解码。

时间校验在 `exp` 到期瞬间拒绝令牌，在 `nbf` 生效瞬间接受令牌。
`ClockLeeway` 必须非负；加入容差后，到期边界仍为开区间，生效边界为闭区间。
非零 `NowOverride` 均用于注入时间，包含 Unix 纪元之前的负值。比较通过有序
时间点的无符号距离实现，避免极端时间值与容差相加减发生有符号溢出。
`jwt_time` 对直接 claims 校验和已签名令牌验签分别检查这些边界。

`auth_middleware.c` 使用离线夹具。范例强制检查完整且非空的用户/角色字段，
拒绝截断和内嵌 NUL，失败时清空身份输出；签发后释放借用的 claims，
JWKS 刷新失败保留原缓存，第三方 IdP 令牌同时校验 issuer 和 audience。
独立范例故障测试检查分配失败、资源释放、错误受众拒绝和非零退出码。

覆盖率按自有 C 文件的 gcov JSON 精确计数，默认门槛为行 85%、分支结果
67%。报告绑定实现、Core 单头、主测试、密钥夹具和统计工具的 SHA256；
任何输入在测量期间变动都会失败，并移除旧报告，不能用新哈希包装旧计数。
Windows/Linux 分别保存于 `out/auth_extensions/coverage/<平台>/jwt/coverage.json`。

测试覆盖：HS/RS/ES 三族签发-验签往返、过期/iss/aud 校验、alg=none 与算法混淆
拒绝、篡改签名拒绝、错误公钥拒绝、PKCS#1/PKCS#8 密钥封装、openssl 互操作令牌
（RS256 与 ES256）、JWKS 解析/严格 kid 选钥/未知 kid 拒绝/超 16 密钥拒绝、
xjwtDecodeHeader、公钥缓存路径，以及两轮审计修复回归：字符串/浮点 exp·nbf 拒绝、
RSA-8192 签发、kid JSON 转义与超长往返、JWKS 非 P-256 曲线跳过、算法族与密钥
类型交叉校验、签名容量不足拒绝、畸形 JWKS 稳态零泄漏（计数分配器实测）、
超长 kid 三路拒绝、非对象 claims 双向拒绝、签发成功/失败及重复签发的输入对象
不变性、畸形输入 fuzz 2000 例；RSA 公私钥 DER 的算法标识、BIT STRING
未使用位、尾随内容与 1024 位弱密钥拒绝；EC 密钥的算法/曲线 OID、私钥版本、
曲线点、标量、公私钥一致性和尾随 DER 拒绝，无效 EC JWKS 条目后的槽位复用，
以及 RSA/EC PKCS#8 可选属性的签验与畸形结构拒绝。
