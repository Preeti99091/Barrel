#include "barrel.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <argtable3.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

#define BRL_ARRAY_LEN(x) (sizeof(x)/sizeof(x[0]))
#define MAX_PATH_LEN 1024

typedef struct {
    char* fullPath;     // For fopen()
    char* relativePath; // For storing inside the VFS
} FilePath;

static int CollectFilesRecursive(
    const char* basePath,
    const char* relativePath,
    FilePath**  files,
    int*        count,
    int*        capacity
);

int main(const int argc, char* argv[]) {
    const char *progName = "buildbrl";

    // -----------------------------
    // Argument table
    // -----------------------------
    struct arg_lit*  help   = arg_lit0("h", "help", "Display this help message");
    struct arg_file* input  = arg_filen(NULL, NULL, "<file>", 1, 100, "Input files (one or more)");
    struct arg_file* output = arg_file0("o", "output", "<file>", "Output file");
    struct arg_end*  end    = arg_end(10);

    void* argtable[] = {help, input, output, end};

    if (arg_nullcheck(argtable) != 0) {
        fprintf(stderr, "%s: Insufficient memory for argtable\n", progName);
        return 1;
    }

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
        fprintf(stderr, "Error: No input files or directories provided.\n");
        arg_freetable(argtable, BRL_ARRAY_LEN(argtable));
        return 1;
    }
    if (output->count == 0) {
        fprintf(stderr, "Error: No output file provided (-o <file> expected).\n");
        arg_freetable(argtable, BRL_ARRAY_LEN(argtable));
        return 1;
    }

    // -----------------------------
    // Collect all files recursively
    // -----------------------------
    FilePath* allFiles = NULL;
    int allCount       = 0;
    int allCap         = 16;
    allFiles           = malloc(allCap * sizeof(FilePath));

    for (int i = 0; i < input->count; i++) {
        const char *path = input->filename[i];
        if (CollectFilesRecursive(path, "", &allFiles, &allCount, &allCap) <= 0) {
            fprintf(stderr, "Error: Failed to scan input path '%s'\n", path);
            arg_freetable(argtable, BRL_ARRAY_LEN(argtable));
            return 1;
        }
    }

    const int fileCount        = allCount;
    const uint64_t tableOffset = sizeof(BRLHeader);
    const uint64_t dataOffset  = tableOffset + fileCount * sizeof(VFSFileEntry);
    uint64_t currentOffset     = dataOffset;

    VFSDataBlob** blobs   = calloc(fileCount, sizeof(VFSDataBlob*));
    VFSFileEntry* entries = calloc(fileCount, sizeof(VFSFileEntry));
    if (!blobs || !entries) {
        fprintf(stderr, "%s: Out of memory\n", progName);
        goto cleanup;
    }

    // -----------------------------
    // Process input files
    // -----------------------------
    for (int i = 0; i < fileCount; i++) {
        const char* diskPath = allFiles[i].fullPath;
        const char* vfsName  = allFiles[i].relativePath;

        FILE *file = fopen(diskPath, "rb");
        if (!file) {
            perror(diskPath);
            goto cleanup;
        }

        if (fseek(file, 0, SEEK_END) != 0) {
            perror("fseek");
            fclose(file);
            goto cleanup;
        }
        const long size = ftell(file);
        rewind(file);

        if (size <= 0) {
            fprintf(stderr, "Error: File '%s' is empty or unreadable.\n", diskPath);
            fclose(file);
            goto cleanup;
        }

        uint8_t* fileBuffer = malloc(size);
        if (!fileBuffer) {
            fprintf(stderr, "Out of memory while reading '%s'\n", diskPath);
            fclose(file);
            goto cleanup;
        }

        if (fread(fileBuffer, 1, size, file) != (size_t)size) {
            fprintf(stderr, "Error reading file '%s'\n", diskPath);
            free(fileBuffer);
            fclose(file);
            goto cleanup;
        }
        fclose(file);

        blobs[i] = VFS_BuildDataBlob(fileBuffer, size);
        if (!blobs[i]) {
            fprintf(stderr, "Failed to build data blob for '%s'\n", diskPath);
            free(fileBuffer);
            goto cleanup;
        }

        VFS_BuildEntry(vfsName, blobs[i], currentOffset, &entries[i]);
        currentOffset += blobs[i]->size;

        free(fileBuffer);
    }

    // -----------------------------
    // Write output file
    // -----------------------------
    FILE* outFile = fopen(output->filename[0], "wb");
    if (!outFile) {
        perror(output->filename[0]);
        goto cleanup;
    }

    BRLHeader header;
    VFS_BuildHeader(fileCount, tableOffset, dataOffset, &header);

    fwrite(&header, sizeof(header), 1, outFile);
    fwrite(entries, sizeof(VFSFileEntry), fileCount, outFile);
    for (int i = 0; i < fileCount; i++)
        fwrite(blobs[i]->data, 1, blobs[i]->size, outFile);

    fclose(outFile);
    printf("VFS built successfully: %s\n", output->filename[0]);

cleanup:
    if (blobs) {
        for (int i = 0; i < fileCount; i++) {
            free(blobs[i]);
        }
    }
    free(blobs);
    free(entries);

    for (int i = 0; i < allCount; i++) {
        free(allFiles[i].fullPath);
        free(allFiles[i].relativePath);
    }
    free(allFiles);

    arg_freetable(argtable, BRL_ARRAY_LEN(argtable));
    return 0;
}

// Recursively collect files under basePath, storing full + relative paths
static int CollectFilesRecursive(
    const char* basePath,
    const char* relativePath,
    FilePath**  files,
    int*        count,
    int*        capacity
) {
    char fullPath[MAX_PATH_LEN];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", basePath, relativePath[0] ? relativePath : "");

    DIR *dir = opendir(fullPath);
    if (!dir) {
        struct stat st;
        if (stat(fullPath, &st) == 0 && S_ISREG(st.st_mode)) {
            if (*count >= *capacity) {
                *capacity *= 2;
                *files = realloc(*files, *capacity * sizeof(FilePath));
            }
            (*files)[*count].fullPath     = strdup(fullPath);
            (*files)[*count].relativePath = strdup(relativePath[0] ? relativePath : basePath);
            (*count)++;
        }
        return 0;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char nextRelative[MAX_PATH_LEN];
        if (relativePath[0]) {
            snprintf(nextRelative, sizeof(nextRelative), "%s/%s", relativePath, entry->d_name);
        } else {
            snprintf(nextRelative, sizeof(nextRelative), "%s", entry->d_name);
        }

        char nextFull[MAX_PATH_LEN];
        snprintf(nextFull, sizeof(nextFull), "%s/%s", basePath, nextRelative);

        struct stat st;
        if (stat(nextFull, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                CollectFilesRecursive(basePath, nextRelative, files, count, capacity);
            } else if (S_ISREG(st.st_mode)) {
                if (*count >= *capacity) {
                    *capacity *= 2;
                    *files    = realloc(*files, *capacity * sizeof(FilePath));
                }
                (*files)[*count].fullPath     = strdup(nextFull);
                (*files)[*count].relativePath = strdup(nextRelative);
                (*count)++;
            }
        }
    }

    closedir(dir);
    return *count;
}