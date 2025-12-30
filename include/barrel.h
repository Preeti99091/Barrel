/**
 * @file barrel.h
 * @brief Barrel core definitions and APIs.
 *
 * This file contains core functions, enums, and integrity utilities
 * for building, loading, and managing Barrel VFS archives.
 */

#ifndef BRL_BARREL
#define BRL_BARREL

#include "brlutil.h"
#include "brlbool.h"

/**
 * @enum VFSError
 * @brief Error codes for general VFS operations.
 */
typedef enum VFSError {
    BRL_OK,
    BRL_MEM_ALLOC_FAILED,
    BRL_IO_ERROR,
    BRL_INVALID_PARAM
} VFSError;

/**
 * @enum BRLIntegrity
 * @brief Error codes for Barrel VFS integrity checks.
 */
typedef enum BRLIntegrity {
    BRLI_OK,
    BRLI_SIGNATURE_MISMATCH,
    BRLI_VERSION_MISMATCH
} BRLIntegrity;

/**
 * @brief Builds the main VFS header for file serialization.
 *
 * @param fileCount     Number of files contained in the VFS
 * @param tableOffset   Offset (RVA) from the start of the VFS where the file table begins
 * @param dataOffset    Offset (RVA) from the start of the VFS where the data blobs begin
 * @param op_header     Pointer to a pre-allocated BRLHeader structure to populate
 */
void VFS_BuildHeader(
    uint64_t   fileCount,
    uint64_t   tableOffset,
    uint64_t   dataOffset,
    BRLHeader* op_header
);

/**
 * @brief Builds a VFS file entry for writing into the virtual file system.
 *
 * @param filename     Pointer to the filename
 * @param blobArray    Pointer to the VFSDataBlob containing the file's data
 * @param blobOffset   Offset (RVA) from the start of the VFS file where this blob will be written
 * @param op_entry     Pointer to a pre-allocated VFSFileEntry to be populated
 */
void VFS_BuildEntry(
    const char*          filename,
    const VFSDataBlob*   blobArray,
    uint64_t             blobOffset,
    VFSFileEntry*        op_entry
);

/**
 * @brief Allocates and builds a VFSDataBlob.
 *
 * @param data          Pointer to the source file data
 * @param dataSize      Size of the data in bytes
 * @return              Pointer to a newly allocated VFSDataBlob (must free later)
 */
VFSDataBlob* VFS_BuildDataBlob(
    const uint8_t*  data,
    uint64_t        dataSize
);

/**
 * @brief Initializes a VFS context from an existing memory buffer.
 *
 * @param buffer    Pointer to the raw VFS data in memory.
 * @param size      Size of the VFS data in bytes.
 * @param op_ctx    Output pointer to the initialized context.
 */
VFSError VFS_CreateContext(
    const uint8_t* buffer,
    uint64_t       size,
    VFSContext*    op_ctx
);

/**
 * @brief Creates a chunked VFS context for reading large archives in manageable memory blocks.
 *
 * This function initializes a VFSChunkedContext, allowing access to
 * portions of an archive without loading the entire file into memory.
 *
 * @param readFunc      Callback used to read chunks from the underlying archive.
 * @param userData      Pointer to user-defined data (e.g., FILE* handle).
 * @param archiveSize   Total size of the archive in bytes.
 * @param chunkSize     Maximum chunk size in bytes (>0).
 * @param op_ctx        Output pointer to the initialized VFSChunkedContext.
 *
 * @return VFSError    Returns BRL_OK if successful or an error code.
 */
VFSError VFS_CreateChunkedContext(
    VFSReadCallback    readFunc,
    void*              userData,
    uint64_t           archiveSize,
    uint64_t           chunkSize,
    VFSChunkedContext* op_ctx
);

/**
 * @brief Checks the integrity of a Barrel VFS header.
 *
 * @param header   Pointer to the VFS header to check.
 * @return         Integrity status code.
 */
BRLIntegrity VFS_CheckIntegrity(
    const BRLHeader* header
);

/**
 * @brief Checks the integrity of a VFS entry by comparing its checksum.
 *
 * @param entry    Pointer to the VFS entry.
 * @param data     Pointer to the entry's data.
 * @return         true if valid, false otherwise.
 */
bool VFS_CheckEntryIntegrity(
    const VFSFileEntry* entry,
    const uint8_t*      data
);

/**
 * @brief Frees memory owned by a VFS context, including the provided base buffer.
 *
 * @param ctx  Pointer to the context to free.
 */
void VFS_UnloadContext(
    VFSContext* ctx
);

/**
 * @brief Fully unloads and frees all data owned by a chunked VFS context, including the provided base buffer.
 *
 * @param ctx   Pointer to an initialized VFSChunkedContext.
 */
void VFS_UnloadChunkedContext(
    VFSChunkedContext* ctx
);

/**
 * @brief Clears all loaded chunks in a chunked VFS context without unloading metadata.
 *
 * @param ctx   Pointer to an initialized VFSChunkedContext.
 */
void VFS_ClearChunkCache(
    VFSChunkedContext* ctx
);

#endif /* BRL_BARREL */