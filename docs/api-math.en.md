# Math - Mathematics Library

> Random number generation and mathematical operations (based on PCG algorithm)

[English](api-math.en.md) | [中文](api-math.md) | [Back to Index](README.en.md)

---

## Table of Contents

- [Data Types](#data-types)
- [Constants](#constants)
- [Random Number Generation](#random-number-generation)
  - [Basic API (Not Thread-Safe)](#xrtrand32)
  - [Ex Version API (Thread-Safe)](#xrtrand32ex)
- [Approximate Comparison](#approximate-comparison)
  - [xrtIntApprox](#xrtintapprox)
  - [xrtNumApprox](#xrtnumapprox)
- [Usage Scenarios](#usage-scenarios)
- [Best Practices](#best-practices)
- [Performance Tips](#performance-tips)
- [Algorithm Quality](#algorithm-quality)
- [Common Errors](#common-errors)

---

## Data Types

### xrand

PCG algorithm random number generator state structure.

**Definition:**
```c
typedef struct {
    uint64 state;  // RNG state
    uint64 inc;    // Controls RNG sequence (stream)
} xrand;
```

**Notes:**
- Used for Ex version API where caller manages random number state
- Basic API uses global state `xCore.rand32`, `xCore.rand64_low`, `xCore.rand64_high`
- Can use `XRAND_INITIALIZER` macro for static initialization

---

## Constants

### XRAND_INITIALIZER

Recommended value for static initialization of random number generator.

**Definition:**
```c
#define XRAND_INITIALIZER  { 0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL }
```

**Notes:**
- Used for statically initializing `xrand` struct
- Example: `xrand rng = XRAND_INITIALIZER;`
- Global state for basic API is auto-initialized by `xrtInit()`

---

## Random Number Generation

### xrtRand32

Generate a 32-bit random number.

**Function Prototype:**
```c
XXAPI uint32 xrtRand32();
```

**Return Value:**
- Returns a random number in range 0 ~ 4294967295 (2^32-1)

**Notes:**
- Uses PCG32 algorithm (high-quality pseudo-random number generator)
- Automatically initialized in `xrtInit()`
- Not thread-safe (needs synchronization for multi-threading)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Generate single random number
    uint32 random = xrtRand32();
    printf("Random: %u\n", random);
    
    // Generate multiple random numbers
    printf("10 random numbers: ");
    for (int i = 0; i < 10; i++) {
        printf("%u ", xrtRand32());
    }
    printf("\n");
    
    xrtUnit();
    return 0;
}
```

---

### xrtRandSeed

Initialize random number generator.

**Function Prototype:**
```c
XXAPI void xrtRandSeed(xrand* rng, uint64 seed, uint64 seq);
```

**Parameters:**
- `rng` - Pointer to random number generator state
- `seed` - Random number seed
- `seq` - Sequence number (for generating different sequences)

**Notes:**
- Same seed and sequence produce same random number sequence
- Can be used for reproducible random sequences (testing, debugging)
- `seq` parameter is internally converted to odd (`(seq << 1) | 1`)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Create independent random number generator
    xrand myRng = XRAND_INITIALIZER;
    xrtRandSeed(&myRng, 12345, 1);
    
    // Use fixed seed (reproducible)
    printf("Fixed seed result: ");
    for (int i = 0; i < 5; i++) {
        printf("%u ", xrtRand32Ex(&myRng));
    }
    printf("\n");
    
    // Reset seed, same result
    xrtRandSeed(&myRng, 12345, 1);
    printf("After reset result: ");
    for (int i = 0; i < 5; i++) {
        printf("%u ", xrtRand32Ex(&myRng));
    }
    printf("\n");
    
    xrtUnit();
    return 0;
}
```

---

### xrtRand64

Generate a 64-bit random number (not thread-safe).

**Function Prototype:**
```c
XXAPI uint64 xrtRand64();
```

**Return Value:**
- Returns a random number in range 0 ~ 18446744073709551615 (2^64-1)

**Notes:**
- Internally implemented by combining two 32-bit random numbers (high + low)
- Auto-initialized in `xrtInit()`
- Suitable for scenarios needing larger range (e.g., generating unique IDs)
- **Not thread-safe**, use `xrtRand64Ex` for multi-threading

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Generate 64-bit random number
    uint64 random = xrtRand64();
    printf("Random64: %llu\n", random);
    
    // Generate large range random IDs
    for (int i = 0; i < 5; i++) {
        uint64 id = xrtRand64();
        printf("ID %d: 0x%016llX\n", i, id);
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtRandRange

Generate a random integer within specified range (not thread-safe).

**Function Prototype:**
```c
XXAPI int xrtRandRange(int min, int max);
```

**Parameters:**
- `min` - Minimum value (inclusive)
- `max` - Maximum value (inclusive)

**Return Value:**
- Returns a random integer in [min, max] range
- If `min == max`, returns 0

**Notes:**
- Based on `xrtRand32()` implementation, uses unbiased algorithm
- **Includes** both max and min values
- Supports negative ranges
- If `min > max`, automatically swaps

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Generate 1-100 random number
    int value = xrtRandRange(1, 100);
    printf("1-100: %d\n", value);
    
    // Generate 0-9 random digit
    int digit = xrtRandRange(0, 9);
    printf("0-9: %d\n", digit);
    
    // Generate negative range
    int signed_val = xrtRandRange(-10, 10);
    printf("-10 to 10: %d\n", signed_val);
    
    // Dice simulation (6-sided die)
    printf("Roll dice 10 times: ");
    for (int i = 0; i < 10; i++) {
        printf("%d ", xrtRandRange(1, 6));
    }
    printf("\n");
    
    // Auto swap when min > max
    int swapped = xrtRandRange(100, 1);  // Equivalent to xrtRandRange(1, 100)
    printf("Auto swapped: %d\n", swapped);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Internally uses unbiased algorithm
- On average 82.25% of calls need only one loop iteration
- **Not thread-safe**, use `xrtRandRangeEx` for multi-threading

---

### xrtRand32Ex

Generate a 32-bit random number (thread-safe).

**Function Prototype:**
```c
XXAPI uint32 xrtRand32Ex(xrand* rng);
```

**Parameters:**
- `rng` - Pointer to random number generator state

**Return Value:**
- Returns a random number in range 0 ~ 4294967295 (2^32-1)

**Notes:**
- Caller manages state, safe to use in multi-threaded environment
- Each thread using its own `xrand` instance avoids race conditions

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Create thread-independent random number generator
    xrand myRng = XRAND_INITIALIZER;
    xrtRandSeed(&myRng, 12345, 1);
    
    printf("Generate 10 32-bit random numbers:\n");
    for (int i = 0; i < 10; i++) {
        printf("%u\n", xrtRand32Ex(&myRng));
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtRand64Ex

Generate a 64-bit random number (thread-safe).

**Function Prototype:**
```c
XXAPI uint64 xrtRand64Ex(xrand* rngLow, xrand* rngHigh);
```

**Parameters:**
- `rngLow` - Pointer to low-bits random number generator state
- `rngHigh` - Pointer to high-bits random number generator state

**Return Value:**
- Returns a random number in range 0 ~ 18446744073709551615 (2^64-1)

**Notes:**
- Caller manages state, safe to use in multi-threaded environment
- Requires two independent `xrand` instances for high and low bits

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Create two independent generators
    xrand rngLow = XRAND_INITIALIZER;
    xrand rngHigh = XRAND_INITIALIZER;
    xrtRandSeed(&rngLow, 11111, 1);
    xrtRandSeed(&rngHigh, 22222, 3);
    
    printf("Generate 5 64-bit random numbers:\n");
    for (int i = 0; i < 5; i++) {
        printf("0x%016llX\n", xrtRand64Ex(&rngLow, &rngHigh));
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtRandRangeEx

Generate a random integer within specified range (thread-safe).

**Function Prototype:**
```c
XXAPI int xrtRandRangeEx(xrand* rng, int min, int max);
```

**Parameters:**
- `rng` - Pointer to random number generator state
- `min` - Minimum value (inclusive)
- `max` - Maximum value (inclusive)

**Return Value:**
- Returns a random integer in [min, max] range

**Notes:**
- Caller manages state, safe to use in multi-threaded environment
- Supports negative ranges
- If `min > max`, automatically swaps

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xrand myRng = XRAND_INITIALIZER;
    xrtRandSeed(&myRng, 67890, 1);
    
    // Dice simulation
    printf("Roll dice 10 times:\n");
    for (int i = 0; i < 10; i++) {
        printf("%d ", xrtRandRangeEx(&myRng, 1, 6));
    }
    printf("\n");
    
    xrtUnit();
    return 0;
}
```

---

## Approximate Comparison

Approximate comparison functions determine if two values are equal within an allowed tolerance. Comparison rules are configured via global `xCore`.

### Configuration Fields

| Field | Type | Description |
|-------|------|-------------|
| `xCore.iApproxIntMode` | `int` | Integer comparison mode: `XRT_APPROX_DIFF`(difference) or `XRT_APPROX_PERCENT`(percentage) |
| `xCore.fApproxIntTol` | `double` | Integer tolerance (absolute value for diff mode, decimal like 0.01=1% for percent mode) |
| `xCore.iApproxNumMode` | `int` | Float comparison mode: `XRT_APPROX_DIFF`(difference) or `XRT_APPROX_PERCENT`(percentage) |
| `xCore.fApproxNumTol` | `double` | Float tolerance |

### Mode Constants

```c
#define XRT_APPROX_DIFF     0   // Difference mode: |a - b| <= tolerance
#define XRT_APPROX_PERCENT  1   // Percentage mode: |a - b| / max(|a|, |b|) <= tolerance
```

---

### xrtIntApprox

Determine if two integers are approximately equal.

**Function Prototype:**
```c
XXAPI bool xrtIntApprox(int64 a, int64 b);
```

**Parameters:**
- `a` - First integer
- `b` - Second integer

**Return Value:**
- `TRUE` - Two numbers are within tolerance
- `FALSE` - Two numbers exceed tolerance

**Notes:**
- Uses `xCore.iApproxIntMode` and `xCore.fApproxIntTol` configuration
- Percentage mode uses the larger absolute value as the base

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Percentage mode: 1% tolerance
    xCore.iApproxIntMode = XRT_APPROX_PERCENT;
    xCore.fApproxIntTol = 0.01;  // 1%
    
    printf("10000 ~ 9900: %s\n", xrtIntApprox(10000, 9900) ? "TRUE" : "FALSE");  // TRUE (1% diff)
    printf("10000 ~ 9000: %s\n", xrtIntApprox(10000, 9000) ? "TRUE" : "FALSE");  // FALSE (10% diff)
    
    // Difference mode: tolerance 10
    xCore.iApproxIntMode = XRT_APPROX_DIFF;
    xCore.fApproxIntTol = 10.0;
    
    printf("100 ~ 108: %s\n", xrtIntApprox(100, 108) ? "TRUE" : "FALSE");  // TRUE (diff 8)
    printf("100 ~ 120: %s\n", xrtIntApprox(100, 120) ? "TRUE" : "FALSE");  // FALSE (diff 20)
    
    xrtUnit();
    return 0;
}
```

---

### xrtNumApprox

Determine if two floating-point numbers are approximately equal.

**Function Prototype:**
```c
XXAPI bool xrtNumApprox(double a, double b);
```

**Parameters:**
- `a` - First floating-point number
- `b` - Second floating-point number

**Return Value:**
- `TRUE` - Two numbers are within tolerance
- `FALSE` - Two numbers exceed tolerance

**Notes:**
- Uses `xCore.iApproxNumMode` and `xCore.fApproxNumTol` configuration
- Suitable for floating-point precision comparison scenarios

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Difference mode: tolerance 0.001
    xCore.iApproxNumMode = XRT_APPROX_DIFF;
    xCore.fApproxNumTol = 0.001;
    
    printf("3.14159 ~ 3.14160: %s\n", xrtNumApprox(3.14159, 3.14160) ? "TRUE" : "FALSE");  // TRUE
    printf("3.14159 ~ 3.15000: %s\n", xrtNumApprox(3.14159, 3.15000) ? "TRUE" : "FALSE");  // FALSE
    
    // Percentage mode: 0.1% tolerance
    xCore.iApproxNumMode = XRT_APPROX_PERCENT;
    xCore.fApproxNumTol = 0.001;  // 0.1%
    
    printf("100.0 ~ 99.95: %s\n", xrtNumApprox(100.0, 99.95) ? "TRUE" : "FALSE");  // TRUE (0.05% diff)
    printf("100.0 ~ 99.00: %s\n", xrtNumApprox(100.0, 99.00) ? "TRUE" : "FALSE");  // FALSE (1% diff)
    
    xrtUnit();
    return 0;
}
```

---

## Usage Scenarios

### 1. Random Array Generation

```c
#include "xrt.h"
#include <stdio.h>

// Generate random integer array
void GenerateRandomArray(int* arr, int count, int min, int max) {
    for (int i = 0; i < count; i++) {
        arr[i] = xrtRandRange(min, max);
    }
}

int main() {
    xrtInit();
    
    int numbers[20];
    GenerateRandomArray(numbers, 20, 1, 100);
    
    printf("20 random numbers 1-100:\n");
    for (int i = 0; i < 20; i++) {
        printf("%3d ", numbers[i]);
        if ((i + 1) % 10 == 0) printf("\n");
    }
    
    xrtUnit();
    return 0;
}
```

---

### 2. Random String Generation

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

// Generate random string
str GenerateRandomString(int length) {
    const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    int charset_size = (int)strlen(chars);
    
    str result = xrtMalloc(length + 1);
    for (int i = 0; i < length; i++) {
        int idx = xrtRandRange(0, charset_size - 1);
        result[i] = chars[idx];
    }
    result[length] = '\0';
    
    return result;
}

int main() {
    xrtInit();
    
    // Generate 8-character random password
    str password = GenerateRandomString(8);
    printf("Random password: %s\n", password);
    xrtFree(password);
    
    // Generate 16-character random token
    str token = GenerateRandomString(16);
    printf("Random token: %s\n", token);
    xrtFree(token);
    
    xrtUnit();
    return 0;
}
```

---

### 3. Array Shuffle

```c
#include "xrt.h"
#include <stdio.h>

// Fisher-Yates shuffle algorithm
void ShuffleArray(int* arr, int count) {
    for (int i = count - 1; i > 0; i--) {
        int j = xrtRandRange(0, i);
        // Swap arr[i] and arr[j]
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

int main() {
    xrtInit();
    
    // Initialize deck (52 cards)
    int cards[52];
    for (int i = 0; i < 52; i++) {
        cards[i] = i + 1;
    }
    
    printf("Before shuffle: ");
    for (int i = 0; i < 10; i++) printf("%d ", cards[i]);
    printf("...\n");
    
    // Shuffle
    ShuffleArray(cards, 52);
    
    printf("After shuffle: ");
    for (int i = 0; i < 10; i++) printf("%d ", cards[i]);
    printf("...\n");
    
    xrtUnit();
    return 0;
}
```

---

### 4. Probability Events

```c
#include "xrt.h"
#include <stdio.h>

// Return TRUE/FALSE by probability
bool RandomChance(double probability) {
    // probability: 0.0 ~ 1.0
    uint32 threshold = (uint32)(probability * 0xFFFFFFFF);
    return xrtRand32() < threshold;
}

// Simulate lottery
bool IsWinner(double win_rate) {
    return RandomChance(win_rate);
}

int main() {
    xrtInit();
    
    // Test 30% probability
    int triggered = 0;
    int total = 10000;
    for (int i = 0; i < total; i++) {
        if (RandomChance(0.3)) {
            triggered++;
        }
    }
    printf("30%% probability test: %d/%d = %.2f%%\n", 
           triggered, total, (double)triggered / total * 100);
    
    // Simulate lottery (1% win rate)
    int wins = 0;
    for (int i = 0; i < 10000; i++) {
        if (IsWinner(0.01)) {
            wins++;
        }
    }
    printf("1%% win rate: 10000 draws, won %d times\n", wins);
    
    xrtUnit();
    return 0;
}
```

---

### 5. Random Sampling

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

// Fisher-Yates shuffle
void ShuffleIntArray(int* arr, int count) {
    for (int i = count - 1; i > 0; i--) {
        int j = xrtRandRange(0, i);
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

// Randomly select N elements from array
void RandomSample(int* source, int src_count, int* dest, int sample_count) {
    // Copy source array
    int* temp = xrtMalloc(src_count * sizeof(int));
    memcpy(temp, source, src_count * sizeof(int));
    
    // Shuffle
    ShuffleIntArray(temp, src_count);
    
    // Take first N
    memcpy(dest, temp, sample_count * sizeof(int));
    xrtFree(temp);
}

int main() {
    xrtInit();
    
    // Source data: 1-20
    int source[20];
    for (int i = 0; i < 20; i++) source[i] = i + 1;
    
    // Randomly sample 5
    int sample[5];
    RandomSample(source, 20, sample, 5);
    
    printf("Randomly sample 5 from 1-20: ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", sample[i]);
    }
    printf("\n");
    
    xrtUnit();
    return 0;
}
```

---

### 6. Random Delay

```c
#include "xrt.h"
#include <stdio.h>

// Random delay (prevent synchronization)
void RandomDelay(int min_ms, int max_ms) {
    int delay = xrtRandRange(min_ms, max_ms);
    printf("Delay %d milliseconds...\n", delay);
    xrtSleep(delay);
}

int main() {
    xrtInit();
    
    printf("Simulate network request random delay:\n");
    for (int i = 0; i < 3; i++) {
        printf("Request %d: ", i + 1);
        RandomDelay(100, 500);  // 100-500ms random delay
    }
    
    xrtUnit();
    return 0;
}
```

---

### 7. UUID/GUID Generation

```c
#include "xrt.h"
#include <stdio.h>

// Simplified UUID generation (non-standard, for demonstration only)
str GenerateSimpleUUID() {
    return xrtFormat(
        "%08X-%04X-%04X-%04X-%012" PRIX64,
        xrtRand32(),
        xrtRand32() & 0xFFFF,
        xrtRand32() & 0xFFFF,
        xrtRand32() & 0xFFFF,
        xrtRand64() & 0xFFFFFFFFFFFFULL
    );
}

int main() {
    xrtInit();
    
    printf("Generate 5 random UUIDs:\n");
    for (int i = 0; i < 5; i++) {
        str uuid = GenerateSimpleUUID();
        printf("  %s\n", uuid);
        xrtFree(uuid);
    }
    
    xrtUnit();
    return 0;
}
```

**Note:** For standard UUIDs, use [XID Distributed ID Module](api-xid.en.md).

---

## Best Practices

### 1. Initialize Seed

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();  // Auto-initialize random number seed
    
    // Basic usage: directly call (global state initialized by xrtInit)
    printf("Random number: %u\n", xrtRand32());
    
    // Ex version: create independent generator
    xrand myRng = XRAND_INITIALIZER;
    xrtRandSeed(&myRng, 12345, 1);
    printf("Ex version: %u\n", xrtRand32Ex(&myRng));
    
    xrtUnit();
    return 0;
}
```

---

### 2. Reproducible Random Sequence

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Use fixed seed for reproducible random sequence
    xrand rng = XRAND_INITIALIZER;
    
#ifdef DEBUG
    // Test environment: use fixed seed, reproducible results
    xrtRandSeed(&rng, 12345, 1);
    printf("Debug mode: using fixed seed\n");
#else
    // Production environment: use global API or random seed
    printf("Production mode: using default seed\n");
#endif
    
    printf("Random sequence: ");
    for (int i = 0; i < 5; i++) {
        printf("%u ", xrtRand32Ex(&rng));
    }
    printf("\n");
    
    xrtUnit();
    return 0;
}
```

---

### 3. Avoid Modulo Bias

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ✗ Biased method
    // int bad_random = xrtRand32() % 100;  // 0-99, slight bias
    
    // ✓ Unbiased method
    int good_random = xrtRandRange(0, 99);  // Internally handles bias
    printf("Unbiased random (0-99): %d\n", good_random);
    
    // Verify distribution
    int counts[10] = {0};
    for (int i = 0; i < 100000; i++) {
        int r = xrtRandRange(0, 9);
        counts[r]++;
    }
    
    printf("\nDistribution stats (100000 times, ideal 10000 each):\n");
    for (int i = 0; i < 10; i++) {
        printf("  %d: %d\n", i, counts[i]);
    }
    
    xrtUnit();
    return 0;
}
```

---

### 4. Floating-Point Random Numbers

```c
#include "xrt.h"
#include <stdio.h>

// Generate [0.0, 1.0) floating-point random number
double RandomDouble() {
    return (double)xrtRand32() / (double)0x100000000ULL;
}

// Generate [min, max) floating-point random number
double RandomDoubleRange(double min, double max) {
    return min + (max - min) * RandomDouble();
}

int main() {
    xrtInit();
    
    printf("10 [0, 1) random floats:\n");
    for (int i = 0; i < 10; i++) {
        printf("  %.6f\n", RandomDouble());
    }
    
    printf("\n5 [10.0, 100.0) random prices:\n");
    for (int i = 0; i < 5; i++) {
        double price = RandomDoubleRange(10.0, 100.0);
        printf("  $%.2f\n", price);
    }
    
    xrtUnit();
    return 0;
}
```

---

### 5. Multi-Thread Safety

```c
#include "xrt.h"
#include <stdio.h>

// Thread entry function
XRT_THREAD_PROC(ThreadFunc, param)
{
    // Each thread uses its own random number generator
    xrand myRng = XRAND_INITIALIZER;
    xrtRandSeed(&myRng, (uint64)(size_t)param, 1);  // Use thread param as seed
    
    for (int i = 0; i < 5; i++) {
        uint32 r = xrtRand32Ex(&myRng);
        printf("Thread%lld: %u\n", (uint64)(size_t)param, r);
    }
    return 0;
}

int main() {
    xrtInit();
    
    // Create multiple threads
    xrt_thread t1, t2;
    xrtThreadCreate(&t1, ThreadFunc, (ptr)1);
    xrtThreadCreate(&t2, ThreadFunc, (ptr)2);
    
    // Wait for threads to complete
    xrtThreadJoin(&t1);
    xrtThreadJoin(&t2);
    
    xrtUnit();
    return 0;
}
```

**Notes:**
- Basic API (`xrtRand32`/`xrtRand64`/`xrtRandRange`) uses global state, not thread-safe
- Ex version API (`xrtRand32Ex`/`xrtRand64Ex`/`xrtRandRangeEx`) caller manages state
- Each thread creating its own `xrand` instance achieves thread safety

---

### 6. Weighted Random Selection

```c
#include "xrt.h"
#include <stdio.h>

// Generate [0.0, 1.0) floating-point random number
double RandomDouble() {
    return (double)xrtRand32() / (double)0x100000000ULL;
}

// Random select by weight
int WeightedRandom(int* items, double* weights, int count) {
    double total = 0;
    for (int i = 0; i < count; i++) {
        total += weights[i];
    }
    
    double random = RandomDouble() * total;
    double cumulative = 0;
    
    for (int i = 0; i < count; i++) {
        cumulative += weights[i];
        if (random < cumulative) {
            return items[i];
        }
    }
    
    return items[count - 1];
}

int main() {
    xrtInit();
    
    // Items and weights
    int items[] = {1, 2, 3, 4};
    double weights[] = {0.1, 0.2, 0.3, 0.4};  // 10%, 20%, 30%, 40%
    
    // Count selections
    int counts[5] = {0};
    for (int i = 0; i < 10000; i++) {
        int selected = WeightedRandom(items, weights, 4);
        counts[selected]++;
    }
    
    printf("Weighted random selection results (10000 times):\n");
    for (int i = 1; i <= 4; i++) {
        printf("  Item %d: %d times (%.1f%%)\n", i, counts[i], counts[i] / 100.0);
    }
    
    xrtUnit();
    return 0;
}
```

---

## Performance Tips

### 1. Batch Generation

```c
#include "xrt.h"
#include <stdio.h>

void Process(int value) {
    // Process random number...
}

int main() {
    xrtInit();
    
    int count = 100000;
    
    // ✗ Inefficient: generate and process alternating
    printf("Method 1: Generate and process alternating\n");
    for (int i = 0; i < count; i++) {
        int r = xrtRandRange(1, 100);
        Process(r);
    }
    
    // ✓ Efficient: pre-generate all random numbers
    printf("Method 2: Pre-generate all random numbers\n");
    int* randoms = xrtMalloc(count * sizeof(int));
    for (int i = 0; i < count; i++) {
        randoms[i] = xrtRandRange(1, 100);
    }
    for (int i = 0; i < count; i++) {
        Process(randoms[i]);
    }
    xrtFree(randoms);
    
    printf("Done\n");
    xrtUnit();
    return 0;
}
```

---

### 2. Avoid Repeated Initialization

```c
#include "xrt.h"
#include <stdio.h>

// ✗ Wrong: resetting seed every time causes same result
uint32 BadRandom() {
    xrand rng = XRAND_INITIALIZER;
    xrtRandSeed(&rng, 12345, 1);  // Same seed every time
    return xrtRand32Ex(&rng);     // Returns first random number each time!
}

// ✓ Correct: initialize only once at start
static xrand g_rng;
static int g_rng_init = 0;

void Setup() {
    if (!g_rng_init) {
        g_rng = (xrand)XRAND_INITIALIZER;
        xrtRandSeed(&g_rng, 12345, 1);
        g_rng_init = 1;
    }
}

uint32 GoodRandom() {
    return xrtRand32Ex(&g_rng);  // Different result each call
}

int main() {
    xrtInit();
    Setup();
    
    // Demonstrate wrong usage
    printf("Wrong usage (same result):\n");
    for (int i = 0; i < 3; i++) {
        printf("  %u\n", BadRandom());
    }
    
    // Demonstrate correct usage
    printf("\nCorrect usage (different each time):\n");
    for (int i = 0; i < 3; i++) {
        printf("  %u\n", GoodRandom());
    }
    
    xrtUnit();
    return 0;
}
```

---

## Algorithm Quality

### PCG Random Number Generator

XRT uses **PCG (Permuted Congruential Generator)** algorithm:

**Advantages:**
- ✅ High quality randomness
- ✅ Fast (faster than standard `rand()`)
- ✅ Long period (PCG32: 2^64, PCG64: 2^128)
- ✅ Passes multiple randomness tests (TestU01)

**Comparison:**
- Higher quality than `rand()`
- Faster than `MT19937`
- Suitable for games, simulations, testing, etc.

---

## Common Errors

### 1. Forgetting to Initialize

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    // ✗ Wrong: not initialized
    // int r = xrtRand32();  // Results may not be ideal
    
    // ✓ Correct: initialize first
    xrtInit();  // Initialize seed
    int r = xrtRand32();
    printf("Random number: %u\n", r);
    
    xrtUnit();
    return 0;
}
```

---

### 2. Range Misunderstanding

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // xrtRandRange includes both ends
    printf("Simulate dice roll (1-6):\n");
    for (int i = 0; i < 10; i++) {
        int r = xrtRandRange(1, 6);  // Can be 1,2,3,4,5,6
        printf("%d ", r);
    }
    printf("\n");
    
    // If need [1, 6) range (not including 6)
    printf("\nRange [1, 6) not including 6:\n");
    for (int i = 0; i < 10; i++) {
        int r = xrtRandRange(1, 5);  // 1,2,3,4,5
        printf("%d ", r);
    }
    printf("\n");
    
    xrtUnit();
    return 0;
}
```

---

## Related Documentation

- [Base Basic Function Library](api-base.en.md) - Library initialization
- [String String Processing](api-string.en.md) - Random string generation
- [Time Time Processing](api-time.en.md) - Time seed
- [Main Index](README.en.md) - Return to documentation home

---

<div align="center">

[Back to Top](#math---mathematics-library)

</div>
