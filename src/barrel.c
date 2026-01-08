#include "barrel.h"
#include "brlalg.h"
#include "brlstdlib.h"

BRLAPI void VFS_BuildHeader(
    const uint64_t fileCount,
    const uint64_t tableOffset,
    const uint64_t dataOffset,
    BRLHeader*     op_header
) {
    op_header->sign        = BRL_SIGNATURE;
    op_header->ver         = BRL_VER;
    op_header->fileCount   = fileCount;
    op_header->tableOffset = tableOffset;
    op_header->dataOffset  = dataOffset;
}

BRLAPI void VFS_BuildEntry(
    const char*          filename,
    const VFSDataBlob*   blobArray,
    const uint64_t       blobOffset,
    VFSFileEntry*        op_entry
) {
    op_entry->nameHash     = BRL_Hash64(filename, BRL_STRLEN(filename));
    op_entry->dataRVA      = blobOffset;
    op_entry->dataSize     = blobArray->size;
    op_entry->dataChecksum = BRL_Hash64(blobArray->data, blobArray->size);
}

BRLAPI VFSDataBlob* VFS_BuildDataBlob(
    const uint8_t*  data,
    const uint64_t  dataSize
) {
    if (!data || dataSize == 0) {
        return NULL;
    }

    VFSDataBlob* blob = BRL_MALLOC(sizeof(VFSDataBlob) + dataSize);
    if (!blob) {
        return NULL;
    }

    blob->size = dataSize;
    BRL_MEMCPY(blob->data, data, dataSize);

    return blob;
}

BRLAPI VFSError VFS_CreateContext(
    const uint8_t* buffer,
    const uint64_t size,
    VFSContext*    op_ctx
) {
    if (!buffer || size == 0 || !op_ctx) {
        return BRL_INVALID_PARAM;
    }

    op_ctx->base       = (uint8_t*)buffer;
    op_ctx->size       = size;
    op_ctx->header     = (BRLHeader*)op_ctx->base;
    op_ctx->entries    = (VFSFileEntry*)(op_ctx->base + op_ctx->header->tableOffset);
    op_ctx->dataRegion = op_ctx->base + op_ctx->header->dataOffset;

    // Create hash table
    op_ctx->hashSlotCount = 1ULL;
    while (op_ctx->hashSlotCount < (op_ctx->header->fileCount * 2))
        op_ctx->hashSlotCount <<= 1;

    op_ctx->hashSlots = BRL_MALLOC(op_ctx->hashSlotCount * sizeof(VFSFileEntry*));
    if (!op_ctx->hashSlots) {
        BRL_FREE(op_ctx->base);
        op_ctx->base = NULL;
        return BRL_MEM_ALLOC_FAILED;
    }

    BRL_MEMSET(op_ctx->hashSlots, 0, op_ctx->hashSlotCount * sizeof(VFSFileEntry*));

    // Populate hash slots
    for (uint64_t i = 0; i < op_ctx->header->fileCount; i++) {
        const uint64_t h = op_ctx->entries[i].nameHash;
        uint64_t slot = h & (op_ctx->hashSlotCount - 1);
        while (op_ctx->hashSlots[slot]) {
            slot = (slot + 1) & (op_ctx->hashSlotCount - 1);
        }
        op_ctx->hashSlots[slot] = &op_ctx->entries[i];
    }

    return BRL_OK;
}

