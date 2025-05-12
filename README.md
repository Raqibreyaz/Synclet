# Synclet

Synclet is a C++ module for efficient file synchronization. It tracks and syncs only the changed parts (chunks) of files, minimizing data transfer. Inspired by Git's delta-based sync, Synclet is designed for projects where bandwidth and speed are critical.

## Features

- Tracks files in a directory and breaks them into chunks
- Detects which chunks have changed, been added, or deleted
- Syncs only the changed chunks, not entire files
- Simple C++ API for integration

## Planned

- Fixed-size chunking (initial version)
- Option for content-defined chunking (future)
- Serialization of file/chunk state for fast comparison

---

**This is the initial setup. Implementation will follow.**
