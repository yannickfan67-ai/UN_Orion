# Native HTTPS / TLS

UN_Orion's native browser uses BearSSL 0.6 for TLS 1.2. The build downloads the exact BearSSL 0.6 release and verifies its SHA-256 before compiling it freestanding for the target architecture.

HTTPS is fail-closed: the server certificate chain, hostname and validity interval are validated against trust anchors generated from `TLS_CA_BUNDLE` (default `/etc/ssl/certs/ca-certificates.crt`). The native TLS layer requires a valid RTC clock and hardware RDRAND entropy; if either is unavailable, HTTPS is disabled rather than falling back to plaintext HTTP. HTTPS-to-HTTP redirects are rejected.

BearSSL is MIT licensed. The CA bundle remains governed by the licenses of the certificates/distribution that supplied it.

Limitations of this first native implementation: TLS 1.2 only; no TLS 1.3 yet, no certificate revocation/OCSP, one synchronous TCP/TLS session at a time, IPv4 only.
