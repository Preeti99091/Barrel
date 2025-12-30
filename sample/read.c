#include "barrel.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    FILE* fp = fopen("example.brl", "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open archive\n");
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    const size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Allocate file buffer, must not be freed until the
    // context is destroyed
    uint8_t* buffer = malloc(size);
    if (!buffer) {
        fprintf(stderr, "Out of memory\n");
        fclose(fp);
        return 1;
    }

    fread(buffer, 1, size, fp);
    fclose(fp);

    // Create context
    VFSContext ctx = {0};
    if (VFS_CreateContext(buffer, size, &ctx) != BRL_OK) {
        fprintf(stderr, "Failed to create context\n");
        return 1;
    }

    // Display header info
    if (ctx.header) {
        printf("Signature:  0x%08X\n", ctx.header->sign);
        printf("Version:    %u.%u.%u\n",
            (ctx.header->ver >> 24) & 0xFF,
            (ctx.header->ver >> 16) & 0xFF,
            (ctx.header->ver >>  0) & 0xFF);
        printf("File Count: %llu\n", ctx.header->fileCount);
        printf("Table RVA:  0x%llX\n", ctx.header->tableOffset);
        printf("Data RVA:   0x%llX\n", ctx.header->dataOffset);
        printf("\n");
    }

    if (VFS_CheckIntegrity(ctx.header) != BRLI_OK) {
        printf("VFS integrity check failed.\n");
        return 1;
    }

    // Display file entries
    for (uint64_t i = 0; i < ctx.header->fileCount; ++i) {
        const VFSFileEntry* e = &ctx.entries[i];
        printf("Entry %llu:\n", i);
        printf("  Hash:     0x%016llX\n", (unsigned long long)e->nameHash);
        printf("  Data RVA: 0x%016llX\n", (unsigned long long)e->dataRVA);
        printf("  Size:     %llu bytes\n", (unsigned long long)e->dataSize);
    }

    const VFSFileEntry* textFile = VFS_FindByName(&ctx, "hello.txt");
    if (textFile) {
        const void* data = VFS_GetDataPtr(&ctx, textFile);
        if (data) {
            printf("Found file: %llu bytes\n", textFile->dataSize);
            printf("Integrity Check: %d\n", VFS_CheckEntryIntegrity(
                    textFile,
                    data)
                );
            fwrite(data, 1, textFile->dataSize, stdout);
            printf("\n");
        } else {
            printf("Failed to unpack hello.txt: data pointer is null\n");
        }
    }

    const VFSFileEntry* binFile = VFS_FindByName(&ctx, "data.bin");
    if (binFile) {
        const uint8_t* data = VFS_GetDataPtr(&ctx, binFile);
        if (data) {
            printf("Found file: %llu bytes\n", binFile->dataSize);
            printf("Integrity Check: %d\n", VFS_CheckEntryIntegrity(
                    binFile,
                    data)
                );
            for (uint64_t i = 0; i < binFile->dataSize; ++i) {
                printf("0x%02X\n", data[i]);
            }
            printf("\n");
        } else {
            printf("Failed to unpack data.bin: data pointer is null\n");
        }
    }

    VFS_UnloadContext(&ctx);
}