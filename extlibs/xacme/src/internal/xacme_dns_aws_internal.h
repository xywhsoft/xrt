#ifndef XACME_DNS_AWS_INTERNAL_H
#define XACME_DNS_AWS_INTERNAL_H

#include <xrt/buffer.h>

#define XACME_AWS_TXT_MAX_VALUES 400u

/* Values 借用 ListResourceRecordSets 响应体，保留原 XML 转义文本。 */
typedef struct xacmeawstxtset {
	bool bPresent;
	char sTtl[24];
	xstrview Values[XACME_AWS_TXT_MAX_VALUES];
	size_t iCount;
} xacmeawstxtset;

bool xacmeAwsParseTxtSet(cstr sXml, cstr sFqdn, xacmeawstxtset* pSet);
/* Only a complete Route53 change acknowledgment proves accepted submission. */
bool xacmeAwsChangeAccepted(cstr sXml);
/* Validate the complete bounded REST-XML error envelope; reset output on failure. */
bool xacmeAwsErrorCode(xstrview Xml, char sCode[64]);
bool xacmeAwsBuildTxtChange(const xacmeawstxtset* pOld,
	cstr sFqdn, cstr sValue, bool bAdd, xbuffer* pOut, bool* pChanged);
/* SigV4 Route53 密钥链；终止字符串不包含 C 字符串的结尾 NUL。 */
bool xacmeAwsSigningKey(cstr sSecret, cstr sDate, cstr sRegion,
	uint8 pKey[32]);
/* 1=唯一公有 zone，0=不存在，-1=畸形或同名公有 zone 歧义。 */
int xacmeAwsSelectPublicZone(cstr sXml, cstr sZone,
	char* sOutId, size_t iIdCap);

#endif
