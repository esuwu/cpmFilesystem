#include "cpmfsys.h"
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#define UNUSED 0xe5
static bool freeList[256];

// This function reads the first block (super block) from the disk and lists all the files stored there and their size
void cpmDir() {
    uint8_t block_zero[1024];  // Assuming block size of 1024 bytes
    blockRead(block_zero, 0);  // Read the directory block (block 0)

    for (int i = 0; i < 32; i++) { // 32 dir entries in the super block (block 0)
        DirStructType *cpm_dir = mkDirStruct(i, block_zero);

        if (cpm_dir && cpm_dir->status > 0 && cpm_dir->status != UNUSED) { // file status between 1 and 15
            int block_number = 0;
            for (int b_index = 0; b_index < 16; b_index++) {
                if (cpm_dir->blocks[b_index] != 0) {
                    block_number++;
                }
            }

            // compute the file length
            int RC = cpm_dir->RC; // number of sectors in the last block
            int BC = cpm_dir->BC; // number of bytes in the last sector
            int file_length = (block_number - 1) * 1024 + RC * 128 + BC;

            printf("%s.%s %d\n", cpm_dir->name, cpm_dir->extension, file_length);
        }
        free(cpm_dir);
    }
}

DirStructType *mkDirStruct(int index, uint8_t *super_block_entry) {
    // validate input pointers
    if (!super_block_entry || index < 0 || index >= 32) {
        fprintf(stderr, "invalid arguments for mkDirStruct\n");
        return NULL;
    }
    // get the dir entry address
    uint8_t *dir_addr = super_block_entry + index * EXTENT_SIZE;
    DirStructType *dir = (DirStructType *) malloc((sizeof(DirStructType)));
    if (dir == NULL) {
        fprintf(stderr, "failed to allocate memory for DirStructType");
        return NULL;
    }
    // clear the dir variable
    memset(dir, 0, sizeof(DirStructType));
    // 1) copy status
    dir->status = dir_addr[0];
    // 2) copy the file name
    strncpy(dir->name, (char *) (dir_addr + 1), 8);
    dir->name[8] = '\0';

    // 3) Obtain XL, BC, XH, RC values
    dir->XL = dir_addr[12];
    dir->BC = dir_addr[13];
    dir->XH = dir_addr[14];
    dir->RC = dir_addr[15];

    // 4) copy 16 bytes of block indices
    memcpy(dir->blocks, dir_addr + 16, 16);

    return dir;
}

void makeFreeList() {
    // set all blocks as free
    memset(freeList, true, sizeof(freeList));
    // Block 0 is never free, it holds the directory
    freeList[0] = false;

    // Read block 0 from the disk into memory
    uint8_t block_zero[1024];
    blockRead(block_zero, 0);

    for (int i = 0; i < 32; i++) {
        DirStructType *extent = mkDirStruct(i, block_zero);
        if (extent != NULL && extent->status != UNUSED) {
            // mark blocks as used if they are indeed used
            for (int j = 0; j < 16; j++) {
                if (extent->blocks[j] != 0) {
                    freeList[extent->blocks[j]] = false;
                }
            }

        }
        free(extent);
    }
}

bool checkLegalName(char *str) {
    while (*str) {
        if (!isalnum((unsigned char) *str) && *str != ' ') // Allow alphanumeric and spaces (adjust as needed)
            return false;
        str++;
    }
    return true;
}

int findExtentWithName(char *name, uint8_t *block_zero) {
    if (!name || !block_zero) {
        return -1;
    }
    char file_name[9] = {0}; // up to 8 characters + terminator
    char ext_name[4] = {0};
    char *dot = strchr(name, '.');

    if (dot) {
        strncpy(file_name, name, dot - name); // copy part before dot
        strncpy(ext_name, dot + 1, 3); // copy up to 3 characters after the dot
    } else {
        strncpy(file_name, name, 8);  // No dot found, assume entire input is the file name
    }

    file_name[8] = '\0';
    ext_name[3] = '\0';

    if (!checkLegalName(file_name) || !checkLegalName(ext_name)) {
        return -1; // illegal name
    }

    for (int i = 0; i < 32; i++) {
        DirStructType *dir = mkDirStruct(i, block_zero);

        if (dir) {
            if (strncmp(dir->name, file_name, 8) == 0 &&
                strncmp(dir->extension, ext_name, 3) == 0) { // compare the name
                int result = dir->status != UNUSED ? i : -1; // if deleted, return -1
                return result;
            }
            free(dir);
        }
    }
    return -1; // file not found
}

