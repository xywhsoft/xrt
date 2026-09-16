/* 由 gen_oidc_keys.py 生成，勿手改。OIDC 组合测试夹具。 */
#ifndef XOAUTH2_TEST_OIDC_KEYS_H
#define XOAUTH2_TEST_OIDC_KEYS_H

static const char OIDC_EC_PRIV[] =
	"-----BEGIN EC PRIVATE KEY-----\nMHcCAQEEIBtKZWI7YuEPlz2iPjb+RVStas3v/jaOMhXzBxIW2POxoAoGCCqGSM49\nAwEHoUQDQgAE8YcPLnP4UlpblrJAlSCAqt7ukR2QtqQgAtAHgOLYgamUkw/QLSj6\nIMcZJJ+ZdSQKJyk5dnqSA4l5jKZbdM7SrQ==\n-----END EC PRIVATE KEY-----\n";
static const char OIDC_EC_PUB[] =
	"-----BEGIN PUBLIC KEY-----\nMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE8YcPLnP4UlpblrJAlSCAqt7ukR2Q\ntqQgAtAHgOLYgamUkw/QLSj6IMcZJJ+ZdSQKJyk5dnqSA4l5jKZbdM7SrQ==\n-----END PUBLIC KEY-----\n";
static const char OIDC_JWKS[] =
	"{\"keys\": [{\"kty\": \"EC\", \"kid\": \"oidc-ec-1\", \"use\": \"sig\", \"crv\": \"P-256\", \"x\": \"8YcPLnP4UlpblrJAlSCAqt7ukR2QtqQgAtAHgOLYgak\", \"y\": \"lJMP0C0o-iDHGSSfmXUkCicpOXZ6kgOJeYymW3TO0q0\"}]}";

#endif
