#include "barrel.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    // Some static files
    const char* filenames[] = {
        "hello.txt",
        "data.bin"
    };

    const uint8_t file1Data[] = "Hello, Virtual File System!";
    const uint8_t file2Data[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    // Build blobs
    VFSDataBlob* blobs[2];
    blobs[0] = VFS_BuildDataBlob(
        file1Data, sizeof(file1Data)
    );
    blobs[1] = VFS_BuildDataBlob(
        file2Data, sizeof(file2Data)
    );

    // Prepare entries
    VFSFileEntry entries[2];

    uint64_t numFiles    = 2;
    uint64_t tableOffset = sizeof(BRLHeader);
    uint64_t dataOffset  = tableOffset + numFiles * sizeof(VFSFileEntry);

    // Build header
    BRLHeader header;
    VFS_BuildHeader(numFiles, tableOffset, dataOffset, &header);

    // Build entries and compute offsets
    uint64_t currentOffset = dataOffset;
    for (uint64_t i = 0; i < numFiles; i++) {
        VFS_BuildEntry(
            filenames[i],
            blobs[i],
            currentOffset,
            &entries[i]
        );
        currentOffset += blobs[i]->size;
    }

    // Write to file
    FILE* fp = fopen("example.brl", "wb");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    fwrite(&header, sizeof(header), 1, fp);
    fwrite(entries, sizeof(VFSFileEntry), numFiles, fp);
    for (uint64_t i = 0; i < numFiles; i++) {
        // NOTE: ALWAYS write the data of the blobs, not the blob itself.
        fwrite(blobs[i]->data, 1, blobs[i]->size, fp);
    }

    fclose(fp);
    printf("VFS built successfully: example.vfs\n");

    // Free blobs
    for (uint64_t i = 0; i < numFiles; i++) {
        free(blobs[i]);
    }
}