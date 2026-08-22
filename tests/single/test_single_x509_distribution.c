#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#include "../fixtures/x509_distribution_vectors.h"



/* 验证单头文件包含独立分发点与列表游标 API。 */
int main(void)
{
	xx509distributionpoint Point;
	xx509distributioncursor Cursor;

	if ( !xrtX509DistributionPointParse(
		(xbytesview) {
			X509_DISTRIBUTION_POINT, sizeof(X509_DISTRIBUTION_POINT)
		}, &Point
	) || !Point.HasName || !Point.HasReasons || !xrtX509DistributionInit(
		(xbytesview) {
			X509_DISTRIBUTION_POINTS, sizeof(X509_DISTRIBUTION_POINTS)
		}, &Cursor
	) || (xrtX509DistributionRead(&Cursor, &Point) != X509_VALUE) ) {
		return 1;
	}
	return 0;
}
