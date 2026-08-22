#ifndef XRT_EXAMPLE_WAIT_H
#define XRT_EXAMPLE_WAIT_H

static bool exWaitLongMin(volatile long* pValue, long iExpectMin, uint32 iTimeoutMs)
{
	uint32 iLoops;

	if ( !pValue ) return false;

	iLoops = (iTimeoutMs / 10u) + 1u;
	while ( iLoops-- > 0u ) {
		if ( *pValue >= iExpectMin ) return true;
		xrtSleep(10u);
	}

	return *pValue >= iExpectMin;
}

#endif
