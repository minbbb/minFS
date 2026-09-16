#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <signal.h>
#include <switch.h>
#include <unistd.h>

#include "config.h"
#include "config_file.h"
#include "http_server.h"
#include "http_request.h"
#include "mime.h"
#include "path_util.h"
#include "fs_list.h"
#include "index_html.h"
#include "styles_css.h"
#include "app_js.h"
#include "state_js.h"
#include "utils_js.h"
#include "ui_js.h"
#include "viewer_js.h"
#include "events_js.h"
#include "log.h"

#define CHUNK_SIZE  65536
#define PROBE_EVERY 200
#define DEAD_LIMIT  4

/* Percent-encode bytes that would break the quoted Content-Disposition
 * filename or the header framing (" \ and any residual control char). */
static void sanitize_header_filename(char *out, size_t size, const char *name)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0;

    for (const char *p = name; *p != 0 && o + 1 < size; p++)
    {
        unsigned char c = (unsigned char)*p;

        if (c == '"' || c == '\\' || c < 0x20 || c == 0x7f)
        {
            if (o + 3 >= size)
                break;
            out[o++] = '%';
            out[o++] = hex[c >> 4];
            out[o++] = hex[c & 0x0f];
        }
        else
        {
            out[o++] = (char)c;
        }
    }

    out[o] = 0;
}

static int server_fd = -1;
static int poll_count = 0;
static int dead_streak = 0;
static int network_down = 1;

static int network_is_up(void)
{
    u32 ip = 0;
    NifmInternetConnectionStatus status = 0;

    if (R_FAILED(nifmGetInternetConnectionStatus(NULL, NULL, &status)))
        return 0;
    if (status != NifmInternetConnectionStatus_Connected)
        return 0;
    if (R_FAILED(nifmGetCurrentIpAddress(&ip)))
        return 0;

    return ip != 0;
}

static int listener_is_alive(void)
{
    int pfd = socket(AF_INET, SOCK_STREAM, 0);
    if (pfd < 0)
        return 0;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(server_config_get()->port);

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(pfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    int ok = connect(pfd, (struct sockaddr *)&addr, sizeof(addr)) == 0;
    close(pfd);
    return ok;
}

static int send_all(int fd, const void *data, size_t len)
{
    const char *p = (const char *)data;

    while (len > 0)
    {
        ssize_t n = send(fd, p, len, 0);

        if (n > 0)
        {
            p += n;
            len -= (size_t)n;
            continue;
        }

        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            svcSleepThread(1000 * 1000);
            continue;
        }

        if (n < 0)
            log_debug("[HTTP] send error: %s\n", strerror(errno));

        return -1;
    }

    return 0;
}

static void send_response(int fd, const char *status, const char *ctype, const char *body)
{
    char header[512];
    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-store\r\n"
        "\r\n",
        status, ctype, (unsigned)strlen(body));

    send_all(fd, header, (size_t)n);
    send_all(fd, body, strlen(body));
}

static void send_error(int fd, const char *status, const char *body)
{
    send_response(fd, status, "text/plain", body);
}

typedef struct
{
    const char *path;
    const char *ctype;
    const char *data;
} StaticAsset;

static const StaticAsset static_assets[] = {
    { "/css/styles.css", "text/css",        styles_css },
    { "/js/app.js",      "text/javascript", app_js     },
    { "/js/state.js",    "text/javascript", state_js   },
    { "/js/utils.js",    "text/javascript", utils_js   },
    { "/js/ui.js",       "text/javascript", ui_js      },
    { "/js/viewer.js",   "text/javascript", viewer_js  },
    { "/js/events.js",   "text/javascript", events_js  },
};

static int build_file_header(char *buf, size_t size, const char *ctype,
                             unsigned long long file_size,
                             long long start, long long end, int is_range,
                             const char *dl_name)
{
    if (is_range)
    {
        return snprintf(buf, size,
            "HTTP/1.1 206 Partial Content\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %llu\r\n"
            "Content-Range: bytes %llu-%llu/%llu\r\n"
            "Connection: close\r\n"
            "Cache-Control: no-store\r\n"
            "Accept-Ranges: bytes\r\n"
            "\r\n",
            ctype,
            (unsigned long long)(end - start + 1),
            (unsigned long long)start,
            (unsigned long long)end,
            (unsigned long long)file_size);
    }

    if (dl_name != NULL)
    {
        char dl_safe[3 * 4096 + 1];
        sanitize_header_filename(dl_safe, sizeof(dl_safe), dl_name);

        return snprintf(buf, size,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %llu\r\n"
            "Content-Disposition: attachment; filename=\"%s\"\r\n"
            "Connection: close\r\n"
            "Cache-Control: no-store\r\n"
            "\r\n",
            ctype, (unsigned long long)file_size, dl_safe);
    }

    return snprintf(buf, size,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %llu\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-store\r\n"
        "Accept-Ranges: bytes\r\n"
        "\r\n",
        ctype, (unsigned long long)file_size);
}