BRLAPI VFSError VFS_CreateChunkedContext(
    const VFSReadCallback readFunc,
    void*                 userData,
    const uint64_t        archiveSize,
    const uint64_t        chunkSize,
    VFSChunkedContext*    op_ctx
) {
    if (!readFunc || !op_ctx || archiveSize == 0 || chunkSize == 0) {
        return BRL_INVALID_PARAM;
    }

    op_ctx->readFunc    = readFunc;
    op_ctx->userData    = userData;
    op_ctx->archiveSize = archiveSize;
    op_ctx->chunkSize   = chunkSize;

    // Allocate context
    op_ctx->context = (VFSContext*)BRL_MALLOC(sizeof(VFSContext));
    if (!op_ctx->context) {
        return BRL_MEM_ALLOC_FAILED;
    }

    BRL_MEMSET(op_ctx->context, 0, sizeof(VFSContext));

    // Load header
    uint8_t headerBuffer[sizeof(BRLHeader)];
    const uint64_t readBytes = readFunc(userData, headerBuffer, 0, sizeof(BRLHeader));
    if (readBytes != sizeof(BRLHeader)) {
        BRL_FREE(op_ctx->context);
        op_ctx->context = NULL;
        return BRL_IO_ERROR;
    }

    const BRLHeader* header = (const BRLHeader*)headerBuffer;
    const size_t tableSize  = header->fileCount * sizeof(VFSFileEntry);

    // Allocate buffer for header + table
    uint8_t* buffer = (uint8_t*)BRL_MALLOC(sizeof(BRLHeader) + tableSize);
    if (!buffer) {
        BRL_FREE(op_ctx->context);
        op_ctx->context = NULL;
        return BRL_MEM_ALLOC_FAILED;
    }

    BRL_MEMCPY(buffer, headerBuffer, sizeof(BRLHeader));

    const uint64_t tableRead = readFunc(userData, buffer + header->tableOffset, header->tableOffset, tableSize);
    if (tableRead != tableSize) {
        BRL_FREE(buffer);
        BRL_FREE(op_ctx->context);
        op_ctx->context = NULL;
        return BRL_IO_ERROR;
    }

    // Use VFS_CreateContext helper
    const VFSError err = VFS_CreateContext(buffer, sizeof(BRLHeader) + tableSize, op_ctx->context);
    if (err != BRL_OK) {
        BRL_FREE(op_ctx->context);
        op_ctx->context = NULL;
        return err;
    }

    // Mark chunked (no full data region)
    op_ctx->context->base       = NULL;
    op_ctx->context->dataRegion = NULL;
    op_ctx->context->size       = archiveSize;

    return BRL_OK;
}

BRLAPI BRLIntegrity VFS_CheckIntegrity(
    const BRLHeader* header
) {
    if (header->sign != BRL_SIGNATURE) {
        return BRLI_SIGNATURE_MISMATCH;
    }

    const uint8_t hdrMajor = BRL_VER_GET_MAJOR(header->ver);
    const uint8_t hdrMinor = BRL_VER_GET_MINOR(header->ver);
    const uint8_t libMajor = BRL_VER_GET_MAJOR(BRL_VER);
    const uint8_t libMinor = BRL_VER_GET_MINOR(BRL_VER);

    if (hdrMajor != libMajor || hdrMinor != libMinor) {
        return BRLI_VERSION_MISMATCH;
    }

    return BRLI_OK;
}

BRLAPI bool VFS_CheckEntryIntegrity(
    const VFSFileEntry* entry,
    const uint8_t*      data
) {
    if (!entry || !data) {
        return false;
    }

    const uint64_t origChecksum = entry->dataChecksum;
    const uint64_t currChecksum = BRL_Hash64(data, entry->dataSize);
    return currChecksum == origChecksum;
}

BRLAPI void VFS_UnloadContext(
    VFSContext* ctx
) {
    if (!ctx) return;

    if (ctx->base) {
        BRL_FREE(ctx->base);
        ctx->base = NULL;
    }

    if (ctx->hashSlots) {
        BRL_FREE(ctx->hashSlots);
        ctx->hashSlots = NULL;
    }

    ctx->header     = NULL;
    ctx->entries    = NULL;
    ctx->dataRegion = NULL;
    ctx->size       = 0;
}

BRLAPI void VFS_UnloadChunkedContext(
    VFSChunkedContext* ctx
) {
    if (!ctx) return;

    for (int i = 0; i < VFS_MAX_CHUNKS; i++) {
        if (ctx->chunks[i].blob) {
            BRL_FREE(ctx->chunks[i].blob);
            ctx->chunks[i].blob = NULL;
        }
    }

    if (ctx->context) {
        VFS_UnloadContext(ctx->context);
        BRL_FREE(ctx->context);
        ctx->context = NULL;
    }
}

BRLAPI void VFS_ClearChunkCache(
    VFSChunkedContext* ctx
) {
    if (!ctx) return;

    for (int i = 0; i < VFS_MAX_CHUNKS; i++) {
        if (ctx->chunks[i].blob) {
            BRL_FREE(ctx->chunks[i].blob);
            ctx->chunks[i].blob        = NULL;
            ctx->chunks[i].baseRVA     = 0;
            ctx->chunks[i].accessCount = 0;
            ctx->chunks[i].lastUsed    = 0;
        }
    }
}
