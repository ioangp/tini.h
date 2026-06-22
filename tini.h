/*  TinyINI: Miniscule .ini file parser
    ; Copyright (C) 2026 Ioan Phillips
    ; v. 0.2.1
    ; https://github.com/ioangp/tini.h

    Usage:
        In one C file, `#define TINI_IMPLEMENTATION`
        Then `#include "tini.h"`

    Notes:
        - By default, a line has a maximum of 512 characters
        - Entries outside of any section go into a section called 'global' which is always the first section
        - All values are stored as strings, though some helper functions are included
            - True, On, 1 considered 'true', False, Off, 0 considered 'false' (case insensitive)
        - Multiline values are not supported
        - Comments can be done with ';' or '#'
        - Inline comments are supported
        - Duplicate section names and keys are allowed

        - Compiles with C89 and up
JMJ*/

#ifndef TINI_H
#define TINI_H

#include <stddef.h>

#define INI_MAX_LINE 512
#define INI_GLOBAL_SECTION "global"

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

/* Use ini_foreach on the IniSection or IniResult structures, not their `items` */
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
IniSection *ini_get_section(IniResult *ini,     const char* section);   /* returns: First IniSection with that name or NULL */
IniEntry   *ini_get_entry(IniSection  *section, const char* key);       /* returns: First IniEntry with that key or NULL */

int         ini_get_int(IniSection    *section, const char* key, int    default_val);
double      ini_get_double(IniSection *section, const char* key, double default_val);
int         ini_get_bool(IniSection   *section, const char* key, int    default_val);

#endif

#ifdef TINI_IMPLEMENTATION

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

static void ini_section_release(IniSection *);
static void ini_result_release(IniResult *);
static char *ini_strdup(const char *s);
static int ini_stricmp(const char *a, const char *b);
static char *trim(char *s);

#define _inilist_append(list, x)                                                   \
    do {                                                                           \
        if (list.count >= list.capacity) {                                         \
            size_t new_cap = list.capacity == 0 ? 32 : list.capacity * 2;          \
            void *new_items = realloc(list.items, new_cap * sizeof(*list.items));  \
            if (!new_items)                                                        \
                _inierr(INI_ERR_OUT_OF_MEMORY)                                     \
            list.items = new_items;                                                \
            list.capacity = new_cap;                                               \
        }                                                                          \
        list.items[list.count++] = x;                                              \
    } while(0)

#define _inierr(errcode) {                                  \
    err->error = errcode;                                   \
    err->line_no = line_no;                                 \
    strncpy(err->line_text, line, sizeof(err->line_text));  \
    err->line_text[sizeof(err->line_text)-1] = '\0';        \
    goto fail;                                              \
}

IniResult *ini_parse(const char *filepath, IniErrorInfo *err) {
    FILE *fp = NULL;
    char line[INI_MAX_LINE] = "";
    IniResult result = {0};
    IniResult *resultp;
    IniSection current_section = {0};
    size_t line_no = 0;
    IniErrorInfo inf = {0};

    if(!err)
        return NULL;
    if(!filepath)
        _inierr(INI_ERR_INVALID_ARGUMENT)

    *err = inf;
    err->error = INI_OK;

    fp = fopen(filepath, "r");
    if (!fp)
        _inierr(INI_ERR_OPEN_FILE)

    current_section.name = ini_strdup(INI_GLOBAL_SECTION);
    if (!current_section.name)
        _inierr(INI_ERR_OUT_OF_MEMORY)

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

            if (*trim(rbrack + 1) != '\0')
                _inierr(INI_ERR_SECTION_SYNTAX);

            if (*name == '\0')
                _inierr(INI_ERR_SECTION_SYNTAX);

            _inilist_append(result, current_section);
            memset(&current_section, 0, sizeof current_section);

            current_section.name = ini_strdup(name);
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

        entry.key = ini_strdup(key);
        entry.value = ini_strdup(value);

        if(!entry.key || !entry.value)
            _inierr(INI_ERR_OUT_OF_MEMORY)

        _inilist_append(current_section, entry);
    }

    _inilist_append(result, current_section);
    memset(&current_section, 0, sizeof current_section);

    fclose(fp);
    fp = NULL;

    resultp = calloc(1, sizeof(result));
    if (!resultp)
        _inierr(INI_ERR_OUT_OF_MEMORY)

    *resultp = result;

    return resultp;

