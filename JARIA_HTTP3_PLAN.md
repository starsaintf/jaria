# Jaria HTTP/3/QUIC Implementation Plan

## Current Status

Jaria now has HTTP/3-aware configuration, feature detection, option parsing, and origin-scoped fallback state. The current Windows/MSYS2 UCRT64 build does not compile a QUIC transport because libngtcp2 and libnghttp3 development headers/pkg-config files are not available in that toolchain.

## Required Libraries

- libngtcp2 for QUIC transport
- libnghttp3 for HTTP/3 framing
- OpenSSL with QUIC support, or an ngtcp2 crypto helper compatible with the selected OpenSSL package

## Transport Boundary

HTTP/3 must remain separate from the existing TCP `HttpConnection`/`Http2Connection` path. HTTP/3 uses UDP and QUIC TLS, so it should not be implemented as another ALPN branch on the TCP socket.

The intended first transport class is:

- `Http3Connection`: owns the UDP socket, ngtcp2 connection, nghttp3 connection, request headers, response headers, and response body buffer.
- `Http3RequestCommand`: selects HTTP/3 only for HTTPS origins when `--enable-http3=true`, dependencies are compiled in, and the origin is not in the HTTP/3 disabled-origin cache.
- fallback path: HTTP/3 failure disables HTTP/3 for the origin and retries through HTTP/2/HTTP/1.1 without consuming the normal retry budget.

## First Complete Wire Slice

1. Single GET request.
2. No multiplexing.
3. No 0-RTT.
4. No persistent Alt-Svc cache.
5. HTTPS origin only.
6. Fallback to the existing HTTP/2/HTTP/1.1 path on QUIC connection, stream, or HTTP/3 framing failure.

## Validation

- Unit tests for feature detection and option parsing.
- Unit tests for origin-scoped HTTP/3 fallback disable.
- Unit tests that HTTP/3 does not alter TCP ALPN advertisement.
- Optional live smoke target against a known HTTP/3 endpoint after a QUIC-capable build is available.
