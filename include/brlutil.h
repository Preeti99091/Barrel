/**
 * @file brlutil.h
 * @brief Barrel utility helper functions.
 */

#ifndef BRL_UTIL
#define BRL_UTIL

#include "brldef.h"

/// @brief Converts megabytes to bytes. @param mb Size in megabytes.
#define VFS_MBTOB(mb)  ((mb) * 1024 * 1024)


/**
 * @brief Returns a pointer to a file's data within the mapped VFS memory.
 *
 * @param ctx    Pointer to initialized VFS context
 * @param entry  Pointer to VFSFileEntry
 * @return       Pointer to the file's raw bytes
 */
void* VFS_GetDataPtr(
    const VFSContext* ctx,
    const VFSFileEntry* entry
);

/**
 * @brief Returns the file size in bytes.
 *
 * @param entry Pointer to the VFSFileEntry
 * @return File size in bytes.
 */
uint64_t VFS_GetDataSize(
    const VFSFileEntry* entry
);

/**
 * @brief Finds a file entry in the VFS by its hash.
 *
 * @param ctx   Pointer to initialized VFS context
 * @param hash  Hash of the filename
 * @return      Pointer to VFSFileEntry or NULL if not found
 */
VFSFileEntry* VFS_FindByHash(
    const VFSContext* ctx,
    uint64_t          hash
);

/**
 * @brief Finds a file entry in the VFS by its name.
 *
 * @param ctx   Pointer to initialized VFS context
 * @param name  Original filename
 * @return      Pointer to VFSFileEntry or NULL if not found
 */
VFSFileEntry* VFS_FindByName(
    const VFSContext* ctx,
    const char*       name
);

/**
 * @brief Retrieves a pointer to a file's data within a chunked archive context.
 *
 * @param ctx       Pointer to an initialized VFSChunkedContext.
 * @param entry     Pointer to the file entry.
 * @param outSize   Optional pointer to store accessible size in bytes.
 * @param chunkIdx  Byte offset from the file start.
 *
 * @return Pointer to the data or NULL on error.
 */
uint8_t* VFS_GetChunkedDataPtr(
    VFSChunkedContext*  ctx,
    const VFSFileEntry* entry,
    uint64_t*           outSize,
    uint64_t            chunkIdx
);

/**
 * @brief Computes the total number of chunks required to store a file.
 *
 * @param fileSize   File size in bytes.
 * @param chunkSize  Chunk size in bytes.
 * @return           Total number of chunks (0 if chunkSize == 0).
 */
uint64_t VFS_GetChunksCount(
    uint64_t fileSize,
    uint64_t chunkSize
);

#endif /* BRL_UTIL */