static void stream_remaining(int fd, FILE *f, long long remaining,
                             char *buf, size_t buf_size, size_t first_got)
{
    size_t got = first_got;
    long long sent = 0;

    while (got > 0 && remaining > 0)
    {
        if (send_all(fd, buf, got) != 0)
        {
            log_debug("[HTTP] transfer aborted: sent %llu of %llu bytes\n",
                   (unsigned long long)sent,
                   (unsigned long long)(remaining + sent));
            return;
        }

        sent += (long long)got;
        remaining -= (long long)got;

        if (remaining <= 0)
            break;

        size_t want = (size_t)(remaining < (long long)buf_size ? remaining : (long long)buf_size);
        got = fread(buf, 1, want, f);

        if (got == 0 && ferror(f))
        {
            log_debug("[HTTP] read error after %llu bytes\n", (unsigned long long)sent);
            return;
        }
    }

    (void)sent;
    log_debug("[HTTP] sent %llu bytes\n", (unsigned long long)sent);
}

static void send_file_ex(int fd, const char *path, const char *ctype,
                         const char *dl_name, const HttpRange *rng)
{
    struct stat st;
    char fspath[4200];
    logical_to_fs_path(fspath, sizeof(fspath), path);

    if (stat(fspath, &st) != 0)
    {
        send_error(fd, "404 Not Found", "Not Found");
        return;
    }

    if (S_ISDIR(st.st_mode))
    {
        send_error(fd, "400 Bad Request", "Cannot download a directory");
        return;
    }

    long long start = 0;
    long long end = (long long)st.st_size - 1;
    int is_range = 0;

    if (rng != NULL)
    {
        if (rng->suffix)
        {
            start = (long long)st.st_size - rng->start;
            if (start < 0)
                start = 0;
            end = (long long)st.st_size - 1;
        }
        else
        {
            start = rng->start;
            end = rng->end >= 0 ? rng->end : (long long)st.st_size - 1;
            if (end > (long long)st.st_size - 1)
                end = (long long)st.st_size - 1;
        }

        if ((long long)st.st_size <= 0 || start >= (long long)st.st_size || start > end)
        {
            char h416[512];
            int h4 = snprintf(h416, sizeof(h416),
                "HTTP/1.1 416 Range Not Satisfiable\r\n"
                "Content-Range: bytes */%llu\r\n"
                "Content-Length: 0\r\n"
                "Connection: close\r\n"
                "\r\n",
                (unsigned long long)st.st_size);

            send_all(fd, h416, (size_t)h4);
            log_debug("[HTTP] 416 for %s (size=%llu)\n", path,
                   (unsigned long long)st.st_size);
            return;
        }

        is_range = 1;
    }

    FILE *f = fopen(fspath, "rb");
    if (f == NULL)
    {
        send_error(fd, "404 Not Found", "Not Found");
        return;
    }

    if (is_range && start > 0)
    {
        if (fseek(f, start, SEEK_SET) != 0)
        {
            send_error(fd, "500 Internal Server Error", "Seek failed");
            fclose(f);
            return;
        }
    }

    long long remaining = is_range ? (end - start + 1) : (long long)st.st_size;

    static char buf[CHUNK_SIZE];
    size_t want = (size_t)(remaining < (long long)CHUNK_SIZE ? remaining : (long long)CHUNK_SIZE);
    size_t got = fread(buf, 1, want, f);

    if (got == 0 && ferror(f))
    {
        send_error(fd, "500 Internal Server Error", "Read failed");
        fclose(f);
        return;
    }

    char header[1024];
    int hn = build_file_header(header, sizeof(header), ctype,
                               (unsigned long long)st.st_size,
                               start, end, is_range, dl_name);

    if (send_all(fd, header, (size_t)hn) != 0)
    {
        fclose(f);
        return;
    }

    stream_remaining(fd, f, remaining, buf, sizeof(buf), got);
    fclose(f);
}

