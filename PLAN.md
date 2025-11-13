# Blockit + SQLite Integration Plan

Purpose: Keep the blockchain as the immutable audit log while persisting large/query‑heavy data in SQLite for fast lookups, search, and analytics. On‑chain entries anchor off‑chain records via cryptographic hashes and references (block height/tx id). This preserves verifiability with practical query performance.


## Goals
- Fast queries on large datasets (filters, aggregations, full‑text search) via SQLite.
- Preserve immutability: every off‑chain record is anchored on‑chain with a content hash and references.
- Deterministic hashing: same content → same hash across time and platforms.
- Minimal code intrusion; clean module boundary (storage vs. chain).
- Backward compatibility: optional migration to backfill anchors for existing chain data.


## High‑Level Architecture
- On‑chain (existing): `Block`, `Transaction`, `Merkle` remain the source of truth for audit.
- Off‑chain (new): SQLite database for domain data, indexes, and FTS.
- Anchor linkage: each persisted row has an anchor tuple: `(content_hash, tx_id, block_height, merkle_root)`; on‑chain transactions include the same `content_hash` inside their payload.
- Verification: recompute hash from row → compare with on‑chain tx payload hash and (optionally) verify inclusion against block merkle root.


## Data Model (SQLite)
Recommended database file: `data/blockit.db` (configurable via env/CLI).

Pragmas at open (tunable per environment):
- `PRAGMA journal_mode=WAL;`
- `PRAGMA synchronous=NORMAL;` (or `FULL` for stronger durability)
- `PRAGMA foreign_keys=ON;`
- `PRAGMA busy_timeout=5000;`
- `PRAGMA cache_size=-20000;` (approx 20MB)

Schema (initial):

1) Domain records (example: a generic documents table; adapt to your domain)
- `records(id TEXT PRIMARY KEY, created_at INTEGER NOT NULL, updated_at INTEGER NOT NULL, schema_version INTEGER NOT NULL, payload BLOB NOT NULL)`
- Indexes:
  - `CREATE INDEX IF NOT EXISTS idx_records_created ON records(created_at);`
  - `CREATE INDEX IF NOT EXISTS idx_records_updated ON records(updated_at);`

2) Anchors (ties SQLite rows to on‑chain)
- `anchors(record_id TEXT PRIMARY KEY, content_hash BLOB NOT NULL, tx_id TEXT NOT NULL, block_height INTEGER NOT NULL, merkle_root BLOB NOT NULL, anchored_at INTEGER NOT NULL, UNIQUE(tx_id))`
- FKs (optional): `FOREIGN KEY(record_id) REFERENCES records(id) ON DELETE CASCADE`
- Indexes:
  - `CREATE INDEX IF NOT EXISTS idx_anchors_block ON anchors(block_height);`

3) Optional materialized views / indexes for common filters
- E.g., `CREATE INDEX IF NOT EXISTS idx_records_schema ON records(schema_version);`

4) Full‑Text Search (optional but recommended for large text bodies)
- External content table pattern with FTS5:
  - `CREATE VIRTUAL TABLE records_fts USING fts5(record_id, content, content='records', content_rowid='rowid');`
  - Triggers to maintain FTS from `records` (insert, update, delete).
  - Alternatively, store searchable text fields separately from `payload` and index them in FTS.


## Hashing & Canonicalization
- Hash algorithm: use existing Lockey SHA256 for consistency with current code.
- Canonicalization of payloads before hashing:
  - Define a canonical binary representation to avoid hash drift. Options:
    - a) Canonical CBOR (recommended)
    - b) Stable JSON (sorted object keys, UTF‑8, no insignificant whitespace)
  - Store the exact canonical bytes in `payload` column; compute `SHA256(payload)`.
- Transaction payloads on‑chain should carry `content_hash` and `record_id`.


## Integration Points (C++)
New module: `storage/sqlite`.

