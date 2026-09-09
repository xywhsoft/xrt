#ifndef XLLM_EMBED_ONNXRUNTIME_COMPAT_H
#define XLLM_EMBED_ONNXRUNTIME_COMPAT_H

/* Avoid SAL macro redefinition warnings when xrt/windows headers are already in scope. */
#if defined(_WIN32)
#if defined(_In_)
#pragma push_macro("_In_")
#undef _In_
#define XLLM__ORT_RESTORE_IN 1
#endif
#if defined(_In_z_)
#pragma push_macro("_In_z_")
#undef _In_z_
#define XLLM__ORT_RESTORE_IN_Z 1
#endif
#if defined(_In_opt_)
#pragma push_macro("_In_opt_")
#undef _In_opt_
#define XLLM__ORT_RESTORE_IN_OPT 1
#endif
#if defined(_In_opt_z_)
#pragma push_macro("_In_opt_z_")
#undef _In_opt_z_
#define XLLM__ORT_RESTORE_IN_OPT_Z 1
#endif
#if defined(_Out_)
#pragma push_macro("_Out_")
#undef _Out_
#define XLLM__ORT_RESTORE_OUT 1
#endif
#if defined(_Out_opt_)
#pragma push_macro("_Out_opt_")
#undef _Out_opt_
#define XLLM__ORT_RESTORE_OUT_OPT 1
#endif
#if defined(_Outptr_)
#pragma push_macro("_Outptr_")
#undef _Outptr_
#define XLLM__ORT_RESTORE_OUTPTR 1
#endif
#if defined(_Outptr_opt_)
#pragma push_macro("_Outptr_opt_")
#undef _Outptr_opt_
#define XLLM__ORT_RESTORE_OUTPTR_OPT 1
#endif
#if defined(_Inout_)
#pragma push_macro("_Inout_")
#undef _Inout_
#define XLLM__ORT_RESTORE_INOUT 1
#endif
#if defined(_Inout_opt_)
#pragma push_macro("_Inout_opt_")
#undef _Inout_opt_
#define XLLM__ORT_RESTORE_INOUT_OPT 1
#endif
#if defined(_Frees_ptr_opt_)
#pragma push_macro("_Frees_ptr_opt_")
#undef _Frees_ptr_opt_
#define XLLM__ORT_RESTORE_FREES_PTR_OPT 1
#endif
#if defined(_Ret_maybenull_)
#pragma push_macro("_Ret_maybenull_")
#undef _Ret_maybenull_
#define XLLM__ORT_RESTORE_RET_MAYBENULL 1
#endif
#if defined(_Ret_notnull_)
#pragma push_macro("_Ret_notnull_")
#undef _Ret_notnull_
#define XLLM__ORT_RESTORE_RET_NOTNULL 1
#endif
#if defined(_Check_return_)
#pragma push_macro("_Check_return_")
#undef _Check_return_
#define XLLM__ORT_RESTORE_CHECK_RETURN 1
#endif
#if defined(_Outptr_result_maybenull_)
#pragma push_macro("_Outptr_result_maybenull_")
#undef _Outptr_result_maybenull_
#define XLLM__ORT_RESTORE_OUTPTR_RESULT_MAYBENULL 1
#endif
#if defined(_Outptr_result_maybenull_z_)
#pragma push_macro("_Outptr_result_maybenull_z_")
#undef _Outptr_result_maybenull_z_
#define XLLM__ORT_RESTORE_OUTPTR_RESULT_MAYBENULL_Z 1
#endif
#if defined(_In_reads_)
#pragma push_macro("_In_reads_")
#undef _In_reads_
#define XLLM__ORT_RESTORE_IN_READS 1
#endif
#if defined(_In_reads_opt_)
#pragma push_macro("_In_reads_opt_")
#undef _In_reads_opt_
#define XLLM__ORT_RESTORE_IN_READS_OPT 1
#endif
#if defined(_Inout_updates_)
#pragma push_macro("_Inout_updates_")
#undef _Inout_updates_
#define XLLM__ORT_RESTORE_INOUT_UPDATES 1
#endif
#if defined(_Out_writes_)
#pragma push_macro("_Out_writes_")
#undef _Out_writes_
#define XLLM__ORT_RESTORE_OUT_WRITES 1
#endif
#if defined(_Out_writes_opt_)
#pragma push_macro("_Out_writes_opt_")
#undef _Out_writes_opt_
#define XLLM__ORT_RESTORE_OUT_WRITES_OPT 1
#endif
#if defined(_Inout_updates_all_)
#pragma push_macro("_Inout_updates_all_")
#undef _Inout_updates_all_
#define XLLM__ORT_RESTORE_INOUT_UPDATES_ALL 1
#endif
#if defined(_Out_writes_bytes_all_)
#pragma push_macro("_Out_writes_bytes_all_")
#undef _Out_writes_bytes_all_
#define XLLM__ORT_RESTORE_OUT_WRITES_BYTES_ALL 1
#endif
#if defined(_Out_writes_all_)
#pragma push_macro("_Out_writes_all_")
#undef _Out_writes_all_
#define XLLM__ORT_RESTORE_OUT_WRITES_ALL 1
#endif
#if defined(_Success_)
#pragma push_macro("_Success_")
#undef _Success_
#define XLLM__ORT_RESTORE_SUCCESS 1
#endif
#if defined(_Outptr_result_buffer_maybenull_)
#pragma push_macro("_Outptr_result_buffer_maybenull_")
#undef _Outptr_result_buffer_maybenull_
#define XLLM__ORT_RESTORE_OUTPTR_RESULT_BUFFER_MAYBENULL 1
#endif
#endif

