# Architecture and data lifetime

`Importer` dispatches independent format adapters in `src/import/`. They return `ImportResult` with immutable `shared_ptr<const Entry>` objects, a detected source name and unsupported-entry diagnostics. Parsing/decryption runs in a QtConcurrent worker. The GUI waits for one replacement confirmation, then runs persistence in another worker. It changes the visible collection only after the snapshot commits.

`OtpEngine` has an injectable seconds clock. It derives every code from current wall time, including on copy. SHA-1/SHA-256/SHA-512 HMAC and AES/PBKDF/scrypt use OpenSSL; Argon2id and Ente secretstream use libsodium. Secrets owned by `Secret` are overwritten on destruction. JSON/parser/Qt/keychain internals can make temporary copies outside that ownership; Aeris does not claim guaranteed erasure of allocator residue, OS swap, crash dumps or third-party clipboard history.

`AccountModel` stores immutable entries; `AccountDelegate` paints visible rows without allocating widgets per account. One one-second timer repaints the viewport and checks clipboard expiry. Application activation also recalculates after resume. Clipboard lifetime uses a monotonic clock plus a wall-time deadline so sleep or wall-clock adjustments cannot extend ownership indefinitely. Clearing checks the exact owned `QMimeData`, not just matching text.

`AccountView` switches between list, compact and responsive card layouts using the same model/delegate. One transient `QVariantAnimation` supplies in-place copy feedback for 1.1 seconds and stops when finished; it adds no idle polling. Account presentation data caches a monogram, color, small identifying mark and brand identifier at model replacement. Exact normalized issuer matches use the bundled catalog; Steam tokens override the issuer. Ambiguities and unknown names keep badges, and account email addresses never participate in matching. Rendered brand pixmaps use a GUI-thread QCache bounded to 8 MiB, keyed by identifier, size and display scale; failed image loads retain the badge. Countdown updates do not repeat matching. Badge colors derive from issuer/account labels, never secrets. Token entries, import formats and encrypted storage contain no icon metadata. No remote icon requests are made. Qt preferences store only the selected theme and layout, separately from the encrypted account snapshot. Delete all retains these appearance preferences.

`VaultStore` exposes load, replace and clear. Its credential/filesystem backends are injectable for failure tests. Production `SystemCredentials` uses QtKeychain with insecure fallback explicitly disabled. Keyring jobs have a 25-second response deadline and return an unlock/retry error. The main process holds a `QLockFile` to prevent concurrent vault mutations.

## Snapshot transaction

The application-data directory comes from Qt's platform `AppLocalDataLocation`. The keychain service is `com.cyberfracture.aeris`. Account contents are in one AES-256-GCM `vault.json`; the envelope contains only version, a random credential UUID, nonce and ciphertext/tag. `Aeris/v1/<credential UUID>` is authenticated associated data. Files are owner-only; the directory is owner-only on POSIX.

Replacement sequence:

1. Finish any pending recovery; validate and serialize the new collection in memory.
2. Generate a fresh 256-bit key, random 96-bit nonce and credential UUID.
3. Atomically write `transaction.json` containing the old/new credential IDs (no passwords or OTP secrets).
4. Stage the new key in the system keyring, then read it back and verify it before committing.
5. Atomically commit `vault.json` using `QSaveFile`, with direct-write fallback disabled. Synchronize the containing directory on POSIX.
6. Delete the old credential; remove the journal. If cleanup fails, report that replacement committed and leave the journal for retry.

On restart, the snapshot's key ID decides which credential to retain. An interruption before snapshot commit retires the staged key; after commit it retires the old key. A write error before commit preserves the old snapshot. If rename succeeded but directory synchronization failed, the new collection is reported as committed with a durability warning rather than incorrectly claiming rollback.

Delete all first records a durable deletion journal. It removes the encrypted snapshot, deletes referenced credentials, then removes the journal. An interruption resumes deletion on load. A failed deletion clears the visible entries and owned clipboard immediately, retains a retry action and does not falsely report completion. Source files are never opened for writing.

No export password or plaintext intermediary is written to disk. Archive members are read directly from bounded memory. Application logs contain no account content; development benchmark output contains only timing/size measurements. Qt's supported platform keychain libraries and the OS control their own internal storage and prompts.

## Account order

AccountView handles internal pointer movement after Qt’s drag-distance threshold, paints an insertion marker and autoscrolls only while dragging. It emits source/destination positions without creating a drag payload, exporting secrets or copying OTP codes. Click-to-copy remains on an ordinary release. Escape, lost focus, layout changes and release outside the viewport cancel the gesture. External export-file drops continue through the existing window import handler.

Window maps visible proxy positions to the underlying sequence and saves a proposed reordered Entries vector with the existing transactional VaultStore::replace operation on the worker thread. Imports, deletion and further order changes are disabled while saving; search and code copying remain available. Close requests are deferred until completion so the application event loop continues servicing the keyring. Only a successful commit moves the model row; failed saves leave its order intact. A committed transaction with a cleanup warning updates the display and reports the warning. QAbstractItemModel row-move notifications preserve selection and cached presentation identities. The search query stays intact. The vault already encodes ordered arrays, so no storage schema, token entry or plaintext order setting was added.

## Keyring event-loop ownership

Every SystemCredentials request creates, starts and finishes its QtKeychain job on the application thread. Disk and encryption work stays on worker threads, which await queued completion. This keeps QtKeychain's global job executor and libsecret callbacks on a persistent event loop rather than an expiring thread-pool thread. Closing an active window waits asynchronously; destructor cleanup also services events instead of blocking the keyring callback.

A 25-second deadline reports an unavailable keyring. The unfinished job remains alive for late backend callbacks, and additional keyring requests fail until that job finishes. This prevents cleanup from racing a late credential write and preserves the transaction journal for recovery. No credential service name, vault format, or insecure-fallback policy changes.
