#ifndef XCRON_TZDB_FILE_H
#define XCRON_TZDB_FILE_H
#include <xcron/tzdb.h>
#if defined(XCRON_FEATURE_XCRON_TZDB_FILE) && (!defined(XCRON_FEATURE_XCRON_TZDB) || !defined(XRT_FEATURE_FILE_ROOT))
#error "cron tzdb file requires tzdb and file-root"
#endif
#if defined(XCRON_FEATURE_XCRON_TZDB_FILE)
#include <xrt/file.h>
XRT_EXTERN_C_BEGIN
/* Explicit manifest of names under a borrowed anchored XRT root. Uses secure
 * RootFileOpen (including contained relative aliases), regular files only and
 * one <=16MiB temporary file at a time inside the SAME aggregate database quota.
 * Version/names/CAS/output rules are Load's. Caller owns Root for the full call,
 * and supplies a trusted immutable release directory/manifest: publication is
 * atomic, but reads across several files are not a filesystem-wide transaction.
 * File size/identity/time are rechecked after reading, and growth/short reads
 * fail; these checks alone cannot prove an immutable source or authenticate it.
 * No default directory, TZ environment, download, directory walk or leap-scale
 * fallback is guessed. Missing/invalid/escaping files reject the entire reload.
 * Root resolution/open/read/stat/close allocations/syscalls are cold costs.
 * Root itself and its native directory handle are caller-owned, outside quota. */
XCRON_API bool xcronTzdbLoadRoot(xcrontzdb* Database,xroot Root,xstrview Version,const xstrview* Names,size_t Count,
    uint64 ExpectedRevision,uint64* Revision,xcrontzdbissue* Issue);
XRT_EXTERN_C_END
#endif
#endif