#include "../../lib/onnxruntime/onnxruntime_c_api.h"

#if defined(_WIN32)
#if defined(XLLM__ORT_RESTORE_OUTPTR_RESULT_BUFFER_MAYBENULL)
#pragma pop_macro("_Outptr_result_buffer_maybenull_")
#undef XLLM__ORT_RESTORE_OUTPTR_RESULT_BUFFER_MAYBENULL
#endif
#if defined(XLLM__ORT_RESTORE_SUCCESS)
#pragma pop_macro("_Success_")
#undef XLLM__ORT_RESTORE_SUCCESS
#endif
#if defined(XLLM__ORT_RESTORE_OUT_WRITES_ALL)
#pragma pop_macro("_Out_writes_all_")
#undef XLLM__ORT_RESTORE_OUT_WRITES_ALL
#endif
#if defined(XLLM__ORT_RESTORE_OUT_WRITES_BYTES_ALL)
#pragma pop_macro("_Out_writes_bytes_all_")
#undef XLLM__ORT_RESTORE_OUT_WRITES_BYTES_ALL
#endif
#if defined(XLLM__ORT_RESTORE_INOUT_UPDATES_ALL)
#pragma pop_macro("_Inout_updates_all_")
#undef XLLM__ORT_RESTORE_INOUT_UPDATES_ALL
#endif
#if defined(XLLM__ORT_RESTORE_OUT_WRITES_OPT)
#pragma pop_macro("_Out_writes_opt_")
#undef XLLM__ORT_RESTORE_OUT_WRITES_OPT
#endif
#if defined(XLLM__ORT_RESTORE_OUT_WRITES)
#pragma pop_macro("_Out_writes_")
#undef XLLM__ORT_RESTORE_OUT_WRITES
#endif
#if defined(XLLM__ORT_RESTORE_INOUT_UPDATES)
#pragma pop_macro("_Inout_updates_")
#undef XLLM__ORT_RESTORE_INOUT_UPDATES
#endif
#if defined(XLLM__ORT_RESTORE_IN_READS_OPT)
#pragma pop_macro("_In_reads_opt_")
#undef XLLM__ORT_RESTORE_IN_READS_OPT
#endif
#if defined(XLLM__ORT_RESTORE_IN_READS)
#pragma pop_macro("_In_reads_")
#undef XLLM__ORT_RESTORE_IN_READS
#endif
#if defined(XLLM__ORT_RESTORE_OUTPTR_RESULT_MAYBENULL_Z)
#pragma pop_macro("_Outptr_result_maybenull_z_")
#undef XLLM__ORT_RESTORE_OUTPTR_RESULT_MAYBENULL_Z
#endif
#if defined(XLLM__ORT_RESTORE_OUTPTR_RESULT_MAYBENULL)
#pragma pop_macro("_Outptr_result_maybenull_")
#undef XLLM__ORT_RESTORE_OUTPTR_RESULT_MAYBENULL
#endif
#if defined(XLLM__ORT_RESTORE_CHECK_RETURN)
#pragma pop_macro("_Check_return_")
#undef XLLM__ORT_RESTORE_CHECK_RETURN
#endif
#if defined(XLLM__ORT_RESTORE_RET_NOTNULL)
#pragma pop_macro("_Ret_notnull_")
#undef XLLM__ORT_RESTORE_RET_NOTNULL
#endif
#if defined(XLLM__ORT_RESTORE_RET_MAYBENULL)
#pragma pop_macro("_Ret_maybenull_")
#undef XLLM__ORT_RESTORE_RET_MAYBENULL
#endif
#if defined(XLLM__ORT_RESTORE_FREES_PTR_OPT)
#pragma pop_macro("_Frees_ptr_opt_")
#undef XLLM__ORT_RESTORE_FREES_PTR_OPT
#endif
#if defined(XLLM__ORT_RESTORE_INOUT_OPT)
#pragma pop_macro("_Inout_opt_")
#undef XLLM__ORT_RESTORE_INOUT_OPT
#endif
#if defined(XLLM__ORT_RESTORE_INOUT)
#pragma pop_macro("_Inout_")
#undef XLLM__ORT_RESTORE_INOUT
#endif
#if defined(XLLM__ORT_RESTORE_OUTPTR_OPT)
#pragma pop_macro("_Outptr_opt_")
#undef XLLM__ORT_RESTORE_OUTPTR_OPT
#endif
#if defined(XLLM__ORT_RESTORE_OUTPTR)
#pragma pop_macro("_Outptr_")
#undef XLLM__ORT_RESTORE_OUTPTR
#endif
#if defined(XLLM__ORT_RESTORE_OUT_OPT)
#pragma pop_macro("_Out_opt_")
#undef XLLM__ORT_RESTORE_OUT_OPT
#endif
#if defined(XLLM__ORT_RESTORE_OUT)
#pragma pop_macro("_Out_")
#undef XLLM__ORT_RESTORE_OUT
#endif
#if defined(XLLM__ORT_RESTORE_IN_OPT_Z)
#pragma pop_macro("_In_opt_z_")
#undef XLLM__ORT_RESTORE_IN_OPT_Z
#endif
#if defined(XLLM__ORT_RESTORE_IN_OPT)
#pragma pop_macro("_In_opt_")
#undef XLLM__ORT_RESTORE_IN_OPT
#endif
#if defined(XLLM__ORT_RESTORE_IN_Z)
#pragma pop_macro("_In_z_")
#undef XLLM__ORT_RESTORE_IN_Z
#endif
#if defined(XLLM__ORT_RESTORE_IN)
#pragma pop_macro("_In_")
#undef XLLM__ORT_RESTORE_IN
#endif
#endif

#endif
