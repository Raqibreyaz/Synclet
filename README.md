# 📁 Synclet

> A fast, intelligent, chunk-based file sync and backup tool, powered by **Content Defined Chunking (CDC)**.

---

## 🚀 Overview

**Synclet** is a custom file synchronization protocol that detects file-level and chunk-level changes using **rolling hashes** and **SHA-256**. It's designed to efficiently sync modified chunks only, rather than entire files, saving bandwidth and storage. Inspired by tools like *rsync* and *Google Photos*, it keeps metadata snapshots and supports bidirectional sync using a robust and extensible protocol.

---

## 🔍 How It Works

### 📸 Snapshot Creation

A snapshot is a metadata structure describing all files and their chunk-level content using SHA-256 hashes.

#### ✅ Steps:

1. For each file, read using a **sliding window** (`w` bytes).
2. Calculate a **rolling hash** (like Rabin-Karp) for the window.
3. When `rolling_hash % N == 0`, a **chunk boundary** is detected.
4. Generate a SHA-256 hash for the chunk and record it in the snapshot.

> ✅ Weak hash (rolling) detects boundaries, strong hash (SHA-256) confirms uniqueness.

#### ⚠ Why not fixed-size chunks?

Fixed-size chunks fail when even a small change shifts all data forward. CDC detects natural boundaries, minimizing reuploads for small edits.

---

## 🔄 Change Detection Logic

Each chunk has metadata:

```json
{
  "offset": 0,
  "chunk_size": 1024,
  "chunk_no": 2,
  "hash": "<sha256>"
}
```

### 🔁 What’s considered modified?

* ✅ `hash` — definitive change indicator.
* 🚫 `offset` or `chunk_no` alone are **not** sufficient to consider a chunk modified.

---

## 📡 Protocol Design

Every sync action is represented by a `TYPE` and associated `PAYLOAD`.

### ✉️ Protocol Message Types

| Type             | Description               | Payload | Raw Data |
| ---------------- | ------------------------- | ------- | -------- |
| `MODIFIED_CHUNK` | Existing chunk modified   | ✅       | ✅        |
| `FILE_CREATE`    | New file created          | ✅       | ❌        |
| `FILE_REMOVE`    | File deleted              | ✅       | ❌        |
| `FILE_RENAME`    | File renamed              | ✅       | ❌        |
| `FILES_CREATE`   | Multiple file creation    | ✅       | ❌        |
| `FILES_REMOVE`   | Multiple file deletions   | ✅       | ❌        |
| `SEND_FILE`      | Sending full file         | ✅       | ❌        |
| `REQ_CHUNK`      | Request specific chunk    | ✅       | ❌        |
| `SEND_CHUNK`     | Chunk data in response    | ✅       | ✅        |
| `REQ_SNAP`       | Request peer snapshot     | ❌       | ❌        |
| `DATA_SNAP`      | Send complete snapshot    | ✅       | ❌        |

---

## 📁 Snapshot Format

```json
{
  "version": "filename|size|offset:chunk_size:chunk_hash",
  "files": [
    {
      "filename": "docs/report.txt",
      "size": 12345,
      "mtime": 1717100000,
      "chunks": {
        "<sha256>": {
          "chunk_no": 1,
          "hash": "<sha256>",
          "offset": 0,
          "chunk_size": 4096
        }
      }
    }
  ]
}
```

---

## ⛓ Copying from Original to Temp File

To reconstruct modified files:

```json
chunks = [
  { "offset": 10, "size": 10 },
  { "offset": 30, "size": 40 },
  { "offset": 100, "size": 50 }
]
```

Steps:

* Copy original data between modified chunks.
* Insert new chunk data where required.
* Combine to form final temp file.

---

## 🔰 Initial Sync Strategy

### When starting from scratch:

1. Client checks for local `peer-snap-file.json`.
2. If absent, client sends `REQ_SNAP` to server.
3. Server replies with `DATA_SNAP`.
4. Client compares snapshots to:

   * Add missing files to server.
   * Fetch new/updated files from server.
   * Delete obsolete files (if server is outdated).

### Conflict Resolution:

* Use `mtime` to resolve direction of sync:

  * Client newer → upload delta chunks.
  * Server newer → fetch delta chunks.
* Delta chunks stored temporarily in `[filename]_dir/chunk-offset.bin`.

---

## 🧹 Cleaning Noisy Events (Under Consideration)

To debounce filesystem noise:

```ts
HashMap<filename, {
  last_event,
  last_time,
  timer_active,
  scheduled_action
}>
```

This avoids redundant syncs when rapid file system events (e.g., save triggers modify + delete + modify) occur.

---

## 🧠 Design Inspirations

* **rsync**: for delta-based sync
* **Bup/Duplicacy**: for snapshot design
* **Google Photos**: for metadata-based intelligent organization
* **Telegram**: potential channel storage

---

## 🤝 Contribute

Feel free to suggest features, report issues, or create pull requests!

---