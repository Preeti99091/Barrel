#include "barrel.h"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <argtable3.h>

#define BRL_ARRAY_LEN(x) (sizeof(x)/sizeof(x[0]))

static inline void PrintHexDump(const void *data, size_t size);

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

int main(const int argc, char* argv[]) {
    const char* progName = "dumpbrl";

    // -----------------------------
    // Argument table
    // -----------------------------
    struct arg_lit*  help       = arg_lit0("h", "help", "Display this help message");
    struct arg_file* input      = arg_file0("i", "input", "<file>", "Input archive file");
    struct arg_int*  chunkBytes = arg_int0("c", "chunk-size", "<n>", "Chunk size for reading (bytes)");
    struct arg_lit*  noData     = arg_lit0("n", "no-data", "Skip printing file contents");
    struct arg_file* extract    = arg_file0("e", "extract", "<file>", "Extract specified file");
    struct arg_str*  findName   = arg_str0("f", "find-by-name", "<name>", "Find file by name");
    struct arg_int*  findHash   = arg_int0(NULL, "find-by-hash", "<hash>", "Find file by hash");
    struct arg_end*  end        = arg_end(20);

    void* argtable[] = {help, input, chunkBytes, noData, extract, findName, findHash, end};

    if (arg_nullcheck(argtable) != 0) {
        fprintf(stderr, "%s: Insufficient memory for argtable\n", progName);
        return 1;
    }

    // -----------------------------
    // Parse arguments
    // -----------------------------
    const int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0) {
        printf("Usage: %s", progName);
        arg_print_syntax(stdout, argtable, "\n");
        arg_print_glossary(stdout, argtable, "  %-15s %s\n");
        arg_freetable(argtable, BRL_ARRAY_LEN(argtable));
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stdout, end, progName);
        fprintf(stderr, "Try '%s --help' for usage.\n", progName);
        arg_freetable(argtable, BRL_ARRAY_LEN(argtable));
        return 1;
    }

    if (input->count == 0) {
        fprintf(stderr, "Error: No input archive specified (-i <file> required).\n");
        arg_freetable(argtable, BRL_ARRAY_LEN(argtable));
        return 1;
    }

    // -----------------------------
    // Open input file
    // -----------------------------
    FILE* fp = fopen(input->filename[0], "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open archive: %s\n", input->filename[0]);
        arg_freetable(argtable, BRL_ARRAY_LEN(argtable));
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    const uint64_t archiveSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // -----------------------------
    // Create chunked VFS context
    // -----------------------------
    VFSChunkedContext chunkCtx = {0};
    const uint64_t chunkSize = (chunkBytes->count > 0) ? (uint64_t)chunkBytes->ival[0] : VFS_MBTOB(1);

    if (VFS_CreateChunkedContext(ReadFileCallback, fp, archiveSize, chunkSize, &chunkCtx) != BRL_OK) {
        fprintf(stderr, "Failed to create chunked context\n");
        fclose(fp);
        return 1;
    }

    const VFSContext* ctx = chunkCtx.context;
    if (!ctx) {
        fprintf(stderr, "Context invalid\n");
        fclose(fp);
        VFS_UnloadChunkedContext(&chunkCtx);
        return 1;
    }

    // -----------------------------
    // Print VFS header
    // -----------------------------
    if (ctx->header) {
        printf("Signature:  0x%08X\n", ctx->header->sign);
        printf("Version:    %u.%u.%u\n",
               (ctx->header->ver >> 24) & 0xFF,
               (ctx->header->ver >> 16) & 0xFF,
               (ctx->header->ver >> 0) & 0xFF);
        printf("File Count: %llu\n", ctx->header->fileCount);
        printf("Table RVA:  0x%llX\n", ctx->header->tableOffset);
        printf("Data RVA:   0x%llX\n\n", ctx->header->dataOffset);
    }

    if (VFS_CheckIntegrity(ctx->header) != BRLI_OK) {
        fprintf(stderr, "VFS integrity check failed, file is corrupt or not a Barrel VFS archive.\n");
        fclose(fp);
        VFS_UnloadChunkedContext(&chunkCtx);
        arg_freetable(argtable, BRL_ARRAY_LEN(argtable));
        return 1;
    }

    // -----------------------------
    // Find entry if requested
    // -----------------------------
    VFSFileEntry* entry = NULL;
    if (findName->count > 0) {
        entry = VFS_FindByName(ctx, findName->sval[0]);
        if (!entry) {
            fprintf(stderr, "File not found by name: %s\n", findName->sval[0]);
        }
    }

    if (findHash->count > 0) {
        VFSFileEntry* hashEntry = VFS_FindByHash(ctx, findHash->ival[0]);
        if (!hashEntry) {
            fprintf(stderr, "File not found by hash: 0x%d\n", findHash->ival[0]);
        } else {
            entry = hashEntry; // override only if found
        }
    }


    // -----------------------------
    // Display file entry/entries
    // -----------------------------
    if (!extract->count && !findName->count && !findHash->count) {
        for (uint64_t i = 0; i < ctx->header->fileCount; ++i) {
            const VFSFileEntry* e = &ctx->entries[i];
            printf("Entry %llu:\n", i);
            printf("  Hash:     0x%016llX\n", (unsigned long long)e->nameHash);
            printf("  Data RVA: 0x%016llX\n", (unsigned long long)e->dataRVA);
            printf("  Size:     %llu bytes\n", (unsigned long long)e->dataSize);
        }
    } else if ((findName->count > 0 || findHash->count > 0) && entry) {
        printf("Found Entry:\n");
        printf("  Hash:     0x%016llX\n", (unsigned long long)entry->nameHash);
        printf("  Data RVA: 0x%016llX\n", (unsigned long long)entry->dataRVA);
        printf("  Size:     %llu bytes\n", (unsigned long long)entry->dataSize);
    }

    // -----------------------------
    // Extract file if requested
    // -----------------------------
    if (extract->count > 0 && entry) {
        FILE* out = fopen(extract->filename[0], "wb");
        if (!out) {
            fprintf(stderr, "Failed to open output file: %s\n", extract->filename[0]);
            VFS_UnloadChunkedContext(&chunkCtx);
            fclose(fp);
            arg_freetable(argtable, BRL_ARRAY_LEN(argtable));
            return 1;
        }

        const uint64_t exceptedSize = VFS_GetDataSize(entry);
        const uint64_t totalChunks  = VFS_GetChunksCount(exceptedSize, chunkSize);
        uint64_t extractedBytes = 0;

        for (uint64_t chunkIndex = 0; chunkIndex < totalChunks; ++chunkIndex) {
            uint64_t actualSize = 0;
            const uint8_t* data = VFS_GetChunkedDataPtr(&chunkCtx, entry, &actualSize, chunkIndex);
            if (!data) {
                fprintf(stderr, "Failed to read chunk #%llu. **Partially extracted %llu bytes**.\n",
                        chunkIndex, extractedBytes);
                break;
            }

            if (actualSize == 0) {
                fprintf(stderr, "Warning: VFS returned 0 bytes for chunk #%llu. Stopping.\n", chunkIndex);
                break;
            }

            fwrite(data, 1, actualSize, out);
            extractedBytes += actualSize;
        }

        fclose(out);

        if (extractedBytes == exceptedSize) {
            printf("Successfully extracted %llu bytes to %s\n", extractedBytes, extract->filename[0]);
        } else {
            fprintf(stderr, "Extraction incomplete: expected %llu bytes, wrote %llu bytes to %s\n",
                    exceptedSize, extractedBytes, extract->filename[0]);
        }
    }

    // -----------------------------
    // Print file contents if requested
    // -----------------------------
    if (!noData->count && !extract->count && !findName->count && !findHash->count) {
        for (uint64_t i = 0; i < ctx->header->fileCount; ++i) {
            const VFSFileEntry* e = &ctx->entries[i];
            const uint64_t offsetCount = VFS_GetChunksCount(e->dataSize, chunkSize);
            if (offsetCount == 0) continue;

            for (uint64_t j = 0; j < offsetCount; ++j) {
                uint64_t localChunkSize = 0;
                const uint8_t* data = VFS_GetChunkedDataPtr(&chunkCtx, e, &localChunkSize, j * chunkSize);
                if (data) {
                    printf("Entry %llu: %llu bytes (chunk %llu loaded)\n", i, e->dataSize, localChunkSize);
                    PrintHexDump(data, localChunkSize);
                    printf("\n");
                }
            }
        }
    }

    // -----------------------------
    // Cleanup
    // -----------------------------
    VFS_UnloadChunkedContext(&chunkCtx);
    fclose(fp);
    arg_freetable(argtable, BRL_ARRAY_LEN(argtable));
    return 0;
}

static inline void PrintHexDump(const void *data, const size_t size) {
    const unsigned char *p = data;

    for (size_t i = 0; i < size; i += 16) {
        // Print offset
        printf("%08zx  ", i);

        // Print hex values
        for (size_t j = 0; j < 16; j++) {
            if (i + j < size) {
                printf("%02x ", p[i + j]);
            } else {
                printf("   ");
            }

            // Extra space between 8-byte groups
            if (j == 7) {
                printf(" ");
            }
        }

        printf(" ");

        // Print ASCII representation
        printf("|");
        for (size_t j = 0; j < 16 && i + j < size; j++) {
            unsigned char c = p[i + j];
            printf("%c", isprint(c) ? c : '.');
        }
        printf("|\n");
    }
}