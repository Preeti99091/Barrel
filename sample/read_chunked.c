#include "barrel.h"
#include <stdio.h>

uint64_t ReadFileCallback(
    void* userData,
    void* buffer,
    const uint64_t offset,
    const uint64_t size
) {
    FILE* fp = userData;
    fseek(fp, offset, SEEK_SET);
    return fread(buffer, 1, size, fp);
}

int main() {
    FILE* fp = fopen("example.brl", "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open archive\n");
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    const uint64_t archiveSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    VFSChunkedContext chunkCtx = {0};
    const uint64_t chunkSize = VFS_MBTOB(16);

    if (VFS_CreateChunkedContext(
            ReadFileCallback,
            fp,
            archiveSize,
            chunkSize,
            &chunkCtx
        ) != BRL_OK) {
        fprintf(stderr, "Failed to create chunked context\n");
        fclose(fp);
        return 1;
    }

    const VFSContext* ctx = chunkCtx.context;
    if (!ctx) {
        fprintf(stderr, "Context invalid\n");
        return 1;
    }

    // Display header info
    if (ctx->header) {
        printf("Signature:  0x%08X\n", ctx->header->sign);
        printf("Version:    %u.%u.%u\n",
            (ctx->header->ver >> 24) & 0xFF,
            (ctx->header->ver >> 16) & 0xFF,
            (ctx->header->ver >>  0) & 0xFF);
        printf("File Count: %llu\n", ctx->header->fileCount);
        printf("Table RVA:  0x%llX\n", ctx->header->tableOffset);
        printf("Data RVA:   0x%llX\n", ctx->header->dataOffset);
        printf("\n");
    }

    if (VFS_CheckIntegrity(ctx->header) != BRLI_OK) {
        printf("VFS integrity check failed.\n");
        return 1;
    }

    // Display file entries
    for (uint64_t i = 0; i < ctx->header->fileCount; ++i) {
        const VFSFileEntry* e = &ctx->entries[i];
        printf("Entry %llu:\n", i);
        printf("  Hash:     0x%016llX\n", (unsigned long long)e->nameHash);
        printf("  Data RVA: 0x%016llX\n", (unsigned long long)e->dataRVA);
        printf("  Size:     %llu bytes\n", (unsigned long long)e->dataSize);
    }

    const VFSFileEntry* textFile = VFS_FindByName(ctx, "hello.txt");
    if (textFile) {
        uint64_t localChunkSize = 0;
        const uint8_t* data = VFS_GetChunkedDataPtr(&chunkCtx, textFile, &localChunkSize, 0);
        if (data) {
            printf("Found file: %llu bytes (chunked, first %llu bytes loaded)\n",
                   textFile->dataSize, localChunkSize);
            fwrite(data, 1, localChunkSize, stdout);
            printf("\n");
        } else {
            printf("Failed to read hello.txt: chunked data pointer is null\n");
        }
    }

    const VFSFileEntry* binFile = VFS_FindByName(ctx, "data.bin");
    if (binFile) {
        uint64_t localChunkSize = 0;
        const uint8_t* data = VFS_GetChunkedDataPtr(&chunkCtx, binFile, &localChunkSize, 0);
        if (data) {
            printf("Found file: %llu bytes (chunked, first %llu bytes loaded)\n",
                   binFile->dataSize, localChunkSize);
            for (uint64_t i = 0; i < localChunkSize; ++i) {
                printf("0x%02X\n", data[i]);
            }
            printf("\n");
        } else {
            printf("Failed to read data.bin: chunked data pointer is null\n");
        }
    }

    VFS_UnloadChunkedContext(&chunkCtx);
}