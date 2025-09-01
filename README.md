# 🧠 Synclet – Efficient, Event-Driven File Sync Without Polling Waste

> **"A no-nonsense, performant file syncing engine that thinks in diffs and sleeps when idle."**

[![Demo Video](https://img.shields.io/badge/▶️-Watch%20Demo-blue)](./Synclet-Demo.mkv)

---

- 👉 [Watch on YouTube](https://youtu.be/t_7HGiFl3x0)

---

## 🚀 Why Synclet?

Tired of constant polling? So were we.
Tired of syncing whole files just because 2 bytes changed? Same here.

**Synclet** is built to **sync files precisely**, using **content-defined chunking**, **event-driven notifications**, and **peer-based snapshots**, all without wasting cycles.

---

## 🔍 Key Features

- 💤 **No Polling** – Uses `inotify` + `epoll` to only wake up when something changes.
- 🧩 **Smart Chunk Sync** – Breaks files into logical chunks using rolling hashes, syncing only what’s needed.
- 🧠 **Snapshot Hashing** – 32-byte global snapshot versioning detects even the smallest file change across machines.
- 🔄 **Two-Way Syncing** – Automatically detects if peer is ahead or behind, then fetches or sends.
- 📦 **Binary-safe Transfers** – No corruption, even in weird chunk boundaries. Everything's binary-packed.
- 📂 **Recursive Directory Tracking** – Watches all nested directories efficiently.
- 🛠️ **Protocol-Based Events** – From `FILE_CREATE` to `MODIFIED_CHUNK`, your peer always knows what's happening.
- 📊 **Progress Tracking** – Visual indicators of how much sync has completed.

---

## 🧬 How It Works

### 🔹 Snapshot Creation

Each file is chunked using **rolling Rabin-Karp hashes**:

- A window slides over bytes.
- If `hash % N == 0`, a chunk boundary is marked.
- SHA-256 ensures strong uniqueness of each chunk.

> ✅ **CDC over fixed-size**: Fixed-size fails for small inserts — everything shifts. Not here.

### 🔹 Event-Driven Sync

- Uses `inotify + epoll` to watch files/dirs non-blockingly.
- File events (`modify`, `create`, `move`, `delete`) are captured **instantly**, no polling.

### 🔹 Efficient Network Protocol

Your protocol isn't ad-hoc. It's designed like this:

```json
TYPE: MODIFIED_CHUNK
PAYLOAD: {
  filename, offset, chunk_size, old_chunk_size, is_last_chunk
}
```

> 🔗 All operations are structured via typed messages + raw data when required.

### 🔹 Initial Sync Logic

- Peers exchange `snap-version` hashes.
- If versions differ:

  - Only the required chunks/files are transferred.
  - Supports downloading missed changes **or** pushing local changes back.

---

## 🔧 Setup

```bash
make client   # Builds the client binary in ./client/output
make server   # Builds the server binary in ./server/output
```

---

## 🔬 Developer Logs (a few highlights)

> See [`dev-logs.txt`](./dev-logs.txt) for more

- ✅ **Reduced polling overhead** using `inotify` instead of file scan loops.
- ✅ **Chunking optimization** from char-by-char to buffer-based reads to reduce syscalls.
- ✅ **Smart rename detection** using `cookie` and timestamps.
- ✅ **Snap versioning** to avoid sending full snapshot every time.
- ✅ **Progress bar** for user feedback.
- ✅ **File sanitization** to avoid invalid chars in saved chunks.

---

## 🔬 Technical Achievements

### Architecture & Design

- **Event-driven I/O**: Replaced polling-based file monitoring with `inotify` + `epoll` for CPU-efficient, non-blocking operations
- **Content-defined chunking**: Implemented Rabin-Karp rolling hash algorithm to detect logical chunk boundaries, enabling delta synchronization
- **Snapshot versioning**: Designed 32-byte hash-based versioning system to track directory states and detect changes across peers

### Optimization & Reliability

- **Binary-safe storage**: Implemented custom binary format for chunk metadata and data to prevent corruption across all file types
- **Smart rename detection**: Developed timer-based pairing of `IN_MOVED_FROM`/`IN_MOVED_TO` events using 200ms windows and cookie matching
- **Ordered reconstruction**: Used ordered sets to ensure deterministic file reconstruction regardless of chunk arrival order
- **Efficient I/O**: Optimized from character-by-character to buffer-based reads to minimize system calls

### Protocol & Sync Logic

- **Bidirectional sync**: Implemented comparison logic to determine whether to push local changes or fetch remote updates based on snapshot versions
- **Progress visibility**: Added real-time progress tracking for ongoing sync operations
- **Recursive monitoring**: Efficient tracking of nested directory structures with automatic watch registration

## 📁 Blueprint Insights

All low-level strategies, from CDC algorithms to protocol structure, are in [`blue-print.txt`](./blue-print.txt). Here’s a glimpse:

- 🔍 **Hybrid Chunk Hashing**: Rolling hash to detect chunk boundary, SHA-256 to fingerprint.
- 🔐 **Binary Chunk Storage**: Metadata and data are stored in raw `.bin` files with custom structure.
- 📤 **Ordered Set for Chunks**: Ensures deterministic file reconstructions from random chunk delivery order.
- 🕵️ **Rename detection**: Paired `IN_MOVED_FROM`/`IN_MOVED_TO` using 200ms time window.

---

## ❤️ Built For

Engineers who hate noisy logs, wasted CPU cycles, or bloated tools — and want **clarity**, **control**, and **clean syncing logic**.

---

## 📽️ Demo

- 👉 [Watch Full Demo](./Synclet-Demo.mkv)
- 👉 [Watch on YouTube](https://youtu.be/t_7HGiFl3x0)

---

## 🧩 Credits & Inspiration

Thanks to:

- `inotify`, `epoll`, and `timerfd` — for event-driven efficiency.
- `nlohmann/json` — for no-fuss JSON serialization.
- Classic chunking papers + rsync for conceptual inspiration.

---
