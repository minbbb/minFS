#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "http_request.h"
#include "log.h"

static int has_header_end(const char *buf, size_t n)
{
    for (size_t i = 0; i + 4 <= n; i++)
    {
        if (buf[i] == '\r' && buf[i + 1] == '\n' &&
            buf[i + 2] == '\r' && buf[i + 3] == '\n')
            return 1;
    }
    return 0;
}

static ssize_t read_request_headers(int fd, char *buf, size_t size)
{
    size_t total = 0;

    while (total < size - 1)
    {
        ssize_t r = recv(fd, buf + total, size - 1 - total, 0);
        if (r <= 0)
            break;
        total += (size_t)r;

        if (has_header_end(buf, total))
            break;
    }

    buf[total] = 0;
    return (ssize_t)total;
}

static int parse_bytes_range(const char *v, const char *ve, HttpRange *rng)
{
    if (ve - v >= 6 && strncasecmp(v, "bytes=", 6) == 0)
        v += 6;
    else
        return 0;

    if (memchr(v, ',', (size_t)(ve - v)) != NULL)
        return 0;

    if (*v == '-')
    {
        const char *q = v + 1;
        long long n = 0;
        int any = 0;

        while (q < ve && *q >= '0' && *q <= '9')
        {
            n = n * 10 + (*q - '0');
            q++;
            any = 1;
        }

        if (!any || q != ve)
            return 0;

        rng->present = 1;
        rng->suffix  = 1;
        rng->start   = n;
        rng->valid   = 1;
        return 1;
    }

    const char *q = v;
    long long a = 0;
    int any = 0;

    while (q < ve && *q >= '0' && *q <= '9')
    {
        a = a * 10 + (*q - '0');
        q++;
        any = 1;
    }

    if (!any || q >= ve || *q != '-')
        return 0;

    q++;

    if (q == ve)
    {
        rng->present = 1;
        rng->suffix  = 0;
        rng->start   = a;
        rng->end     = -1;
        rng->valid   = 1;
        return 1;
    }

    long long b = 0;
    any = 0;

    while (q < ve && *q >= '0' && *q <= '9')
    {
        b = b * 10 + (*q - '0');
        q++;
        any = 1;
    }

    if (!any || q != ve)
        return 0;

    rng->present = 1;
    rng->suffix  = 0;
    rng->start   = a;
    rng->end     = b;
    rng->valid   = 1;
    return 1;
}

static int parse_range_request(const char *req, HttpRange *rng)
{
    const char *p = strstr(req, "\r\n");
    if (p == NULL)
        return 0;

    while ((p = strstr(p, "\r\n")) != NULL)
    {
        p += 2;

        const char *eol = strstr(p, "\r\n");
        if (eol == NULL || eol == p)
            break;

        const char *colon = memchr(p, ':', (size_t)(eol - p));
        if (colon == NULL)
            continue;

        if ((size_t)(colon - p) == 5 && strncasecmp(p, "Range", 5) == 0)
        {
            const char *v = colon + 1;
            while (v < eol && (*v == ' ' || *v == '\t'))
                v++;
            return parse_bytes_range(v, eol, rng);
        }
    }

    return 0;
}

int http_request_read(int fd, HttpRequest *req)
{
    ssize_t n = read_request_headers(fd, req->raw, sizeof(req->raw));
    if (n <= 0)
        return -1;

    memset(&req->range, 0, sizeof(req->range));
    req->has_range = parse_range_request(req->raw, &req->range) && req->range.valid;

    char *line_end = strstr(req->raw, "\r\n");
    if (line_end != NULL)
        *line_end = 0;

    int target_off = 0;
    if (sscanf(req->raw, "%15s%n", req->method, &target_off) != 1)
        return -2;

    char *target = req->raw + target_off;

    /* Replicate original "%15s %8191s": a token longer than 15 chars leaves a
     * non-whitespace char here and must be rejected the same way. */
    if (*target != ' ' && *target != '\t' && *target != 0)
        return -2;

    while (*target == ' ' || *target == '\t')
        target++;

    char *word_end = strpbrk(target, " \t");
    if (word_end != NULL)
        *word_end = 0;

    if (*target == 0)
        return -2;

    log_debug("[HTTP] %s %s\n", req->method, target);

    char *q = strchr(target, '?');
    if (q != NULL)
        *q = 0;

    req->target = target;
    req->query  = q != NULL ? q + 1 : NULL;
    return 0;
}