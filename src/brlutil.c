#include "brlutil.h"
#include "brlalg.h"
#include "brlstdlib.h"

// global tick counter for LRU, used in chunked data handling
static uint64_t g_lruTick = 0;

BRLAPI void* VFS_GetDataPtr(
    const VFSContext*   ctx,
    const VFSFileEntry* entry
) {
    return ctx->base + entry->dataRVA;
}

BRLAPI uint64_t VFS_GetDataSize(
    const VFSFileEntry* entry
) {
     return entry->dataSize;
}

BRLAPI VFSFileEntry* VFS_FindByHash(
    const VFSContext* ctx,
    const uint64_t    hash
) {
    if (!ctx->hashSlots) {
        return NULL;
    }

    uint64_t slot = hash & (ctx->hashSlotCount - 1);
    for (uint64_t i = 0; i < ctx->hashSlotCount; i++) {
        VFSFileEntry* entry = ctx->hashSlots[slot];
        if (!entry) {
            return NULL;
        }

        if (entry->nameHash == hash) {
            return entry;
        }

        slot = (slot + 1) & (ctx->hashSlotCount - 1);
    }

    return NULL;
}

BRLAPI VFSFileEntry* VFS_FindByName(
    const VFSContext* ctx,
    const char*       name
) {
    const uint64_t hash = BRL_Hash64(name, BRL_STRLEN(name));
    return VFS_FindByHash(ctx, hash);
}

BRLAPI uint8_t* VFS_GetChunkedDataPtr(
    VFSChunkedContext*  ctx,
    const VFSFileEntry* entry,
    uint64_t*           outSize,
    const uint64_t      chunkIdx
) {
    if (!ctx || !entry) {
        return NULL;
    }

    // Calculate absolute file offset for this chunk
    const uint64_t rva = entry->dataRVA + (chunkIdx * ctx->chunkSize);
    if (rva >= ctx->archiveSize) {
        return NULL;
    }

    VFSDataBlobChunk* selected = NULL;

    // Try to find an already loaded chunk
    for (int i = 0; i < VFS_MAX_CHUNKS; i++) {
        if (ctx->chunks[i].blob && ctx->chunks[i].baseRVA == rva) {
            selected = &ctx->chunks[i];
            break;
        }
    }

    // Load chunk if not cached
    if (!selected) {
        int slot = 0;
        uint64_t oldest = UINT64_MAX;

        // Find a free or least recently used slot
        for (int i = 0; i < VFS_MAX_CHUNKS; i++) {
            if (!ctx->chunks[i].blob) {
                slot = i;
                break;
            }
            if (ctx->chunks[i].lastUsed < oldest) {
                oldest = ctx->chunks[i].lastUsed;
                slot = i;
            }
        }

        // Free old chunk if present
        if (ctx->chunks[slot].blob) {
            BRL_FREE(ctx->chunks[slot].blob);
            ctx->chunks[slot].blob = NULL;
        }

        // Determine how many bytes to read
        uint64_t thisChunkSize = ctx->chunkSize;
        if (rva + thisChunkSize > ctx->archiveSize) {
            thisChunkSize = ctx->archiveSize - rva;
        }

        VFSDataBlob* blob = BRL_MALLOC(sizeof(VFSDataBlob) + thisChunkSize);
        if (!blob) {
            return NULL;
        }

        blob->size = thisChunkSize;

        const uint64_t readBytes = ctx->readFunc(
            ctx->userData,
            blob->data,
            rva,
            thisChunkSize
        );

        if (readBytes != thisChunkSize) {
            BRL_FREE(blob);
            return NULL;
        }

        ctx->chunks[slot].blob        = blob;
        ctx->chunks[slot].baseRVA     = rva;
        ctx->chunks[slot].accessCount = 1;
        ctx->chunks[slot].lastUsed    = g_lruTick++;

        selected = &ctx->chunks[slot];
    } else {
        selected->accessCount++;
        selected->lastUsed = g_lruTick++;
    }

    // Compute how many bytes remain for this file portion
    if (outSize) {
        *outSize = selected->blob->size;

        const uint64_t fileOffset = chunkIdx * ctx->chunkSize;
        const uint64_t remaining  = (entry->dataSize > fileOffset)
            ? (entry->dataSize - fileOffset)
            : 0;

        if (*outSize > remaining) {
            *outSize = remaining;
        }
    }

    return selected->blob->data;
}

BRLAPI uint64_t VFS_GetChunksCount(
    const uint64_t fileSize,
    const uint64_t chunkSize
) {
    return (fileSize + chunkSize - 1) / chunkSize;
}
