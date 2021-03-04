#include <stdlib.h>
#include "file_mapping.h"


void AddMappedView(MappedFileView *mapped_view, MappedFileViewList **pList)
{
    MappedFileView *current;
    MappedFileViewList *list = *pList;
    if (list == NULL) {
        list = (MappedFileViewList *) malloc(sizeof(MappedFileViewList));
        list->head = mapped_view;
        *pList = list;
        return;
    }

    if (list->head == NULL) {
        list->head = mapped_view;
        return;
    }

    current = list->head;

    while(current->next != NULL) {
        current = current->next;
    }

    current->next = mapped_view;
}

bool DeleteMappedView(MappedFileView *mapped_file, MappedFileViewList *list)
{
    MappedFileView *to_delete = NULL;
    MappedFileView *current;

    if (list == NULL)
        return false;

    current = list->head;

    // mapped_file is the first in the list
    if (current == mapped_file) {
        to_delete = current;
        list->head = NULL;
        free(to_delete);
        return true;
    }

    while(current != NULL) {
        if (current->next == mapped_file) {
            to_delete = current->next;
            current->next = to_delete->next;
            free(to_delete);
            return true;
        }
        current = current->next;
    }

    return false;
}

MappedFileView* SearchMappedViews(void *base, MappedFileViewList *list)
{
    if (list == NULL)
        return NULL;
    MappedFileView *current = list->head;
    while(current->base != base && current->next != NULL)
        current = current->next;
    return base == current->base ? current : NULL;
}
