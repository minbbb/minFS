#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <strings.h>

#include "fs_list.h"
#include "path_util.h"

typedef struct
{
    char *name;
    int   is_dir;
    long  size;
} Item;

typedef struct
{
    char  *buf;
    size_t len;
    size_t cap;
    int    ok;
} JsonBuf;

static void jb_reserve(JsonBuf *jb, size_t extra)
{
    if (!jb->ok)
        return;

    if (jb->len + extra + 1 > jb->cap)
    {
        size_t nc = jb->cap ? jb->cap : 256;
        while (jb->len + extra + 1 > nc)
            nc *= 2;

        char *nb = (char *)realloc(jb->buf, nc);
        if (nb == NULL)
        {
            jb->ok = 0;
            return;
        }

        jb->buf = nb;
        jb->cap = nc;
    }
}

static void jb_putc(JsonBuf *jb, char c)
{
    jb_reserve(jb, 1);
    if (!jb->ok)
        return;

    jb->buf[jb->len++] = c;
    jb->buf[jb->len] = 0;
}

static void jb_puts(JsonBuf *jb, const char *s)
{
    size_t l = strlen(s);

    jb_reserve(jb, l);
    if (!jb->ok)
        return;

    memcpy(jb->buf + jb->len, s, l);
    jb->len += l;
    jb->buf[jb->len] = 0;
}

static void jb_escape_raw(JsonBuf *jb, const char *s)
{
    while (*s)
    {
        unsigned char c = (unsigned char)*s;

        if (c == '"' || c == '\\')
        {
            jb_putc(jb, '\\');
            jb_putc(jb, (char)c);
        }
        else if (c < 0x20)
        {
            char esc[8];
            snprintf(esc, sizeof(esc), "\\u%04x", c);
            jb_puts(jb, esc);
        }
        else
        {
            jb_putc(jb, (char)c);
        }

        s++;
    }
}

static void jb_escape(JsonBuf *jb, const char *s)
{
    jb_putc(jb, '"');
    jb_escape_raw(jb, s);
    jb_putc(jb, '"');
}

static int item_cmp(const void *a, const void *b)
{
    const Item *ia = (const Item *)a;
    const Item *ib = (const Item *)b;

    if (ia->is_dir != ib->is_dir)
        return ia->is_dir ? -1 : 1;

    return strcasecmp(ia->name, ib->name);
}

static void jb_write_listing_header(JsonBuf *jb, const char *path, const char *parent)
{
    jb_puts(jb, "{\"path\":");
    jb_escape(jb, path);
    jb_puts(jb, ",\"parent\":");
    if (parent[0])
        jb_escape(jb, parent);
    else
        jb_puts(jb, "null");
    jb_puts(jb, ",\"items\":");
}

static void json_error(const char *path, const char *parent, char **out)
{
    JsonBuf jb;
    memset(&jb, 0, sizeof(jb));
    jb.ok = 1;

    jb_write_listing_header(&jb, path, parent);
    jb_puts(&jb, "[]");
    jb_puts(&jb, ",\"error\":\"cannot open directory ");
    jb_escape_raw(&jb, path);
    jb_puts(&jb, "\"}");

    *out = jb.ok ? jb.buf : NULL;
}

char *fs_list_json(const char *path)
{
    char parent[4096];
    path_parent_dir(path, parent, sizeof(parent));

    char fspath[4096];
    logical_to_fs_path(fspath, sizeof(fspath), path);

    DIR *d = opendir(fspath);
    if (d == NULL)
    {
        char *out;
        json_error(path, parent, &out);
        return out;
    }

    Item *items = NULL;
    size_t count = 0;
    size_t cap = 0;
    char full[4608];
    struct dirent *e;

    while ((e = readdir(d)) != NULL)
    {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;

        int plen = snprintf(full, sizeof(full), "%s/%s",
                            strcmp(fspath, "sdmc:/") == 0 ? "sdmc:" : fspath, e->d_name);
        if (plen < 0 || (size_t)plen >= sizeof(full))
            continue;

        struct stat st;
        Item it;
        memset(&it, 0, sizeof(it));

        if (stat(full, &st) == 0)
        {
            it.is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
            it.size = (long)st.st_size;
        }

        it.name = strdup(e->d_name);
        if (it.name == NULL)
            continue;

        if (count == cap)
        {
            size_t nc = cap ? cap * 2 : 64;
            Item *ni = (Item *)realloc(items, nc * sizeof(Item));
            if (ni == NULL)
            {
                free(it.name);
                break;
            }
            items = ni;
            cap = nc;
        }

        items[count++] = it;
    }

    closedir(d);
    qsort(items, count, sizeof(Item), item_cmp);

    JsonBuf jb;
    memset(&jb, 0, sizeof(jb));
    jb.ok = 1;

    jb_write_listing_header(&jb, path, parent);
    jb_putc(&jb, '[');

    for (size_t i = 0; i < count; i++)
    {
        if (i > 0)
            jb_putc(&jb, ',');

        jb_putc(&jb, '{');
        jb_puts(&jb, "\"name\":");
        jb_escape(&jb, items[i].name);
        jb_puts(&jb, ",\"dir\":");
        jb_puts(&jb, items[i].is_dir ? "true" : "false");
        jb_puts(&jb, ",\"size\":");
        {
            char sz[32];
            snprintf(sz, sizeof(sz), "%ld", items[i].size);
            jb_puts(&jb, sz);
        }
        jb_putc(&jb, '}');
    }

    jb_puts(&jb, "]}");

    for (size_t i = 0; i < count; i++)
        free(items[i].name);
    free(items);

    return jb.ok ? jb.buf : NULL;
}