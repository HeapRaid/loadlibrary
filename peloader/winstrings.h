#ifndef __STRINGS_H
#define __STRINGS_H

size_t CountWideChars(const void *wcharbuf);
char * CreateAnsiFromWide(const void *wcharbuf);
char *string_from_wchar(const void *wcharbuf, size_t len);

#endif
