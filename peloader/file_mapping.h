#ifndef LOADLIBRARY_FILE_MAPPING_H
#define LOADLIBRARY_FILE_MAPPING_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


typedef struct mapped_file_entry {
    void *next;
    intptr_t start;
    intptr_t end;
    int64_t size;
    int fd;
} MappedFileEntry;

typedef struct mapped_file_object_list {
    MappedFileEntry *head;
} MappedFileObjectList;

void AddMappedFile(MappedFileEntry *mapped_file, MappedFileObjectList *list);
bool DeleteMappedFile(MappedFileEntry *mapped_file, MappedFileObjectList *list);
MappedFileEntry* SearchMappedFile(MappedFileEntry *mapped_file, MappedFileObjectList *list);

extern MappedFileObjectList FileMappingList;

#endif //LOADLIBRARY_FILE_MAPPING_H
