# Import compatibility

All adapters are independently written C++ under MIT. Upstream files were read as wire-format references, not copied as implementations. `tests/fixtures` contains generated fictional accounts; the fixture password is `synthetic päss🔐`.

| Adapter | Structure and encryption | Reference |
| --- | --- | --- |
| Aegis | Outer v1, database v1–3, `entries[].info`; scrypt password slots wrap a 32-byte master key; separate hex AES-GCM nonce/tag fields | [Vault specification](https://github.com/beemdevelopment/Aegis/blob/d6f4e5925a97e4e91593f1542085eae03432a759/docs/vault.md) |
| Google | `otpauth-migration://offline?data=` protobuf payload v1; batch ID/size/index must agree; duplicate and missing parts fail | [Migration wire schema](https://github.com/beemdevelopment/Aegis/blob/d6f4e5925a97e4e91593f1542085eae03432a759/app/src/main/proto/google_auth.proto) |
| Raivo | Array with `kind`, `timer`, `digits` (numeric strings), `issuer`, `account`, `secret`; ZIP reads exactly `raivo-otp-export.json` in memory, including WinZip AES encryption | [Exporter](https://github.com/raivo-otp/ios-application/blob/8ea4901b49996b9237d8e937aa2fa14890f2d43d/Raivo/Features/DataExportFeature.swift), [fields](https://github.com/raivo-otp/ios-application/blob/8ea4901b49996b9237d8e937aa2fa14890f2d43d/Raivo/Models/Password.swift) |
| 2FAS | Schemas 1–4; `services[].otp`; encrypted `cipher:salt:nonce` base64 string; PBKDF2-HMAC-SHA256/10000, AES-256-GCM | [Compatibility reference](https://github.com/beemdevelopment/Aegis/blob/d6f4e5925a97e4e91593f1542085eae03432a759/app/src/main/java/com/beemdevelopment/aegis/importers/TwoFASImporter.java) |
| Ente | Newline-separated OTP URIs; encrypted v1 `kdfParams`, `encryptedData`, `encryptionNonce`; Argon2id v1.3, one authenticated XChaCha20-Poly1305 secretstream message | [Exporter](https://github.com/ente-io/ente/blob/82851614b1e1276046a267249772c968544b6087/mobile/apps/auth/lib/ui/settings/data/export_widget.dart), [crypto implementation](https://github.com/ente/ente_crypto_dart/blob/981a9e0f4a227023991af3332ef0e6ab6d14a1c2/lib/src/crypto_util.dart) |
| Bitwarden Authenticator | `encrypted:false`, `items[].login.totp`; Android six-column CSV header (including its nine-field row variant), iOS eleven-column CSV with empty password/fields | [Android exporter](https://github.com/bitwarden/authenticator-android/blob/1fa1322827a4fefc1cac4666402e6ef3b4aceded/authenticator/src/main/kotlin/com/bitwarden/authenticator/data/authenticator/repository/AuthenticatorRepositoryImpl.kt), [iOS exporter](https://github.com/bitwarden/authenticator-ios/blob/main/AuthenticatorShared/Core/Vault/Services/ExportItemsService.swift) |
| Proton | JSON v1 `entries[].content.uri`; encrypted v1 salt and nonce-prefixed ciphertext; Argon2id 19 MiB/2 passes/1 lane, AES-GCM AAD `proton.authenticator.export.v1` | [Exporter](https://github.com/protonpass/proton-pass-common/blob/af901bef505e848db7d7691b49c85750a55c52f4/proton-authenticator/src/entry/exporter.rs), [encrypted exporter](https://github.com/protonpass/proton-pass-common/blob/af901bef505e848db7d7691b49c85750a55c52f4/proton-authenticator/src/entry/password_exporter.rs) |
| andOTP | Plain JSON array; legacy SHA256(password) + 12-byte nonce + AES-GCM ciphertext/tag; current BE32 iteration count + 12-byte salt + 12-byte nonce + AES-GCM, PBKDF2-SHA1 | [Compatibility reference](https://github.com/beemdevelopment/Aegis/blob/d6f4e5925a97e4e91593f1542085eae03432a759/app/src/main/java/com/beemdevelopment/aegis/importers/AndOtpImporter.java) |
| URI text | One `otpauth://` link per nonempty line, strict base32, duplicate parameters rejected; Steam URI/encoder variants | [Key URI format](https://github.com/google/google-authenticator/wiki/Key-Uri-Format) |

## Behavior

Format detection uses contents, not filename extensions. Password prompts appear only after recognizing an encrypted container. andOTP binary backups have no reliable marker, so Aeris asks the user to select current or legacy format. Binary garbage is never silently interpreted as a valid backup.

A supported token with an invalid secret, algorithm, period or digit count fails the entire import. HOTP and other declared token types are collected as exclusions instead. Empty/all-unsupported imports never replace the collection. No secret, password, raw URI or parser payload is included in error messages or application logs.

The phone-generated issuer/account label and OTP parameters are retained. Bitwarden Android's extra CSV columns are accepted only when they agree with the URI. iOS's password-shaped CSV header is shared with the password manager, so the parser restricts it to OTP-only rows with no password/custom fields. A password-manager export that is structurally identical to an Authenticator export cannot be distinguished from contents alone; full vault exports with password/URI fields are rejected.

Names containing unescaped CSV commas or newlines in a producer's malformed CSV cannot be recovered unambiguously; use that producer's JSON export. Extra enrollment QR images are rejected, even if a migration QR is present in the same screenshot.

## Resource limits

- 16 MiB per file/decrypted JSON document; 64 MiB combined input; 100 selected files/Google parts.
- 10,000 entries, 1,024 characters per issuer/account, 1,024 decoded secret bytes, 8,192 characters per URI, 4,096 password bytes.
- PNG/JPEG only, 25 megapixels, Qt image allocation limit 128 MiB; no SVG/remote image loading.
- ZIP: at most 64 members, 16 MiB per expanded member, 64 MiB total advertised expansion; no filesystem extraction or path traversal.
- PBKDF2: 1–10,000,000 iterations. Scrypt: power-of-two N ≤1,048,576; r ≤32, p ≤16, N×r ≤2,097,152, N×r×p ≤8,388,608; eight Aegis slots maximum.
- Argon2id: 8 KiB–1 GiB, 1–10 passes, memory×passes ≤4 GiB, one lane. This accepts Ente's 1 GiB/4-pass export setting; such imports have a substantially larger peak than normal operation.

Requirements beyond these limits are rejected rather than weakened. These are explicit compatibility limits, not automatic KDF downgrades.
