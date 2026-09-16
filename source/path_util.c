#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "path_util.h"

const char *path_basename(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Control bytes (0x00-0x1F, 0x7F) are dropped everywhere a client-supplied
 * path lands, so percent-encoded CR/LF (%0d/%0a) or NUL (%00) can never
 * reach the filesystem or a response header. */
static int is_control_char(char c)
{
    unsigned char u = (unsigned char)c;
    return u < 0x20 || u == 0x7f;
}

void path_percent_decode(const char *in, char *out, size_t out_size)
{
    size_t o = 0;

    for (size_t i = 0; in[i] != 0 && o + 1 < out_size; i++)
    {
        if (in[i] == '%' && in[i + 1] != 0 && in[i + 2] != 0)
        {
            int hi = hexval(in[i + 1]);
            int lo = hexval(in[i + 2]);

            if (hi >= 0 && lo >= 0)
            {
                char c = (char)((hi << 4) | lo);
                if (!is_control_char(c))
                    out[o++] = c;
                i += 2;
                continue;
            }
        }

        if (!is_control_char(in[i]))
            out[o++] = in[i];
    }

    out[o] = 0;
}

void path_normalize(char *out, size_t out_size, const char *in)
{
    char tmp[8200];
    const char *tok[512];
    int n = 0;
    char *save = NULL;

    snprintf(tmp, sizeof(tmp), "/%s", in);

    for (char *t = strtok_r(tmp, "/", &save); t != NULL; t = strtok_r(NULL, "/", &save))
    {
        if (strcmp(t, ".") == 0)
            continue;

        if (strcmp(t, "..") == 0)
        {
            if (n > 0)
                n--;
            continue;
        }

        if (n < 512)
            tok[n++] = t;
    }

    size_t o = 0;
    out[o++] = '/';

    for (int i = 0; i < n; i++)
    {
        size_t l = 0;

        for (const char *p = tok[i]; *p != 0; p++)
            if (!is_control_char(*p))
                l++;

        if (l == 0)
            continue;

        size_t need = (i > 0 ? 1 : 0) + l + 1;

        if (o + need > out_size)
            break;

        if (i > 0)
            out[o++] = '/';

        for (const char *p = tok[i]; *p != 0; p++)
            if (!is_control_char(*p))
                out[o++] = *p;
    }

    out[o] = 0;
}

int path_from_query(const char *query, char *out, size_t out_size)
{
    char raw[4096] = "";
    char query_copy[4096] = "";
    char *save = NULL;

    if (query != NULL)
        snprintf(query_copy, sizeof(query_copy), "%s", query);

    int found = 0;

    for (char *part = strtok_r(query_copy, "&", &save);
         part != NULL; part = strtok_r(NULL, "&", &save))
    {
        if (strncmp(part, "path=", 5) == 0)
        {
            snprintf(raw, sizeof(raw), "%s", part + 5);
            found = 1;
            break;
        }
    }

    char decoded[4096];
    path_percent_decode(raw, decoded, sizeof(decoded));
    path_normalize(out, out_size, decoded);

    return found;
}

void path_parent_dir(const char *path, char *out, size_t size)
{
    if (path == NULL || path[0] == 0 || (path[0] == '/' && path[1] == 0))
    {
        out[0] = 0;
        return;
    }

    size_t i = strlen(path);

    while (i > 0 && path[i - 1] == '/')
        i--;
    while (i > 0 && path[i - 1] != '/')
        i--;
    while (i > 0 && path[i - 1] == '/')
        i--;

    if (i == 0)
    {
        if (size > 1)
        {
            out[0] = '/';
            out[1] = 0;
        }
        return;
    }

    if (i >= size)
        i = size - 1;

    memcpy(out, path, i);
    out[i] = 0;
}

void logical_to_fs_path(char *out, size_t out_size, const char *logical)
{
#ifdef SYSMODULE
    if (logical[0] == '/' && logical[1] == 0)
        snprintf(out, out_size, "sdmc:/");
    else
        snprintf(out, out_size, "sdmc:%s", logical);
#else
    snprintf(out, out_size, "%s", logical);
#endif
}

void fs_to_logical_path(char *out, size_t out_size, const char *in)
{
    const char *p = in != NULL ? in : "";

    if (strncasecmp(p, "sdmc:", 5) == 0)
        p += 5;

    path_normalize(out, out_size, p);
}