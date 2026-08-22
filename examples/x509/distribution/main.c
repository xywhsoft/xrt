#include <stdio.h>

#include <xrt.h>



/* 展示零分配遍历证书分发点正文中的 URI。 */
int main(void)
{
	static const uint8 PointsDer[] = {
		0x30, 0x12, 0x30, 0x10, 0xA0, 0x0E, 0xA0, 0x0C, 0x86, 0x0A,
		0x68, 0x74, 0x74, 0x70, 0x3A, 0x2F, 0x2F, 0x63, 0x72, 0x6C
	};
	xx509distributioncursor Points;
	xx509distributionpoint Point;
	xx509genname Name;

	if ( !xrtX509DistributionInit(
		(xbytesview) { PointsDer, sizeof(PointsDer) }, &Points
	) || (xrtX509DistributionRead(&Points, &Point) != X509_VALUE) ||
		!Point.HasName ||
		(xrtX509GeneralNameRead(
			&Point.Name.FullNames, &Name
		) != X509_VALUE) || (Name.Type != X509_NAME_URI) ) {
		return 1;
	}
	printf("CRL URI: %.*s\n", (int)Name.Value.Size, (cstr)Name.Value.Data);
	return 0;
}
