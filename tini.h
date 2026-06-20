/*  TinyINI: Miniscule .ini file parser
    ; Copyright (C) 2026 Ioan Phillips
    ; v. 0.0.2
    ; https://github.com/ioangp/tini.h

    Usage:
        In one C file, `#define TINI_IMPLEMENTATION`
        Then `#include "tini.h"`

    Notes:
        - By default, a line has a maximum of 512 characters
        - Entries outside of any section go into a section called 'global' which is always the first section
        - All values are stored as strings, though some helper functions are included
        - Multiline values are not supported
        - Comments can be done with ';' or '#'
        - Inline comments are supported
        - Duplicate section names and keys are allowed

        - Compiles with C89 and up
*/

#ifndef TINI_H
#define TINI_H

#include <stddef.h>

#define INI_MAX_LINE 512

typedef enum IniError {
    INI_OK = 0,
    INI_ERR_OPEN_FILE,
    INI_ERR_SECTION_SYNTAX,
    INI_ERR_MISSING_EQUALS,
    INI_ERR_OUT_OF_MEMORY,
    INI_ERR_LINE_TOO_LONG,
    INI_ERR_INVALID_ARGUMENT
} IniError;

typedef struct IniErrorInfo {
    IniError error;
    size_t line_no;
    char line_text[INI_MAX_LINE];
} IniErrorInfo;

typedef struct IniEntry {
    const char *key;
    const char *value;
} IniEntry;

typedef struct IniSection {
    IniEntry *items; /* Entries */
    size_t count;
    size_t capacity;
    const char *name;
} IniSection;

typedef struct IniResult {
    IniSection *items; /* Sections */
    size_t count;
    size_t capacity;
} IniResult;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define ini_foreach(Type, item, in_list) for (Type *item = (in_list)->items; item < (in_list)->items + (in_list)->count; ++item)
#define ini_foreach_ansi(item, in_list) for (item = (in_list)->items; item < (in_list)->items + (in_list)->count; ++item)
#else
#define ini_foreach(Type, item, in_list) Type *item; for (item = (in_list)->items; item < (in_list)->items + (in_list)->count; ++item)
#define ini_foreach_ansi(item, in_list) for (item = (in_list)->items; item < (in_list)->items + (in_list)->count; ++item)
#endif

IniResult  *ini_parse(const char *filepath, IniErrorInfo *err);
void        ini_free(IniResult *ini);
void        ini_print(IniResult *ini);
IniSection *ini_get_section(IniResult *ini, const char* section); /* returns: First IniSection with that name or NULL */
IniEntry   *ini_get_entry(IniSection *section, const char* key); /* returns: First IniEntry with that key or NULL */

#endif

#ifdef TINI_IMPLEMENTATION

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

#define list_append(list, x)                                                        \
    do {                                                                            \
        if (list.count >= list.capacity) {                                          \
            if (list.capacity == 0)                                                 \
                list.capacity = 32;                                                 \
            else                                                                    \
                list.capacity *= 2;                                                 \
            list.items = realloc(list.items, list.capacity * sizeof(*list.items));  \
        }                                                                           \
        list.items[list.count++] = x;                                               \
    } while(0)

char *trim(char *s)
{
    char *end;

    while (isspace((unsigned char)*s))
        s++;

    if (*s == '\0')
        return s;

    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
        end--;

    end[1] = '\0';
    return s;
}

#define _inierr(errcode) {                                  \
    err->error = errcode;                                   \
    err->line_no = line_no;                                 \
    strncpy(err->line_text, line, sizeof(err->line_text));  \
    err->line_text[sizeof(err->line_text)-1] = '\0';        \
    return NULL;                                            \
}

IniResult *ini_parse(const char *filepath, IniErrorInfo *err) {
    FILE *fp;
    char line[INI_MAX_LINE];
    IniResult result = {0};
    IniResult *resultp;
    IniSection current_section = {0};
    size_t line_no = 0;

    fp = fopen(filepath, "r");
    if (!fp)
        _inierr(INI_ERR_OPEN_FILE)

    current_section.name = "global";

    while (fgets(line, sizeof line, fp) != NULL) {
        char *p;
        IniEntry entry = {0};
        char *key;
        char *value;
        char *eq;

        if (!strchr(line, '\n') && !feof(fp))
            _inierr(INI_ERR_LINE_TOO_LONG)
        
        line_no++;
        p = trim(line);

        if (*p == '\0' || *p == ';' || *p == '#')
            continue;

        if (*p == '[') {
            char *rbrack;
            char *name;

            rbrack = strchr(p, ']');
            if (!rbrack)
                _inierr(INI_ERR_SECTION_SYNTAX)

            *rbrack = '\0';
            name = trim(p + 1);

            list_append(result, current_section);
            memset(&current_section, 0, sizeof current_section);

            current_section.name = strdup(name);
            if (!current_section.name)
                _inierr(INI_ERR_OUT_OF_MEMORY)

            continue;
        }

        /* Key-value pair */
        eq = strchr(p, '=');
        if (!eq)
            _inierr(INI_ERR_MISSING_EQUALS)

        *eq = '\0';

        key = trim(p);
        value = trim(eq + 1);

        /*A comment is whitespace followed by ; or # */
        p = value; /* reuse */

        while (*p) {
            if ((*p == ';' || *p == '#') &&
                p > value &&
                isspace((unsigned char)p[-1])) {
                *p = '\0';
                break;
            }

            p++;
        }

        value = trim(value);

        entry.key = strdup(key);
        entry.value = strdup(value);

        if(!entry.key || !entry.value)
            _inierr(INI_ERR_OUT_OF_MEMORY)

        list_append(current_section, entry);
    }

    list_append(result, current_section);

    fclose(fp);

    resultp = calloc(1, sizeof(result));
    *resultp = result;
    return resultp;
}

void ini_free(IniResult *ini)
{
    size_t i;
    size_t j;

    if (!ini) return;

    for (i = 0; i < ini->count; ++i) {
        IniSection *sec = &ini->items[i];

        for (j = 0; j < sec->count; ++j) {
            IniEntry *e = &sec->items[j];

            free((void *)e->key);
            free((void *)e->value);
        }

        free(sec->items);
        free((void *)sec->name);
    }

    free(ini->items);
    free(ini);
}

IniSection *ini_get_section(IniResult *ini, const char* section)
{
    ini_foreach(IniSection, s, ini) {
        if (strcmp(s->name, section) == 0) {
            return s;
        }
    }

    return NULL;
}

IniEntry *ini_get_entry(IniSection *section, const char* key)
{
    IniEntry *e;
    ini_foreach_ansi(e, section) {
        if (strcmp(e->key, key) == 0) {
            return e;
        }
    }

    return NULL;
}

void ini_print(IniResult *ini)
{
    IniSection *section;
    ini_foreach_ansi(section, ini)
    {
        IniEntry *entry;
        printf("[%s]\n", section->name);

        ini_foreach_ansi(entry, section)
        {
            printf("%s = %s\n",
                entry->key,
                entry->value);
        }

        printf("\n");
    }
}

#endif
