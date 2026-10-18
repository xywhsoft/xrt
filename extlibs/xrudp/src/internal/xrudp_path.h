#ifndef XRUDP_INTERNAL_PATH_H
#define XRUDP_INTERNAL_PATH_H
#include <xrudp/session.h>
typedef struct xrudp__pathecho {
	xrudpaddress Address;
	unsigned char Token[16];
	bool Pending;
} xrudp__pathecho;
typedef struct xrudp__path {
	xrudppathstate State;
	xrudppathsendproc Send;
	xrudp__pathecho Echo[2];
	unsigned char Token[16];
	uint64_t CandidateNumber, ActiveNumber, UnvalidatedNumber, ResponseAt, InspectSecond;
	uint32_t Inspections, ControlInspections, InitialAllowance;
	bool Trusted;
} xrudp__path;
bool xrudp__path_address(const xrudpaddress* address);
bool xrudp__path_equal(const xrudpaddress* a, const xrudpaddress* b);
void xrudp__path_clock(xrudp__path* path, uint64_t now);
void xrudp__path_fail(xrudp__path* path, uint64_t at, xrudpresult reason);
xrudpresult xrudp__path_start(xrudp__path* path, const xrudpaddress* address, uint64_t now, bool local);
bool xrudp__path_inspect(xrudp__path* path, const xrudpaddress* address, bool control, uint64_t now);
void xrudp__path_credit(xrudp__path* path, uint64_t number, uint32_t size);
void xrudp__path_echo(xrudp__path* path, const xrudpaddress* address, const void* token);
void xrudp__path_promote(xrudp__path* path, uint64_t largest);
void xrudp__path_clear(xrudp__path* path);
uint64_t xrudp__path_due(const xrudp__path* path, uint64_t now);
#endif
