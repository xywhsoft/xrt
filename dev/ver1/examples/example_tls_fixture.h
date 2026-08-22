#ifndef XRT_EXAMPLE_TLS_FIXTURE_H
#define XRT_EXAMPLE_TLS_FIXTURE_H

#include "example_common.h"

typedef struct {
	str sCertPath;
	str sKeyPath;
} extlsfixture;

static void exTlsFixtureUnit(extlsfixture* pFixture);


static const char ex_sTlsCertPem[] =
	"-----BEGIN CERTIFICATE-----\n"
	"MIIDaDCCAlCgAwIBAgIUYoO37Gjm4QLwJTKIcZrwMvNGtZEwDQYJKoZIhvcNAQEL\n"
	"BQAwXjELMAkGA1UEBhMCQ04xEjAQBgNVBAgMCUd1YW5nZG9uZzERMA8GA1UEBwwI\n"
	"U2hlbnpoZW4xFDASBgNVBAoMC1hSVCBFeGFtcGxlMRIwEAYDVQQDDAlsb2NhbGhv\n"
	"c3QwHhcNMjYwNDAxMDQzNTQ3WhcNMzYwMzMwMDQzNTQ3WjBeMQswCQYDVQQGEwJD\n"
	"TjESMBAGA1UECAwJR3Vhbmdkb25nMREwDwYDVQQHDAhTaGVuemhlbjEUMBIGA1UE\n"
	"CgwLWFJUIEV4YW1wbGUxEjAQBgNVBAMMCWxvY2FsaG9zdDCCASIwDQYJKoZIhvcN\n"
	"AQEBBQADggEPADCCAQoCggEBAIqyvwP7c3EjckZEXJv+yJ7ngp4p34am4UMhsclC\n"
	"hkrtWvEN3itxTO4Lf+70td4mAO1nHJYYpLvk0LT82MWycEcU3FG8f7hwLkDYZ1GQ\n"
	"1YpQ7RHf0qz0Aq/n8DPn+xI63+t5pV1HWeqZfFYZ3a2LWpNm+rZtqdRvPdnLxOVC\n"
	"cIMcyJSypDDzOODIaK2tGTclGDEfjiqa1KvhKVWJ2MRRCta4NjzA6MEScYxcdNlA\n"
	"YTYUiSVbHk8cPzTFk1hloQKSOtTDtQMrrrJLm+vbJjVusnjrgLA9QLmNK9/7e3/T\n"
	"EbR0A5bXGrYps7Gg4b0MpbuxpL2Ak8DmELndyWglQo3k/AUCAwEAAaMeMBwwGgYD\n"
	"VR0RBBMwEYIJbG9jYWxob3N0hwR/AAABMA0GCSqGSIb3DQEBCwUAA4IBAQARMzy6\n"
	"lnRnVK5uyKVazYLQsw9RATd2Bz2YBVqLd3XFm2zRJ9x20+4DLIvv/2t4Ih/0+XNV\n"
	"OIZvGt/NGJpmBG3OYZ8qaUn2fIO6Obt9thv7lLIDsAhyk9afRYJVCItvpMHb/6zc\n"
	"s8ZkK2ksivSWzb3VXulNbTMCMMvUKtQq6QgcWqwFVJwYJOaDLhCWtvmlgb/y0nVX\n"
	"0Q48OBgAQdohuyHreMVA/j4Xjz+aA7aBgHA4FdFnDdcnBM0ToMxrRnZDRpmzIR8K\n"
	"PQEG3xxC1o6pO8RBjE0/nlvLprj4LwcOd61mH/FmS3oa9vMryYwGYX7w1zxsrcIm\n"
	"nUGCJs1VSHG40wSH\n"
	"-----END CERTIFICATE-----\n";

