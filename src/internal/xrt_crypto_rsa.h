#ifndef XRT_INTERNAL_CRYPTO_RSA_H
#define XRT_INTERNAL_CRYPTO_RSA_H

#include "xrt_crypto_int31.h"



#if defined(XRT_FEATURE_CRYPTO_RSA)

#define XRT_RSA_MIN_MODULUS_SIZE 128u
#define XRT_RSA_MAX_MODULUS_SIZE 1024u
#define XRT_RSA_MAX_I31_WORDS 267u
#define XRT_RSA_MAX_FACTOR_SIZE 512u
#define XRT_RSA_MAX_FACTOR_I31_WORDS 136u



/* RSA 内部失败类型用于区分密钥错误与签名编码错误。 */
typedef enum __xrt_rsa_result {
	XRT_RSA_RESULT_OK = 0,
	XRT_RSA_RESULT_ARGUMENT,
	XRT_RSA_RESULT_KEY,
	XRT_RSA_RESULT_INPUT,
	XRT_RSA_RESULT_RANDOM
} __xrt_rsa_result;



/* 设置 RSA 操作的结构化错误。 */
void __xrtRsaError(cstr sOperation, cstr sMessage, int iCode);



/* 判断大端字节串是否表示大于一的奇数。 */
bool __xrtRsaExponentValid(const uint8* pExponent, size_t iSize);



/* 验证 RSA 公钥视图的尺寸、模数和指数基本约束。 */
bool __xrtRsaKeyValid(const xrsapublickey* pKey);



/* 执行严格输入检查后的 RSA 模幂，不直接修改线程错误。 */
__xrt_rsa_result __xrtRsaPower(
	const xrsapublickey* pKey,
	const void* pInput,
	size_t iInputSize,
	void* pOutput
);



#if defined(XRT_FEATURE_CRYPTO_RSA_PRIVATE)

/* 固定轨迹计算 x / y mod m；临时区至少容纳 3 * 模数字数，失败返回 0。 */
uint32 __xrtRsaModDivide(uint32* x, const uint32* y, const uint32* m,
	uint32 m0i, uint32* t);

/* 只供盲化包装使用的 CRT/完整指数核心；输入已经随机化。 */
__xrt_rsa_result __xrtRsaPrivateCore(const xrsaprivatekey* pKey,
	const void* pInput, size_t iInputSize, void* pOutput);

/* 随机基底盲化和结果复核；RANDOM 失败保留安全随机源的线程错误。 */
__xrt_rsa_result __xrtRsaPrivatePower(
	const xrsaprivatekey* pKey,
	const void* pInput,
	size_t iInputSize,
	void* pOutput
);

#endif

#endif

#endif
