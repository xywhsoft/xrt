"""Adversarial certificates for the independent ACME grant verifier."""

import datetime
import unittest

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.x509.oid import NameOID

from tools.test_acme_mock import verify_grant_pairing


class GrantVerifierTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.now = datetime.datetime.now(datetime.timezone.utc)
        cls.root_key = ec.generate_private_key(ec.SECP256R1())
        cls.root_name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, 'trusted CA')])
        cls.root = cls.certificate(cls.root_name, cls.root_key, cls.root_name, cls.root_key, True, 1)
        cls.leaf_key = ec.generate_private_key(ec.SECP256R1())
        cls.leaf_name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, 'test.example.com')])
        cls.leaf = cls.certificate(cls.leaf_name, cls.leaf_key, cls.root_name, cls.root_key, False)

    @classmethod
    def certificate(cls, subject, key, issuer, signer, ca, path_length=None, *, days=30,
                    future=False, allow_sign=True):
        start = cls.now + datetime.timedelta(days=1 if future else -1)
        end = cls.now + datetime.timedelta(days=days)
        if days < 0: start = cls.now - datetime.timedelta(days=30)
        builder = (x509.CertificateBuilder().subject_name(subject).issuer_name(issuer)
                   .public_key(key.public_key()).serial_number(x509.random_serial_number())
                   .not_valid_before(start).not_valid_after(end)
                   .add_extension(x509.BasicConstraints(ca=ca, path_length=path_length), True))
        if ca:
            builder = builder.add_extension(x509.KeyUsage(False, False, False, False, False,
                                                          allow_sign, allow_sign, None, None), True)
        else:
            builder = builder.add_extension(x509.SubjectAlternativeName([x509.DNSName('test.example.com')]), False)
        return builder.sign(signer, hashes.SHA256())

    @staticmethod
    def pem(cert):
        return cert.public_bytes(serialization.Encoding.PEM).decode()

    def verify(self, *certs, roots=None):
        key = self.leaf_key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                                         serialization.NoEncryption()).decode()
        verify_grant_pairing(''.join(self.pem(c) for c in certs), key, 'test.example.com',
                             *(self.pem(c) for c in (roots if roots is not None else [self.root])))

    def rejected(self, *certs, roots=None):
        with self.assertRaises((AssertionError, ValueError)):
            self.verify(*certs, roots=roots)

    def test_direct_chain_including_root(self): self.verify(self.leaf, self.root)
    def test_direct_chain_omitting_root(self): self.verify(self.leaf)

    def test_same_issuer_name_wrong_signing_key(self):
        rogue = ec.generate_private_key(ec.SECP256R1())
        leaf = self.certificate(self.leaf_name, self.leaf_key, self.root_name, rogue, False)
        self.rejected(leaf, self.root)

    def test_untrusted_root_with_same_name(self):
        rogue = ec.generate_private_key(ec.SECP256R1())
        root = self.certificate(self.root_name, rogue, self.root_name, rogue, True, 0)
        leaf = self.certificate(self.leaf_name, self.leaf_key, self.root_name, rogue, False)
        self.rejected(leaf, root)

    def test_disconnected_suffix(self):
        other_key = ec.generate_private_key(ec.SECP256R1())
        other_name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, 'other CA')])
        other = self.certificate(other_name, other_key, other_name, other_key, True, 0)
        self.rejected(self.leaf, other)

    def test_duplicate_certificate(self): self.rejected(self.leaf, self.root, self.root)
    def test_no_trust_anchor(self): self.rejected(self.leaf, self.root, roots=[])

    def test_expired_leaf(self):
        self.rejected(self.certificate(self.leaf_name, self.leaf_key, self.root_name, self.root_key, False, days=-1))

    def test_future_leaf(self):
        self.rejected(self.certificate(self.leaf_name, self.leaf_key, self.root_name, self.root_key, False, future=True))

    def test_non_ca_signer(self):
        root = self.certificate(self.root_name, self.root_key, self.root_name, self.root_key, False)
        self.rejected(self.leaf, root, roots=[root])

    def test_ca_without_signing_usage(self):
        root = self.certificate(self.root_name, self.root_key, self.root_name, self.root_key, True, 0, allow_sign=False)
        self.rejected(self.leaf, root, roots=[root])

    def intermediate(self):
        key = ec.generate_private_key(ec.SECP256R1())
        name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, 'intermediate CA')])
        cert = self.certificate(name, key, self.root_name, self.root_key, True, 0)
        leaf = self.certificate(self.leaf_name, self.leaf_key, name, key, False)
        return leaf, cert

    def test_intermediate_chain(self):
        leaf, issuer = self.intermediate(); self.verify(leaf, issuer, self.root)

    def test_intermediate_chain_omitting_root(self):
        leaf, issuer = self.intermediate(); self.verify(leaf, issuer)

    def test_wrong_intermediate_signer_same_name(self):
        leaf, issuer = self.intermediate()
        rogue = ec.generate_private_key(ec.SECP256R1())
        bad = self.certificate(issuer.subject, rogue, self.root_name, self.root_key, True, 0)
        self.rejected(leaf, bad, self.root)

    def test_root_path_length_exceeded(self):
        leaf, issuer = self.intermediate()
        root = self.certificate(self.root_name, self.root_key, self.root_name, self.root_key, True, 0)
        self.rejected(leaf, issuer, root, roots=[root])

    def test_expired_anchor(self):
        root = self.certificate(self.root_name, self.root_key, self.root_name, self.root_key, True, 1, days=-1)
        self.rejected(self.leaf, root, roots=[root])

    def test_alternate_trusted_roots_with_same_subject(self):
        key = ec.generate_private_key(ec.SECP256R1())
        wrong = self.certificate(self.root_name, key, self.root_name, key, True, 1)
        self.verify(self.leaf, roots=[wrong, self.root])

    def test_malformed_chain(self):
        key = self.leaf_key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                                         serialization.NoEncryption()).decode()
        for chain in ('', 'not PEM', self.pem(self.leaf) + '\n-----BEGIN CERTIFICATE-----\ninvalid\n-----END CERTIFICATE-----'):
            with self.subTest(chain=chain[:10]), self.assertRaises(ValueError):
                verify_grant_pairing(chain, key, 'test.example.com', self.pem(self.root))


if __name__ == '__main__': unittest.main()