static const char ex_sTlsKeyPem[] =
	"-----BEGIN RSA PRIVATE KEY-----\n"
	"MIIEogIBAAKCAQEAirK/A/tzcSNyRkRcm/7InueCninfhqbhQyGxyUKGSu1a8Q3e\n"
	"K3FM7gt/7vS13iYA7Wcclhiku+TQtPzYxbJwRxTcUbx/uHAuQNhnUZDVilDtEd/S\n"
	"rPQCr+fwM+f7Ejrf63mlXUdZ6pl8VhndrYtak2b6tm2p1G892cvE5UJwgxzIlLKk\n"
	"MPM44Mhora0ZNyUYMR+OKprUq+EpVYnYxFEK1rg2PMDowRJxjFx02UBhNhSJJVse\n"
	"Txw/NMWTWGWhApI61MO1Ayuuskub69smNW6yeOuAsD1AuY0r3/t7f9MRtHQDltca\n"
	"timzsaDhvQylu7GkvYCTwOYQud3JaCVCjeT8BQIDAQABAoIBAAFrOC8ufpJo94A2\n"
	"rqWC4E2t1m/PQ66Gh6s5OEQei9ikNR1eVAg+PGL6rPhGZROMxc7Skp/fcyn1OoRW\n"
	"H3066NFKqkFPdsADLRnz4hoFwQV38/Y47lhfa6VIhQlxj/zdANGRbkALcjpHOoF3\n"
	"+hpOdg1SzPGaTWszUx7mqdDi59s+J4fMLGZf72xxPdAfmcAFIBq1mPDvnmNjcEOG\n"
	"ydF2nCyOnvXPJgATmH1Ecm5y3+WBDoXNUI1DIibK4idxDzmHA+h0zb4EhtmEhOM4\n"
	"dhGuMra+TJWBSJ8EN0BrGoWmFmJ93QtJl69LBJV/QOOrZDb1qDqaojVqYzhhTubP\n"
	"oVMbVRkCgYEAvoVRpNlj7xHJwSAs7/c4HEMVj8ql2joUweuKjtOYSdJxMnEFGsuA\n"
	"/3Ywgi7Xx7mc6f8OQyhV/L1UNCsBNagbwPYj2knfSolTmDklP3+boUJQVPhDkw0Y\n"
	"pDp97xNwrLXDR86De9neKT8ojPXEU32RR+d8bd/4m8oNJJwJf6xFKS0CgYEAul3m\n"
	"H/CuhKDzStiZcYhl2z/GGAKB8uZ07vwhlmfSw07A0awweC3rF5OJ93qHRT0ZAfGy\n"
	"1aYdc0bGcyHa67/xnb4mWYDXNptyf3Q1TCIi+nOqalJbk0ZfPBV98//1zvXga2ie\n"
	"2yQyYrJtvw7mW1YzQYeAq+PRf3Z4fMjZvpcqtTkCgYA0ub6biZIPgnO8X8Qv8NH1\n"
	"eFdKQQHfP/2ooR/qYQKfQ38SP5bzEGi1yiaokIAlBOg5Fd4DlfEeDeN0wIYILGrp\n"
	"3vSTH6iM/y5ETWRSi2UtnqWOrlo9Iv2zzYA2nsGq+m59u9hFeUjzT0hQol9f37tK\n"
	"E/UqjzZFHwi+HfS/AZTuTQKBgAuSKuiOw/ceGxzph9Vht5k+Q2lYNoNDRb1U0C0L\n"
	"cy2HJTefbj738uG62lUQOXfWDEhvnj/fmXJ/0XByiKocd77ogG8MLdCJJDm/mFOK\n"
	"xwsvxUPmqyLguqb7Wp+co8FeyLlCfKJ0g+BW3bOAFFNVbcdCx31knqxAScjNm59W\n"
	"uWMZAoGAKvrkef9oYB5AAMSQbGIYvIm+vv57T8tZS+tog0qaU8QPYPXNqJ3MFL46\n"
	"zQZCvDFm+8mkLiOhzmJIme4NRrjywITkXEIPg9YSe792dtxgvHE78XPOlZO8uE+W\n"
	"aeku7Q3d+O5bw7M7R6Cq99hQ1j3RlkMKsNE766qAkAXoJBaB89E=\n"
	"-----END RSA PRIVATE KEY-----\n";


static bool exTlsFixtureInit(extlsfixture* pFixture, const char* sStem)
{
	char sCertFile[128];
	char sKeyFile[128];

	if ( !pFixture ) return false;
	memset(pFixture, 0, sizeof(*pFixture));
	memset(sCertFile, 0, sizeof(sCertFile));
	memset(sKeyFile, 0, sizeof(sKeyFile));

	snprintf(sCertFile, sizeof(sCertFile), "%s_cert.pem", sStem ? sStem : "xrt_example_tls");
	snprintf(sKeyFile, sizeof(sKeyFile), "%s_key.pem", sStem ? sStem : "xrt_example_tls");

	pFixture->sCertPath = exMakeAppFilePath(sCertFile);
	pFixture->sKeyPath = exMakeAppFilePath(sKeyFile);
	if ( !pFixture->sCertPath || !pFixture->sKeyPath ) {
		exTlsFixtureUnit(pFixture);
		return false;
	}
	if ( !xrtFileWriteAll(pFixture->sCertPath, (str)ex_sTlsCertPem, strlen(ex_sTlsCertPem), XRT_CP_UTF8) ) {
		exTlsFixtureUnit(pFixture);
		return false;
	}
	if ( !xrtFileWriteAll(pFixture->sKeyPath, (str)ex_sTlsKeyPem, strlen(ex_sTlsKeyPem), XRT_CP_UTF8) ) {
		exTlsFixtureUnit(pFixture);
		return false;
	}
	return true;
}


static void exTlsFixtureUnit(extlsfixture* pFixture)
{
	if ( !pFixture ) return;
	if ( pFixture->sCertPath ) {
		xrtFileDelete(pFixture->sCertPath);
		xrtFree(pFixture->sCertPath);
		pFixture->sCertPath = NULL;
	}
	if ( pFixture->sKeyPath ) {
		xrtFileDelete(pFixture->sKeyPath);
		xrtFree(pFixture->sKeyPath);
		pFixture->sKeyPath = NULL;
	}
}

#endif
