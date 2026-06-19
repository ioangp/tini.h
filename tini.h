/*  TinyINI: Miniscule .ini file parser
    ; Ioan P.
    ; v. 0.0.1
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
/*  Iterate over sections in a result or entries in a section. E.g.

    ```
    IniResult *r = ini_parse("config.ini");

    ini_foreach(IniSection, section, r)
    {
        ini_foreach(IniEntry, entry, section)
        {
            if (ini_entry_bool(entry))
                do_something();
        }
    }
    ```
*/
#define ini_foreach(Type, item, in_list) for (Type *item = (in_list)->items; item < (in_list)->items + (in_list)->count; ++item)
#else
#define ini_foreach(Type, item, in_list) Type *item; for (item = (in_list)->items; item < (in_list)->items + (in_list)->count; ++item)
#define ini_foreach_ansi(item, in_list) for (item = (in_list)->items; item < (in_list)->items + (in_list)->count; ++item)
#endif

IniResult  *ini_parse(const char *filepath);
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

IniResult *ini_parse(const char *filepath) {
    FILE *fp;
    char line[INI_MAX_LINE];
    IniResult result = {0};
    IniResult *resultp;
    IniSection current_section = {0};

    fp = fopen(filepath, "r");

    current_section.name = "global";

    while (fgets(line, sizeof line, fp) != NULL) {
        char *p;
        IniEntry entry = {0};
        char *key;
        char *value;
        char *eq;

        p = trim(line);

        if (*p == '\0' || *p == ';' || *p == '#')
            continue;

        if (*p == '[') {
            char *rbrack;
            size_t len;
            char *name;

            rbrack = strchr(p, ']');
            if (!rbrack)
                return NULL;

            *rbrack = '\0';
            name = trim(p + 1);

            list_append(result, current_section);
            memset(&current_section, 0, sizeof current_section);

            current_section.name = strdup(name);
            if (!current_section.name)
                return NULL;

            continue;
        }

        /* Key-value pair */
        eq = strchr(p, '=');
        if (!eq)
            return NULL;

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