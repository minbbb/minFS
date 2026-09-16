#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <stddef.h>

#define HTTP_MAX_REQUEST 8192
#define HTTP_MAX_METHOD  16

typedef struct
{
    int       present;
    int       valid;
    int       suffix;
    long long start;
    long long end;
} HttpRange;

typedef struct
{
    char        raw[HTTP_MAX_REQUEST];
    char        method[HTTP_MAX_METHOD];
    const char *target;
    const char *query;
    HttpRange   range;
    int         has_range;
} HttpRequest;

/* Reads and parses one request. Returns 0 on success, -1 on read error /
 * closed connection, -2 on a malformed request line. */
int http_request_read(int fd, HttpRequest *req);

#endif