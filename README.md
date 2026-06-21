# tini.h
TinyINI. Miniscule single-header INI parser for C. Intended for small projects that just need to load a simple configuration file without pulling in a large dependency. It supports sections, key/value pairs, comments, and a small helper API.

## Notes
- By default, a line has a maximum of 512 characters
- Entries outside of any section go into a section called 'global' which is always the first section
- All values are stored as strings, though helper functions are included to get int, double, bool
    - True, On, 1 considered 'true', False, Off, 0 considered 'false' (case insensitive)
- Section names can be any string
- Comments (including inline) are supported with ';' or '#'
- Duplicate section names and keys **are** allowed
- Multiline values **are not** supported
- Compiles with C89 and up

## Usage
Copy tini.h into your project. In one source file:

```
#define TINI_IMPLEMENTATION
#include "tini.h"
```
In other files:

```
#include "tini.h"
```

## Examples
Given:

```
; config.ini

name = Alice
debug = true

[window]
width = 1280
height = 720
```

Load it:

```
#include <stdio.h>

#define TINI_IMPLEMENTATION
#include "tini.h"

int main(void)
{
    IniErrorInfo err = {0};
    IniResult *ini = ini_parse("config.ini", &err);

    if (!ini)
        printf("config.ini:%d code:%d\n%s\n", err.line_no, err.error, err.line_text);
        return 1;

    IniSection *window = ini_get_section(ini, "window");

    if (window) {
        IniEntry *width = ini_get_entry(window, "width");

        if (width)
            printf("Width: %s\n", width->value);

        int height = ini_get_int(window, "height", 0);
    }

    ini_free(ini);
}
```

### Iteration

To iterate through sections and entries:

```
ini_foreach(IniSection, section, ini)
{
    printf("[%s]\n", section->name);

    ini_foreach(IniEntry, entry, section)
    {
        printf("%s = %s\n",
            entry->key,
            entry->value);
    }
}
```