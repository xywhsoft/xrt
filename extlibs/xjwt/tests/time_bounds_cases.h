/* RFC 7519 time boundaries and overflow-safe leeway arithmetic. */
#ifndef XJWT_TIME_BOUNDS_CASES_H
#define XJWT_TIME_BOUNDS_CASES_H
#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct timecase {
    const char* Name;
    const char* Claim;
    int64_t Date;
    int64_t Now;
    int Leeway;
    bool Accept;
    int Error;
} timecase;

static int jwt_time_case(const timecase* test, bool signed_token)
{
    int failed = 0;
    xvalue* claims = xrtValueObject();
    xvalue* verified = NULL;
    char* token = NULL;
    bool accepted = false;
    if ( claims == NULL || !xrtValueObjectSetNew(claims, xrtStrView(test->Claim),
            xrtValueInt(test->Date)) ) {
        fprintf(stderr, "fixture allocation failed\n");
        failed = 1;
        goto done;
    }
    xjwtcheck check;
    xjwtCheckInit(&check);
    check.NowOverride = test->Now;
    check.ClockLeeway = test->Leeway;
    if ( signed_token ) {
        token = xjwtHs256(claims, "public-time-contract-fixture", 0);
        if ( token == NULL ) { failed = 1; goto done; }
        xrtClearError();
        verified = xjwtVerify(token, "public-time-contract-fixture", &check);
        accepted = verified != NULL;
    } else {
        xrtClearError();
        accepted = xjwtClaimsValid(claims, &check);
    }
    if ( accepted != test->Accept || (!accepted && xjwtLastError() != test->Error) ) {
        fprintf(stderr, "[FAIL] %s %s accepted=%d error=%d\n",
            signed_token ? "signed" : "claims", test->Name, (int)accepted, xjwtLastError());
        failed = 1;
    } else {
        printf("[PASS] %s %s\n", signed_token ? "signed" : "claims", test->Name);
    }
done:
    xrtValueRelease(verified); xrtFree(token); xrtValueRelease(claims);
    xrtClearError();
    return failed;
}

static int jwt_time_bounds_run(bool overflow_only)
{
    const timecase tests[] = {
        {"exp before deadline", "exp", 101, 100, 0, true, 0},
        {"exp exact deadline", "exp", 100, 100, 0, false, XJWT_ERROR_EXPIRED},
        {"exp past deadline", "exp", 99, 100, 0, false, XJWT_ERROR_EXPIRED},
        {"exp within leeway", "exp", 91, 100, 10, true, 0},
        {"exp exact leeway deadline", "exp", 90, 100, 10, false, XJWT_ERROR_EXPIRED},
        {"nbf exact activation", "nbf", 100, 100, 0, true, 0},
        {"nbf future activation", "nbf", 101, 100, 0, false, XJWT_ERROR_NOT_YET},
        {"nbf exact leeway activation", "nbf", 110, 100, 10, true, 0},
        {"nbf beyond leeway activation", "nbf", 111, 100, 10, false, XJWT_ERROR_NOT_YET},
        {"nbf maximum clock with leeway", "nbf", INT64_MAX, INT64_MAX, INT_MAX, true, 0},
        {"exp minimum clock with leeway", "exp", INT64_MIN, INT64_MIN, INT_MAX, true, 0},
        {"exp minimum exact deadline", "exp", INT64_MIN, INT64_MIN, 0, false, XJWT_ERROR_EXPIRED},
        {"nbf full signed range", "nbf", INT64_MAX, INT64_MIN, INT_MAX, false, XJWT_ERROR_NOT_YET},
        {"exp full signed range", "exp", INT64_MIN, INT64_MAX, INT_MAX, false, XJWT_ERROR_EXPIRED},
        {"negative epoch override", "exp", -99, -100, 0, true, 0},
        {"negative leeway rejected", "exp", INT64_MAX, INT64_MAX, -1, false, XJWT_ERROR_ARGUMENT},
        {"minimum leeway rejected", "nbf", INT64_MAX, INT64_MAX, INT_MIN, false, XJWT_ERROR_ARGUMENT},
    };
    int failures = 0;
    if ( overflow_only )
        return jwt_time_case(&tests[9], false);
    for ( size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++ ) {
        failures += jwt_time_case(&tests[i], false);
        failures += jwt_time_case(&tests[i], true);
    }
    return failures != 0;
}
#endif