fail:
    if(fp)
        fclose(fp);

    ini_section_release(&current_section);
    ini_result_release(&result);

    return NULL;
}

static void ini_section_release(IniSection *sec)
{
    size_t j;
    if (!sec) return;

    for (j = 0; j < sec->count; ++j) {
        free((void *)sec->items[j].key);
        free((void *)sec->items[j].value);
    }
    free(sec->items);
    free((void *)sec->name);
}

static void ini_result_release(IniResult *res)
{
    size_t i;
    if (!res) return;

    for (i = 0; i < res->count; ++i)
        ini_section_release(&res->items[i]);
    free(res->items);
}

void ini_free(IniResult *ini)
{
    if (!ini) return;
    ini_result_release(ini);
    free(ini);
}

IniSection *ini_get_section(IniResult *ini, const char* section)
{
    IniSection *s;

    if(!ini)
        return NULL;

    ini_foreach_ansi(s, ini) {
        if (strcmp(s->name, section) == 0) {
            return s;
        }
    }

    return NULL;
}

IniEntry *ini_get_entry(IniSection *section, const char* key)
{
    IniEntry *e;

    if(!section)
        return NULL;

    ini_foreach_ansi(e, section) {
        if (strcmp(e->key, key) == 0) {
            return e;
        }
    }

    return NULL;
}

int ini_get_int(IniSection *section, const char* key, int default_val)
{
    IniEntry *e = ini_get_entry(section, key);
    char *end;
    long value;
    
    if(!e)
        return default_val;
        
    errno = 0;
    value = strtol(e->value, &end, 10);
        
    if (errno != 0 || end == e->value || *end != '\0')
        return default_val;
    
    if (value < INT_MIN || value > INT_MAX)
        return default_val;
    
    return (int)value;
}

double ini_get_double(IniSection *section, const char* key, double default_val)
{
    IniEntry *e = ini_get_entry(section, key);
    char *end;
    double value;
    
    if(!e)
        return default_val;
        
    errno = 0;
    value = strtod(e->value, &end);
        
    if (errno != 0 || end == e->value || *end != '\0')
        return default_val;
        
    return value;
}

int ini_get_bool(IniSection *section, const char* key, int default_val)
{
    IniEntry *e = ini_get_entry(section, key);
    if(!e)
        return default_val;

    if(ini_stricmp(e->value, "1") == 0)
        return true;

    if (ini_stricmp(e->value, "true") == 0)
        return true;
    
    if (ini_stricmp(e->value, "on") == 0)
        return true;

    if(ini_stricmp(e->value, "0") == 0)
        return false;

    if (ini_stricmp(e->value, "false") == 0)
        return false;
    
    if (ini_stricmp(e->value, "off") == 0)
        return false;
    
    return default_val;
}

void ini_print(IniResult *ini)
{
    IniSection *section;

    if (!ini)
        return;

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

static char *trim(char *s)
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

static char *ini_strdup(const char *s)
{
    size_t len;
    char *copy;

    if (!s)
        return NULL;

    len = strlen(s) + 1;
    copy = malloc(len);

    if (copy)
        memcpy(copy, s, len);

    return copy;
}

static int ini_stricmp(const char *a, const char *b)
{
    unsigned char ca, cb;

    while (*a && *b) {
        ca = (unsigned char)tolower((unsigned char)*a);
        cb = (unsigned char)tolower((unsigned char)*b);

        if (ca != cb)
            return ca - cb;

        a++;
        b++;
    }

    return (unsigned char)tolower((unsigned char)*a) -
           (unsigned char)tolower((unsigned char)*b);
}

#endif
