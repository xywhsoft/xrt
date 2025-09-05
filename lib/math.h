


/*
	PCG Random Number Generation for C. [ Update : 2025/08/19 from https://github.com/imneme/pcg-c-basic ]
	修改：
		整合到 xrt 库
	使用协议注意事项：
		Apache License, Version 2.0 协议
		允许个人使用、商业使用
		复制、分发、修改，除了加上作者的版权信息，还必须保留免责声明，免除作者的责任
*/



/*
 * PCG Random Number Generation for C.
 *
 * Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For additional information about the PCG random number generation scheme,
 * including its license and other licensing options, visit
 *
 *       http://www.pcg-random.org
 */



// PCG 算法随机数状态结构
struct pcg_state_setseq_64 {    // Internals are *Private*.
	uint64_t state;             // RNG state.  All values are possible.
	uint64_t inc;               // Controls which RNG sequence (stream) is
								// selected. Must *always* be odd.
};
typedef struct pcg_state_setseq_64 pcg32_random_t;



// 如果你必须静态初始化随机数，推荐使用这个值
#define PCG32_INITIALIZER   { 0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL }



static pcg32_random_t pcg32_global = PCG32_INITIALIZER;



// pcg32_random()
// pcg32_random_r(rng)
//     Generate a uniformly distributed 32-bit random number

uint32_t pcg32_random_r(pcg32_random_t* rng)
{
    uint64_t oldstate = rng->state;
    rng->state = oldstate * 6364136223846793005ULL + rng->inc;
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}



// pcg32_srandom(initstate, initseq)
// pcg32_srandom_r(rng, initstate, initseq):
//     Seed the rng.  Specified in two parts, state initializer and a
//     sequence selection constant (a.k.a. stream id)

void pcg32_srandom_r(pcg32_random_t* rng, uint64_t initstate, uint64_t initseq)
{
    rng->state = 0U;
    rng->inc = (initseq << 1u) | 1u;
    pcg32_random_r(rng);
    rng->state += initstate;
    pcg32_random_r(rng);
}



// pcg32_boundedrand(bound):
// pcg32_boundedrand_r(rng, bound):
//     Generate a uniformly distributed number, r, where 0 <= r < bound

uint32_t pcg32_boundedrand_r(pcg32_random_t* rng, uint32_t bound)
{
    // To avoid bias, we need to make the range of the RNG a multiple of
    // bound, which we do by dropping output less than a threshold.
    // A naive scheme to calculate the threshold would be to do
    //
    //     uint32_t threshold = 0x100000000ull % bound;
    //
    // but 64-bit div/mod is slower than 32-bit div/mod (especially on
    // 32-bit platforms).  In essence, we do
    //
    //     uint32_t threshold = (0x100000000ull-bound) % bound;
    //
    // because this version will calculate the same modulus, but the LHS
    // value is less than 2^32.

    uint32_t threshold = -bound % bound;

    // Uniformity guarantees that this loop will terminate.  In practice, it
    // should usually terminate quickly; on average (assuming all bounds are
    // equally likely), 82.25% of the time, we can expect it to require just
    // one iteration.  In the worst case, someone passes a bound of 2^31 + 1
    // (i.e., 2147483649), which invalidates almost 50% of the range.  In 
    // practice, bounds are typically small and only a tiny amount of the range
    // is eliminated.
    for (;;) {
        uint32_t r = pcg32_random_r(rng);
        if (r >= threshold)
            return r % bound;
    }
}







// 获取 32 位随机数
XXAPI uint32 xrtRand32()
{
	return pcg32_random_r(&pcg32_global);
}



// 设置 32 位随机数种子
XXAPI void xrtSetRandSeed32(uint64 seed, uint64 seq)
{
	pcg32_srandom_r(&pcg32_global, seed, seq);
}



// 获取 64 位随机数
static pcg32_random_t pcg32_global_64_low = PCG32_INITIALIZER;
static pcg32_random_t pcg32_global_64_high = PCG32_INITIALIZER;
XXAPI uint64 xrtRand64()
{
	uint32 iLow = pcg32_random_r(&pcg32_global_64_low);
	uint32 iHigh = pcg32_random_r(&pcg32_global_64_high);
	return ((uint64)iHigh << 32) | (uint64)iLow;
}



// 设置 64 位随机数种子
XXAPI void xrtSetRandSeed64(uint64 lowseed, uint64 lowseq, uint64 highseed, uint64 highseq)
{
	pcg32_srandom_r(&pcg32_global_64_low, lowseed, lowseq);
	pcg32_srandom_r(&pcg32_global_64_high, highseed, highseq);
}



// 获取 32 位范围随机数
XXAPI int xrtRandRange(int min, int max)
{
	uint32 iRange = (max - min) + 1;
	if ( iRange > 0 ) {
		return pcg32_boundedrand_r(&pcg32_global, iRange) + min;
	} else if ( iRange < 0 ) {
		iRange = (min - max) + 1;
		return pcg32_boundedrand_r(&pcg32_global, iRange) + max;
	} else {
		return 0;
	}
}


