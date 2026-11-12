# Local TLS fixture

The integration suite generates a fresh self-signed certificate and private key through `tests/tls_fixture.py`. Temporary PEM files are deleted as soon as the server context loads them. No private key or certificate is shipped in the repository. Never install a fixture certificate into a trust store.

The integration suite proves that the default client rejects this self-signed certificate and the explicit untrusted-TLS option still marks the peer unverified.
