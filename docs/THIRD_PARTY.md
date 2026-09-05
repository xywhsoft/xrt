# 第三方组件

XRT 会把复用的第三方实现隔离在明确的模块边界中，并在源码和发布文档中保留许可证。以下组件已进入 2.0 源码。

## yyjson 0.12.0 数值内核

- 作者：YaoYuan
- 来源：<https://github.com/ibireme/yyjson>
- 使用位置：`src/text/number_float_core.c`、`src/third_party/yyjson`
- 许可证：MIT License
- 改动：没有引入 JSON DOM、读取器或写出器；只精炼复用 Eisel-Lemire 风格读取快路、固定栈 BigInt 精确舍入后备、Schubfach 最短写出和二者共用的 128 位十进制幂表。XRT 自行实现显式长度语法、错误、分配、容量和公开 API。

Copyright (c) 2020 YaoYuan <ibireme@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## miniz 3.1.2

- 作者：Rich Geldreich、Tenacious Software LLC、RAD Game Tools 和 Valve Software
- 来源：<https://github.com/richgel999/miniz>
- 使用位置：`src/third_party/miniz`、`src/compress/inflate.c`、
  `src/compress/deflate.c`
- 许可证：MIT License
- 改动：复用旧版 XRT 已验证的 `tinfl` 与 `tdefl` 资产；只编译所选编解码
  单元，把公开 API、分配、结构化错误、输出限额和 gzip 完整性收口到独立
  Inflate/Deflate 模块，第三方类型不进入公共头文件。`tinfl` 与 `tdefl` 均
  精炼掉 XRT 不使用的 heap、固定缓冲、callback wrapper、PNG、状态查询和
  分配 helper；`tdefl` 使用约 164 KiB 的低内存状态布局；增加 8 到 15 位
  精确窗口限制，使解码器校验线路
  回溯距离和当前流实际可用历史，编码器限制匹配距离并写出对应 zlib CINFO；
  复位保留的字典内存不会被非法流作为历史读取。

Copyright 2013-2014 RAD Game Tools and Valve Software
Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

## nmhash32x v2.0

- 作者：James Z.M. Gao
- 来源：<https://github.com/rurban/smhasher>
- 使用位置：`src/hash/nmhash32.c`
- 许可证：BSD 2-Clause

Copyright (c) 2021, James Z.M. Gao. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

## BearSSL 0.6 AES ct64 与 GHASH ctmul64

- 作者：Thomas Pornin
- 来源：<https://bearssl.org/>
- 使用位置：`src/crypto/aes.c`、`src/crypto/aes_gcm.c`
- 许可证：MIT License
- 改动：适配 XRT 类型、错误模型、代码风格、公开状态和批处理边界；保留常量时间位切片 S-box、AES 轮函数与 GHASH 乘法核心。

Copyright (c) 2016 Thomas Pornin <pornin@bolet.org>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## BearSSL 0.6 int31 与 prime_i31

- 作者：Thomas Pornin
- 来源：https://bearssl.org/
- 使用位置：`src/crypto/int31.c`、`src/crypto/nist.c`、`src/crypto/rsa.c`、`src/crypto/rsa_private.c`、`src/internal/xrt_crypto_int31.h`、`src/internal/xrt_crypto_nist.h`、`src/internal/xrt_crypto_rsa.h`
- 许可证：MIT License
- 改动：适配 XRT 类型、命名、注释和裁剪边界；保留曲线与 RSA 共用的常数时间原语以及 P-256/P-384 点公式，以预计算 R^2 输入替代通用 Montgomery 转换依赖，并按同一 int31 表示补充 RSA 双素数 CRT 归约与重组路径。

Copyright (c) 2016 Thomas Pornin <pornin@bolet.org>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## BearSSL int31 modular division

- 作者：Thomas Pornin，Copyright (c) 2018 Thomas Pornin <pornin@bolet.org>
- 来源：[官方 i31_moddiv.c](https://www.bearssl.org/gitweb/?p=BearSSL;a=blob_plain;f=src/int/i31_moddiv.c;hb=HEAD)，2026-09-05 取用。
- 使用位置：`src/crypto/rsa_blinding.c`；只随 RSA 私钥模块裁剪编入。
- 许可证：MIT；文件开头完整保留原版权与许可声明，生成的单头也保留声明。
- 改动：XRT 命名与类型适配；以 `memcpy` 保持原有位转换且避免严格别名违规；新增随机基底盲化包装，不修改模除法的固定迭代算法。

## Brad Conte SHA-256

- 作者：Brad Conte
- 来源：<https://github.com/B-Con/crypto-algorithms>
- 使用位置：`src/crypto/sha256.c`
- 许可证：Public Domain
- 改动：扩展 SHA-224，共享压缩核心，并补充状态 Guard、累计长度上限、失败原子性、分块一致性和统一错误模型。

## portable8439 与 poly1305-donna

- 作者：portable8439 contributors；Andrew Moon（poly1305-donna）
- 来源：<https://github.com/floodyberry/poly1305-donna>
- 使用位置：`src/crypto/chacha20.c`、`src/crypto/poly1305.c`
- 许可证：portable8439 为 CC0-1.0；poly1305-donna 为 Public Domain
- 改动：重构为 XRT 的无分配流状态、分离裁剪边界、参数检查、计数器上限、失败原子性和敏感数据清理；未直接复制旧版外围 API。

## Mike Hamburg / STROBE X25519

- 作者：Mike Hamburg；Cryptography Research, Inc.
- 来源：STROBE / mongoose 集成链。
- 使用位置：`src/crypto/curve25519.c`；X25519 与 Ed25519 共用该有限域算术层。
- 许可证：MIT License
- 改动：适配 XRT 类型、命名、裁剪和结构化错误；增加输入 u-coordinate 最高位屏蔽、低阶全零拒绝、失败原子性、任意缓冲重叠、敏感中间值清理，并用显式无符号借位替代实现定义的负数算术右移。

Copyright (c) 2015-2016 Cryptography Research, Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## BBRE 0.0.2

- 作者：Max Nurzia
- 来源：<https://github.com/mnurzia/bbre>；旧版 XRT 已保存完整源码、生成测试、模糊测试语料和开发工具
- 使用位置：`src/third_party/bbre/bbre.c`、`src/third_party/bbre/bbre.h`
- 许可证：MIT License
- 改动：修复命名组释放大小与 clone 丢失组名；增加 XRT 独立执行上下文、非零起点捕获修复和真正的完整输入匹配；完整测试、基准与历史资产由核心 Regex 模块维护。

Copyright (c) 2024 Max Nurzia

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## PCG Random Number Generation for C

- 作者：Melissa O'Neill
- 来源：<https://github.com/imneme/pcg-c-basic>
- 使用位置：`src/math/random.c`
- 许可证：Apache License 2.0

Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at:

<http://www.apache.org/licenses/LICENSE-2.0>

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.

## rapidhash v3.0

- 作者：Nicolas De Carli
- 来源：<https://github.com/Nicoshev/rapidhash>
- 使用位置：`src/hash/rapidhash.c`
- 许可证：BSD 2-Clause

Copyright (C) 2024 Nicolas De Carli.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