static void handle_client(int cfd)
{
    HttpRequest req;
    int rc = http_request_read(cfd, &req);

    if (rc == -1)
        return;

    if (rc == -2)
    {
        send_error(cfd, "400 Bad Request", "Bad Request");
        return;
    }

    if (strcmp(req.method, "GET") != 0)
    {
        send_error(cfd, "405 Method Not Allowed", "Method Not Allowed");
        return;
    }

    if (req.target[0] == 0 || strcmp(req.target, "/") == 0)
    {
        send_response(cfd, "200 OK", "text/html; charset=utf-8", index_html);
        log_debug("[HTTP] GET / -> index\n");
        return;
    }

    if (strcmp(req.target, "/api") == 0 || strcmp(req.target, "/download") == 0 ||
        strcmp(req.target, "/img") == 0 || strcmp(req.target, "/video") == 0)
    {
        char norm[4096];

        if (strcmp(req.target, "/api") == 0)
        {
            if (!path_from_query(req.query, norm, sizeof(norm)))
                snprintf(norm, sizeof(norm), "%s", server_config_get()->start_path);
        }
        else
        {
            path_from_query(req.query, norm, sizeof(norm));
        }

        if (strcmp(req.target, "/video") == 0)
        {
            const char *ctype = mime_for_video(norm);
            if (ctype == NULL)
            {
                send_error(cfd, "415 Unsupported Media Type", "Not a video");
                return;
            }

            log_debug("[HTTP] GET /video?path=%s -> %s%s\n", norm, ctype,
                   req.has_range ? " (range)" : "");
            send_file_ex(cfd, norm, ctype, NULL,
                         req.has_range ? &req.range : NULL);
            return;
        }

        if (strcmp(req.target, "/download") == 0)
        {
            log_debug("[HTTP] GET /download?path=%s\n", norm);
            send_file_ex(cfd, norm, "application/octet-stream",
                         path_basename(norm), NULL);
            return;
        }

        if (strcmp(req.target, "/img") == 0)
        {
            const char *ctype = mime_for_image(norm);
            if (ctype == NULL)
            {
                send_error(cfd, "415 Unsupported Media Type", "Not an image");
                return;
            }

            log_debug("[HTTP] GET /img?path=%s -> %s\n", norm, ctype);
            send_file_ex(cfd, norm, ctype, NULL, NULL);
            return;
        }

        char *json = fs_list_json(norm);
        const char *body = json != NULL ? json : "{\"error\":\"out of memory\"}";

        log_debug("[HTTP] GET /api?path=%s -> %u bytes\n", norm, (unsigned)strlen(body));
        send_response(cfd, "200 OK", "application/json; charset=utf-8", body);
        free(json);
        return;
    }

    for (size_t i = 0; i < sizeof(static_assets) / sizeof(static_assets[0]); i++)
    {
        if (strcmp(req.target, static_assets[i].path) == 0)
        {
            send_response(cfd, "200 OK", static_assets[i].ctype, static_assets[i].data);
            log_debug("[HTTP] GET %s -> %s\n", req.target, static_assets[i].ctype);
            return;
        }
    }

    log_debug("[HTTP] GET %s -> 404\n", req.target);
    send_error(cfd, "404 Not Found", "Not Found");
}

int http_server_init(void)
{
    signal(SIGPIPE, SIG_IGN);
    server_config_reload();

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
        return -1;

    int yes = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(server_config_get()->port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(server_fd);
        server_fd = -1;
        return -1;
    }

    if (listen(server_fd, 8) < 0)
    {
        close(server_fd);
        server_fd = -1;
        return -1;
    }

    int flags = fcntl(server_fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    return 0;
}

int http_server_rebind(void)
{
    http_server_exit();
    return http_server_init();
}

int http_server_poll(void)
{
    if (server_fd < 0)
        return -1;

    if (++poll_count >= PROBE_EVERY)
    {
        poll_count = 0;
        network_down = !network_is_up();

        if (!network_down)
        {
            if (!listener_is_alive())
            {
                if (++dead_streak >= DEAD_LIMIT)
                {
                    log_file("[net] listener dead while network up\n");
                    return -1;
                }
            }
            else
            {
                dead_streak = 0;
            }
        }
        else
        {
            dead_streak = 0;
        }
    }

    int cfd = accept(server_fd, NULL, NULL);
    if (cfd < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;

        if (network_down)
            return 0;

        log_file("[net] accept error: %s\n", strerror(errno));
        return -1;
    }

    int flags = fcntl(cfd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(cfd, F_SETFL, flags & ~O_NONBLOCK);

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    handle_client(cfd);
    close(cfd);
    return 0;
}

void http_server_exit(void)
{
    if (server_fd >= 0)
    {
        close(server_fd);
        server_fd = -1;
    }
}