// Pad remaining space with blanks if the name/extension is short
void prepareDirEntryField(char *dest, const char *src, size_t n) {
    size_t i = 0;
    while (i < n && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    while (i < n) {
        dest[i++] = ' ';
    }
}

void writeDirStruct(DirStructType *dir, uint8_t index, uint8_t *super_block_entry) {
    if (!dir || !super_block_entry) {
        fprintf(stderr, "invalid arguments for writeDirStruct");
        return;
    }

    if (index >= 32) {
        fprintf(stderr, "index out of bounds in write dir struct\n");
        return;
    }
    uint8_t *dir_addr = super_block_entry + index * 32; // each dir entry is 32 bytes

    dir_addr[0] = dir->status;
    prepareDirEntryField((char *) dir_addr + 1, dir->name, 8);
    prepareDirEntryField((char *) dir_addr + 9, dir->extension, 3);

    dir_addr[12] = dir->XL;

    dir_addr[13] = dir->BC;
    dir_addr[14] = dir->XH;
    dir_addr[15] = dir->RC;

    memcpy(dir_addr + 16, dir->blocks, 16);
}

void printFreeList() {
    // Assume that block 0 is never free because it contains the directory
    printf("Free Block List:\n");
    for (int i = 0; i < 256; i++) {
        if (i % 16 == 0) {
            // Print the block index header every 16 blocks
            printf("%s%02x: ", i > 0 ? "\n" : "", i);
        }
        // Print '*' for used blocks, '.' for free blocks
        printf("%c", freeList[i] ? '.' : '*');
    }
    printf("\n");
}

int cpmDelete(char *name) {
    if (!name) {
        printf("Invalid file name\n");
        return -1;
    }
    uint8_t block_zero[1024];  // Assuming block size of 1024 bytes
    blockRead(block_zero, 0);  // Read the directory block (block 0)

    int extentNumber = findExtentWithName(name, block_zero);
    if (extentNumber == -1) {
        printf("Failed to find file %s\n.", name);
        return -1;  // File not found
    }

    DirStructType *dir = mkDirStruct(extentNumber, block_zero);
    if (!dir) {
        printf("failed to retrieve directory structure");
        return -1;
    }

    dir->status = UNUSED;

    // free all blocks
    for (int i = 0; i < 16; i++) {
        if (dir->blocks[i] != 0) {
            freeList[dir->blocks[i]] = true;
        }
    }
    // write it back to disk
    writeDirStruct(dir, extentNumber, block_zero);
    free(dir);
    printf("File successfully deleted %s\n", name);
    return 0;
}

int cpmRename(char *oldName, char *newName) {
    if (!oldName || newName) {
        printf("Invalid file name\n");
        return -1;
    }

    if (!checkLegalName(newName)) {
        printf("Illegal new name: %s\n", newName);
        return -1;
    }
    uint8_t block_zero[1024];  // Assuming block size of 1024 bytes
    blockRead(block_zero, 0);  // Read the directory block (block 0)

    int extentNumber = findExtentWithName(oldName, block_zero);
    if (extentNumber == -1) {
        printf("File not found: %s\n", oldName);
        return -1;
    }

    DirStructType *dir = mkDirStruct(extentNumber, block_zero);
    if (!dir) {
        printf("failed to retrieve directory structure");
        return -1;
    }

    // set a new name
    char *dot = strchr(newName, '.');
    if (dot) {
        memset(dir->name, ' ', 8);
        memset(dir->extension, ' ', 3);

        strncpy(dir->name, newName, dot - newName);
        strncpy(dir->extension, dot + 1, 3);
    } else { // file has no extension
        memset(dir->name, ' ', 8);
        strncpy(dir->name, newName, 8);
    }

    // write it back to disk
    writeDirStruct(dir, extentNumber, block_zero);

    free(dir);
    printf("File renamed successfully from %s to %s\n", oldName, newName);
    return 0;
}