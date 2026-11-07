#!/usr/bin/env python3
"""Root CA bundle for the link viewer: any https:// site the user opens from a chat.

Reads a PEM file of trusted roots (default: the build machine's system store, Mozilla-derived) and writes
src/services/ca_bundle.h in the format WiFiClientSecure::setCACertBundle() expects (Arduino esp_crt_bundle.c):
  u16 count, then per root: u16 subject length, u16 public key length, DER subject, DER SubjectPublicKeyInfo,
sorted by DER subject (the verifier binary-searches on the issuer name).

  tools/make_ca_bundle.py [roots.pem]
"""
import sys
from pathlib import Path
from cryptography import x509
from cryptography.hazmat.primitives import serialization

REPO = Path(__file__).resolve().parents[1]
source = Path(sys.argv[1] if len(sys.argv) > 1 else '/etc/ssl/certs/ca-certificates.crt')
roots = {}
for block in source.read_text().split('-----END CERTIFICATE-----'):
    if '-----BEGIN CERTIFICATE-----' not in block:
        continue
    cert = x509.load_pem_x509_certificate((block + '-----END CERTIFICATE-----').encode())
    name = cert.subject.public_bytes()
    key = cert.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)
    roots[name] = key  # one entry per subject (re-issued roots keep the same name)
bundle = bytearray(len(roots).to_bytes(2, 'big'))
for name in sorted(roots):
    key = roots[name]
    bundle += len(name).to_bytes(2, 'big') + len(key).to_bytes(2, 'big') + name + key
lines = [', '.join('0x%02x' % b for b in bundle[i:i + 24]) for i in range(0, len(bundle), 24)]
(REPO / 'src/services/ca_bundle.h').write_text(
    '#pragma once\n#include <stdint.h>\n'
    f'// Root CAs for the link viewer ({len(roots)} roots, {len(bundle)} bytes): tools/make_ca_bundle.py from {source.name}.\n'
    'inline constexpr uint8_t LINK_CA_BUNDLE[] = {\n ' + ',\n '.join(lines) + '\n};\n')
print(len(roots), 'roots,', len(bundle), 'bytes')
