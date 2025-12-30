<div align="center">
	<br>
	<h1>Barrel</h1>
	<p>
		<b>High-performance virtual file system for game engines, resource archives, embedded system and more</b>
	</p>
	<br>
</div>

# Overview
Barrel is a lightweight, read-only virtual filesystem designed for speed, memory efficiency, and predictable access patterns.
It allows you to bundle files into a single memory-mapped container, eliminating file I/O overhead and handle juggling.

Originally built for a 3D engine, Barrel can also serve as a backup format, resource archive, and more.

# Key Features
* **Zero-Copy Access**: Access file data directly via pointers. No intermediate buffers or memcpy required when using memory-mapped mode.
* **Dual-Mode Operation**:
  * **Standard Context**: Best for small-to-medium archives where the entire file is mapped to memory.
  * **Chunked Context**: High-performance streaming with a built-in LRU (Least Recently Used) Cache for large archives that exceed available RAM.
* **Hash-Based Lookups**: Filenames are stored as precomputed uint64_t hashes, providing O(1) average-case lookup time via a power-of-two hash table.
* **Integrity Verification**: Built-in 64-bit checksums for every file entry to detect data corruption.
* **Self-Contained**: Zero external dependencies beyond standard headers (stdint, string, stdlib).

# Configuration 
You can tune Barrel by defining these macros before including the headers:
* `VFS_MAX_CHUNKS`: Number of slots in the LRU cache (default: 8).
* `BRL_STRUCT_PACKING_BYTES`: Alignment for the binary format (default: 8).
* `BRL_DISABLE_STRUCT_PACKING`: Define this if targeting a platform with strict alignment requirements.

# Examples
This repository provides samples and example tools that can be used to interact with the Barrel format. Check `sample/` and `tools/`.

# Archive Structure
Barrel uses Relative Virtual Addresses (RVA). 
The entire archive is a single contiguous block, making it perfectly safe for `mmap`.
```text
+----------------------------------------------------+
| BRLHeader                                          |  <-- Magic signature, versioning, and offsets to the Table and Data sections.
|  - signature                                       |
|  - version                                         |
|  - fileCount                                       |
|  - tableOffset                                     |
|  - dataOffset                                      |
+----------------------------------------------------+
| VFSFileEntry[fileCount]                            |  <-- An array of descriptors containing name hashes and data pointers.
|  - nameHash                                        |
|  - dataRVA                                         |
|  - dataSize                                        |
|  - dataChecksum                                    |
+----------------------------------------------------+
| VFSDataBlob[]                                      |  <-- The raw payload. Each blob is prefixed by its own size for safety.
|  - size                                            |
|  - data[]                                          |
+----------------------------------------------------+
```

# Performance Comparison
| Feature          | Standard File I/O (stdio.h)                       | Barrel (Standard)                            | Barrel (Chunked)                               |
|------------------|---------------------------------------------------|----------------------------------------------|------------------------------------------------|
| Data Access      | Copy-based: fread copies data from Kernel to App. | Zero-copy: Direct pointer to archive memory. | Hybrid: Copies small chunks into LRU cache.    |
| Lookup Speed     | O(N): OS must parse directory strings.            | O(1): Instant hash-table lookup.             | O(1): Instant hash-table lookup.               |
| Memory Footprint | Dynamic: Scales with number of open files.        | Linear: Equal to total archive size.         | Fixed: Header + (Chunk Size × VFS_MAX_CHUNKS). |
| I/O Overhead     | High: Multiple syscalls per file open/read.       | Zero: No syscalls after initial load.        | Low: Minimal syscalls via readFunc callback.   |

# License
Barrel is free for personal and commercial use under the MIT License.
You can use, modify, and integrate it into your engine or tools.
Forking and contribution is heavily encouraged!
