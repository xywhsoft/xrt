/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XMQTT_FEATURES_H
#define XMQTT_FEATURES_H

/* xmqtt 及其直接依赖。 */
#if defined(XMQTT_MODULE_ALL) || defined(XMQTT_MODULE_XMQTT)
#ifndef XMQTT_FEATURE_XMQTT
#define XMQTT_FEATURE_XMQTT
#endif
#ifndef XMQTT_MODULE_XMQTT_WIRE
#define XMQTT_MODULE_XMQTT_WIRE
#endif
#ifndef XMQTT_MODULE_XMQTT_PROPERTIES
#define XMQTT_MODULE_XMQTT_PROPERTIES
#endif
#endif

/* xmqtt_properties 及其直接依赖。 */
#if defined(XMQTT_MODULE_ALL) || defined(XMQTT_MODULE_XMQTT_PROPERTIES)
#ifndef XMQTT_FEATURE_XMQTT_PROPERTIES
#define XMQTT_FEATURE_XMQTT_PROPERTIES
#endif
#ifndef XMQTT_MODULE_XMQTT_WIRE
#define XMQTT_MODULE_XMQTT_WIRE
#endif
#endif

/* xmqtt_wire 及其直接依赖。 */
#if defined(XMQTT_MODULE_ALL) || defined(XMQTT_MODULE_XMQTT_WIRE)
#ifndef XMQTT_FEATURE_XMQTT_WIRE
#define XMQTT_FEATURE_XMQTT_WIRE
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#endif

#endif /* XMQTT_FEATURES_H */
