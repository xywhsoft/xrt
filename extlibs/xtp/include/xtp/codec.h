#ifndef XTP_CODEC_H
#define XTP_CODEC_H
#include <xtp/features.h>
#include <xtp/export.h>
#include <xtp/types.h>
#if defined(XTP_FEATURE_XTP_CODEC)
typedef struct xtpcodecconfig {
    size_t MaxFrameBytes, MaxMetadataBytes, MaxCommandBytes, MaxParams, MaxKeyBytes, MaxBatch;
} xtpcodecconfig;
XRT_EXTERN_C_BEGIN
/* Defaults: 1MiB frame,16KiB metadata,256 command bytes,64 params,64 key bytes,
 * 64 batch items. Non-NULL config fields are literal limits; zero is not unlimited.
 * Frame>=32 and <=UINT32_MAX; params<=256,batch in1..64,wire field widths apply. */
XTP_API void xtpCodecConfigInit(xtpcodecconfig* Config);
/* Synchronous, zero allocation; NULL Config selects defaults. Inputs must stay
 * stable/readable. Required/Written and written bytes must not overlap sources,
 * descriptors, config, or each other. Every failure preserves all outputs.
 * Encode preflights the ENTIRE batch before touching output; table order stays
 * unchanged. No implicit NUL, value conversion, hashing of Body, or sorting fields. */
XTP_API xtpresult xtpEncodeSize(const xtppacket* Packets,size_t Count,
    const xtpcodecconfig* Config,size_t* Required);
XTP_API xtpresult xtpEncode(const xtppacket* Packets,size_t Count,
    const xtpcodecconfig* Config,void* Output,size_t Capacity,size_t* Written);
/* Exactly one complete frame, borrowed until caller changes/frees Input.
 * No allocation/copy. Short input=TRUNCATED,extra bytes=LENGTH. Failure preserves
 * Frame; Frame/config/input must be disjoint. All fields validated before success. */
XTP_API xtpresult xtpDecode(xbytesview Input,const xtpcodecconfig* Config,xtpframeview* Frame);
/* Accept only live library-produced views. At supports BOTH array and table in
 * transmitted order,O(1),without comparing keys. Find is a linear table lookup;
 * array returns WRONG_MODE. FOUND may contain an empty value. Non-FOUND zeroes
 * a valid disjoint output; invalid/overlapping output preserves bytes. No rescans. */
XTP_API xtpparamresult xtpParamAt(const xtpparamsview* Params,size_t Index,xbytesview* Value);
XTP_API xtpparamresult xtpParamFind(const xtpparamsview* Params,xstrview Key,xbytesview* Value);
XTP_API xtpparamiter xtpParams(const xtpparamsview* Params);
XTP_API xtpparamresult xtpParamNext(xtpparamiter* Iter,xtpparamentry* Entry);
XRT_EXTERN_C_END
#endif
#endif