Files to add:
- `include/blokit/storage/sqlite_store.hpp`
- `src/storage/sqlite_store.cpp`
- `src/storage/migrations.sql` (embedded or loaded at runtime)

Class sketch (`SqliteStore`):
- `open(const std::string& path, OpenOptions opts)` → opens DB, applies pragmas, runs migrations.
- `begin_tx() / commit_tx() / rollback_tx()` → RAII transaction guard.
- `upsert_record(const Record& r)` → writes payload row and updates `updated_at`.
- `anchor_record(const std::string& id, Bytes content_hash, const TxRef& ref)` → inserts into `anchors`.
- `get_record(const std::string& id)` → returns payload + anchor if present.
- `query_records(const Query&)` → filters + pagination using prepared statements.
- `search(text)` → FTS query if enabled (returns record_ids with ranking).
- `verify_anchor(const std::string& id, ChainView& chain)` → recompute hash from `records.payload`, compare to `anchors.content_hash`, fetch tx by `tx_id`, verify it contains the same hash, optionally verify merkle inclusion vs. `merkle_root`.

Types:
- `Record { std::string id; int64_t created_at; int64_t updated_at; int32_t schema_version; std::vector<uint8_t> payload; }`
- `TxRef { std::string tx_id; int64_t block_height; std::array<uint8_t,32> merkle_root; }`


## Chain Hooks
Where to call storage methods (minimal intrusion):
- Transaction creation (client/API layer):
  1) Canonicalize domain object → `payload`
  2) `content_hash = sha256(payload)`
  3) Prepare on‑chain transaction embedding `{record_id, content_hash}`
  4) Write off‑chain `records` row (unanchored yet)
- Block commit (node side):
  - After a block is finalized:
    - For each tx with `{record_id, content_hash}`: call `anchor_record(record_id, content_hash, {tx_id, height, merkle_root})`
    - Optional: verify recomputed hash from `records` matches `content_hash` before anchoring; if missing record, mark as orphan/unresolved anchor (diagnostic table: `pending_anchors`).


## Migrations
- `migrations.sql` contains idempotent DDL (guard with `IF NOT EXISTS`).
- Version table: `schema_migrations(version INTEGER PRIMARY KEY, applied_at INTEGER NOT NULL)`
- `SqliteStore::migrate()` runs in a single transaction.


## CMake & Dependencies
- Add SQLite3 (system) and SQLiteCpp (C++ wrapper):
  - `find_package(SQLite3 REQUIRED)` and `FetchContent_Declare(SQLiteCpp ...)`
  - Link targets: `SQLite::SQLite3` and `SQLiteCpp`
- Add new sources/headers to the core library target.
- Option flags: `-DBLOKIT_SQLITE=ON` to enable; compile out with `#ifdef BLOKIT_SQLITE`.


## Configuration
- New env/CLI options:
  - `BLOKIT_DB_PATH` (default: `data/blockit.db`)
  - `BLOKIT_DB_WAL=1` (enable WAL)
  - `BLOKIT_DB_FTS=1` (enable FTS)
- Add to `README.md` configuration section.


## Query Patterns
- Exact filters: `SELECT ... FROM records WHERE created_at BETWEEN ? AND ? AND schema_version=?` with indexes.
- FTS queries (if enabled): `SELECT record_id FROM records_fts WHERE records_fts MATCH ? ORDER BY rank LIMIT ? OFFSET ?` then fetch rows.
- Verified reads:
  - `get_record(id)` + `verify_anchor(id)` if the caller requires audit integrity.


## Testing Strategy
1) Unit tests (storage): open/close, migrations, CRUD, anchor, WAL mode, busy handling.
2) Integration tests (chain + storage):
   - Create tx with `{record_id, content_hash}`; commit a block; anchor; verify.
   - Tamper test: mutate `records.payload` → `verify_anchor` must fail.
   - Missing record: anchor arrives before record write → tracked as `pending_anchors`, later resolved.
