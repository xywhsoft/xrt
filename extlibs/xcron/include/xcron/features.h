/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XCRON_FEATURES_H
#define XCRON_FEATURES_H

/* xcron 及其直接依赖。 */
#if defined(XCRON_MODULE_ALL) || defined(XCRON_MODULE_XCRON)
#ifndef XCRON_FEATURE_XCRON
#define XCRON_FEATURE_XCRON
#endif
#ifndef XCRON_MODULE_XCRON_EXPRESSION
#define XCRON_MODULE_XCRON_EXPRESSION
#endif
#endif

/* xcron_expression 及其直接依赖。 */
#if defined(XCRON_MODULE_ALL) || defined(XCRON_MODULE_XCRON_EXPRESSION)
#ifndef XCRON_FEATURE_XCRON_EXPRESSION
#define XCRON_FEATURE_XCRON_EXPRESSION
#endif
#endif

#endif /* XCRON_FEATURES_H */
