/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XREGEX_FEATURES_H
#define XREGEX_FEATURES_H

/* xregex 及其直接依赖。 */
#if defined(XREGEX_MODULE_ALL) || defined(XREGEX_MODULE_XREGEX)
#ifndef XREGEX_FEATURE_REGEX
#define XREGEX_FEATURE_REGEX
#endif
#ifndef XREGEX_MODULE_REGEX_REPLACE
#define XREGEX_MODULE_REGEX_REPLACE
#endif
#ifndef XREGEX_MODULE_REGEX_SPLIT
#define XREGEX_MODULE_REGEX_SPLIT
#endif
#ifndef XREGEX_MODULE_REGEX_SET
#define XREGEX_MODULE_REGEX_SET
#endif
#endif

/* regex_set 及其直接依赖。 */
#if defined(XREGEX_MODULE_ALL) || defined(XREGEX_MODULE_REGEX_SET)
#ifndef XREGEX_FEATURE_REGEX_SET
#define XREGEX_FEATURE_REGEX_SET
#endif
#ifndef XREGEX_MODULE_REGEX_CORE
#define XREGEX_MODULE_REGEX_CORE
#endif
#endif

/* regex_split 及其直接依赖。 */
#if defined(XREGEX_MODULE_ALL) || defined(XREGEX_MODULE_REGEX_SPLIT)
#ifndef XREGEX_FEATURE_REGEX_SPLIT
#define XREGEX_FEATURE_REGEX_SPLIT
#endif
#ifndef XREGEX_MODULE_REGEX_MATCH
#define XREGEX_MODULE_REGEX_MATCH
#endif
#ifndef XRT_MODULE_STRING_SPLIT
#define XRT_MODULE_STRING_SPLIT
#endif
#endif

/* regex_replace 及其直接依赖。 */
#if defined(XREGEX_MODULE_ALL) || defined(XREGEX_MODULE_REGEX_REPLACE)
#ifndef XREGEX_FEATURE_REGEX_REPLACE
#define XREGEX_FEATURE_REGEX_REPLACE
#endif
#ifndef XREGEX_MODULE_REGEX_MATCH
#define XREGEX_MODULE_REGEX_MATCH
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#endif

/* regex_match 及其直接依赖。 */
#if defined(XREGEX_MODULE_ALL) || defined(XREGEX_MODULE_REGEX_MATCH)
#ifndef XREGEX_FEATURE_REGEX_MATCH
#define XREGEX_FEATURE_REGEX_MATCH
#endif
#ifndef XREGEX_MODULE_REGEX_CORE
#define XREGEX_MODULE_REGEX_CORE
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#endif

/* regex_core 及其直接依赖。 */
#if defined(XREGEX_MODULE_ALL) || defined(XREGEX_MODULE_REGEX_CORE)
#ifndef XREGEX_FEATURE_REGEX_CORE
#define XREGEX_FEATURE_REGEX_CORE
#endif
#endif

#endif /* XREGEX_FEATURES_H */