3) FTS tests (if enabled): insert records with text fields, ensure queries return expected order.
4) Concurrency tests: parallel writers + readers; ensure no SQLITE_BUSY storms (busy timeout/backoff).


## Backfill Plan (optional)
- For existing chains:
  1) Iterate historical blocks.
  2) For each tx with `{record_id, content_hash, maybe payload snapshot}`:
     - If record absent: create a `records` row with `payload` from historical data if available, else create a stub with `NULL` payload and mark `needs_materialization`.
     - Insert `anchors` rows with historical `tx_id`, `block_height`, `merkle_root`.
  3) Create a `backfill_state` table to checkpoint progress; support resume.


## Observability
- Add counters/gauges:
  - `sqlite_write_ms`, `sqlite_read_ms`, `anchors_created_total`, `verify_fail_total`.
- Log at INFO on migration, WARN on anchor mismatch, ERROR on DB open/IO failure.


## Error Handling & Edge Cases
- Handle `SQLITE_BUSY` with busy timeout and one retry loop (bounded).
- Wrap all write phases in transactions.
- Ensure deterministic hashing: a single canonicalization function used by both client and node.
- Partial failure on block commit: continue anchoring remaining txs, collect failures with reasons; retry on next startup.
- DB corruption detection: PRAGMA quick_check on startup in non‑prod or on admin command.


## Security Considerations
- Do not store private keys in SQLite.
- Protect DB file permissions; optionally support file encryption at rest (via OS or SQLite codec if available in target env).
- Validate lengths and bounds before inserting BLOBs.


## Incremental Delivery Plan
1) Storage skeleton + migrations + basic CRUD.
2) Anchor API + chain commit integration (write anchors on finalized block).
3) Deterministic hashing utility + tx payload carry `{record_id, content_hash}`.
4) Verification plumbing + tests.
5) Index tuning and config.
6) Optional FTS + triggers.
7) Backfill tool (CLI subcommand: `blokit backfill --from-height 0`).


## Code Touchpoints (proposed)
- New:
  - `include/blokit/storage/sqlite_store.hpp`
  - `src/storage/sqlite_store.cpp`
  - `src/storage/migrations.sql`
- Existing (hook only; avoid invasive changes):
  - `include/blokit/structure/transaction.hpp` → ensure tx payload carries `{record_id, content_hash}`
  - `include/blokit/structure/block.hpp` → expose `merkle_root` and block height for anchoring
  - `include/blokit/structure/chain.hpp` → callback after block finalize → `SqliteStore::anchor_record(...)`
  - `README.md` → config and ops
  - `CMakeLists.txt` → SQLite linkage
  - `test/` → add storage + integration tests


## Example Flow (happy path)
1) Client assembles domain object D; canonicalize to bytes P.
2) Compute H = SHA256(P); set tx.payload = {record_id, H, optional metadata}.
3) Submit tx; node includes in block B; merkle_root = R.
4) On commit: `SqliteStore.anchor_record(record_id, H, {tx_id, height(B), R})`.
5) Query: `get_record(record_id)`; if audit needed → `verify_anchor(record_id)`.


## Risks & Mitigations
- Hash drift due to inconsistent encoding → centralized canonicalizer and tests; record encoding version.
- Orphan anchors (no row at commit time) → `pending_anchors` queue, reconciler job.
- Performance regressions → WAL, prepared statements, tuned indexes; measure.
- FTS bloat → consider `detail=column` or contentless external table if only indexing.


## Next Steps (actionable)
- Wire SQLite3 dependency in CMake.
- Implement `SqliteStore` with migrations and pragmas.
- Add chain commit hook to write anchors.
- Add deterministic canonicalizer + SHA256 wrapper (Lockey).
- Extend transaction payload to include `{record_id, content_hash}`.
- Write integration tests sealing end‑to‑end anchor + verify.
