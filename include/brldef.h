/**
 * @file brldef.h
 * @brief Barrel core definitions.
 *
 * This file contains the core definitions for the Barrel VFS structure.
 */

#ifndef BRL_DEF
#define BRL_DEF

#include <stdint.h>

// -----------------------------------------------------------
// VFS Signature and Version
// -----------------------------------------------------------
/// @brief Unique file signature used to identify Barrel archives.
#define BRL_SIGNATURE       0x45410000

/// @brief Major version of the Barrel format.
#define BRL_VER_MAJOR       1
/// @brief Minor version of the Barrel format.
#define BRL_VER_MINOR       0
/// @brief Patch version of the Barrel format.
#define BRL_VER_PATCH       0

/// @brief Encoded full version (major, minor, patch) as 32-bit value.
#define BRL_VER ((BRL_VER_MAJOR << 24) | (BRL_VER_MINOR << 16) | BRL_VER_PATCH)

/// @brief Extracts the major version from an encoded version value.
#define BRL_VER_GET_MAJOR(ver)  ((uint8_t)(((ver) >> 24) & 0xFF))
/// @brief Extracts the minor version from an encoded version value.
#define BRL_VER_GET_MINOR(ver)  ((uint8_t)(((ver) >> 16) & 0xFF))
/// @brief Extracts the patch version from an encoded version value.
#define BRL_VER_GET_PATCH(ver)  ((uint8_t)((ver) & 0xFF))

/// @brief Maximum number of chunks that can be cached simultaneously.
#ifndef VFS_MAX_CHUNKS
#define VFS_MAX_CHUNKS                8
#endif

/**
 * @brief Read callback type for chunked VFS reads.
 *
 * This callback is invoked to read data from a chunked context.
 *
 * @param userData  Pointer to user-defined data (e.g., FILE handle).
 * @param buffer    Output buffer to store the read contents.
 * @param offset    Requested read offset (RVA).
 * @param size      Number of bytes to read.
 * @return Number of bytes actually read.
 */
typedef uint64_t (*VFSReadCallback)(
    void*    userData,
    void*    buffer,
    uint64_t offset,
    uint64_t size
);

/// @brief Struct packing alignment (in bytes) used for VFS structures.
#ifndef BRL_DISABLE_STRUCT_PACKING
#ifndef BRL_STRUCT_PACKING_BYTES
#define BRL_STRUCT_PACKING_BYTES      8
#endif
#pragma pack(push, BRL_STRUCT_PACKING_BYTES) // Pack structs
#endif

// -----------------------------------------------------------
// VFS Header
// -----------------------------------------------------------

/**
 * @brief VFS (BRL) file header structure.
 *
 * Describes the layout of the virtual file system,
 * including file count, table offset, and data section offset.
 */
typedef struct BRLHeader {
    uint32_t sign;           ///< Signature (magic value identifying the archive)
    uint32_t ver;            ///< Parsed version of the archive
    uint64_t fileCount;      ///< Number of files in the archive
    uint64_t tableOffset;    ///< RVA to the file table
    uint64_t dataOffset;     ///< RVA to the start of data blobs
} BRLHeader;

// -----------------------------------------------------------
// VFS File Entry
// -----------------------------------------------------------

/**
 * @brief Represents a single file entry in the VFS.
 *
 * Each entry describes the location, size, and integrity
 * information of one stored file.
 */
typedef struct VFSFileEntry {
    uint64_t    nameHash;     ///< Precomputed hash of the filename for fast lookup
    uint64_t    dataRVA;      ///< RVA to the start of the file data
    uint64_t    dataSize;     ///< Size of the file in bytes
    uint64_t    dataChecksum; ///< Checksum for verifying file data integrity
} VFSFileEntry;

// -----------------------------------------------------------
// VFS File Data
// -----------------------------------------------------------

/**
 * @brief Represents a contiguous data blob in the VFS.
 *
 * The blob begins with its size field, followed by a flexible
 * array member containing the file’s actual bytes.
 */
typedef struct VFSDataBlob {
    uint64_t size;           ///< Size of this data blob in bytes
    uint8_t  data[];         ///< Flexible array member holding the file bytes
} VFSDataBlob;

// -----------------------------------------------------------
// VFS Chunked File Data
// -----------------------------------------------------------

/**
 * @brief Represents a cached chunk of VFS file data.
 *
 * Used to store partial data blobs for chunked reading and caching.
 */
typedef struct VFSDataBlobChunk {
    VFSDataBlob* blob;        ///< Pointer to the chunk's data blob
    uint64_t     baseRVA;     ///< RVA of this chunk’s start
    uint32_t     accessCount; ///< Number of times this chunk was accessed
    uint64_t     lastUsed;    ///< Timestamp or counter of last usage
} VFSDataBlobChunk;

// -----------------------------------------------------------
// VFS Context
// -----------------------------------------------------------

/**
 * @brief Main context structure for a loaded VFS archive.
 *
 * Holds pointers to mapped memory regions, header, file table,
 * and data regions for efficient access.
 */
typedef struct VFSContext {
    uint8_t*       base;          ///< Base pointer to the mapped archive memory
    uint64_t       size;          ///< Total size of the mapped region
    BRLHeader*     header;        ///< Pointer to the VFS header
    VFSFileEntry*  entries;       ///< Pointer to the array of file table entries
    uint8_t*       dataRegion;    ///< Pointer to the start of all data blobs
    VFSFileEntry** hashSlots;     ///< Array of entry pointers for hash-based lookup
    uint64_t       hashSlotCount; ///< Number of hash slots (must be a power of two)
} VFSContext;

// -----------------------------------------------------------
// VFS Chunked Context
// -----------------------------------------------------------

/**
 * @brief Context for chunked VFS access.
 *
 * Stores information for managing chunk-based reading,
 * including cache management and the read callback.
 */
typedef struct VFSChunkedContext {
    VFSReadCallback  readFunc;    ///< Callback used to read data chunks
    VFSContext*      context;     ///< Pointer to the VFS context for this archive
    void*            userData;    ///< User-defined data (e.g., file handle)
    uint64_t         archiveSize; ///< Total size of the archive file
    uint64_t         chunkSize;   ///< Size of each data chunk in bytes

    VFSDataBlobChunk chunks[VFS_MAX_CHUNKS]; ///< Fixed-size cache of loaded chunks
} VFSChunkedContext;

#ifndef BRL_DISABLE_STRUCT_PACKING
#pragma pack(pop) // End structs pack
#endif

#endif /* BRL_DEF */