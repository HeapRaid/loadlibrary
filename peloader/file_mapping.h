#ifndef LOADLIBRARY_FILE_MAPPING_H
#define LOADLIBRARY_FILE_MAPPING_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct mapped_file_view {
    void *next;
    void *base;
    size_t size;
} MappedFileView;

typedef struct mapped_file_view_list {
    MappedFileView *head;
} MappedFileViewList;

void AddMappedView(MappedFileView *mapped_view, MappedFileViewList **list);
bool DeleteMappedView(MappedFileView *mapped_view, MappedFileViewList *list);
MappedFileView* SearchMappedViews(void *base, MappedFileViewList *list);

#endif //LOADLIBRARY_FILE_MAPPING_H
