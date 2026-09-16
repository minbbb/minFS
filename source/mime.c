#include <stddef.h>
#include <string.h>
#include <strings.h>

#include "mime.h"

typedef struct
{
    const char *ext;
    const char *mime;
} MimeEntry;

static const MimeEntry image_exts[] = {
    { "jpg",  "image/jpeg"    },
    { "jpeg", "image/jpeg"    },
    { "png",  "image/png"     },
    { "gif",  "image/gif"     },
    { "webp", "image/webp"    },
    { "bmp",  "image/bmp"     },
    { "svg",  "image/svg+xml" },
};

static const MimeEntry video_exts[] = {
    { "mp4",  "video/mp4"        },
    { "m4v",  "video/x-m4v"      },
    { "webm", "video/webm"       },
    { "ogv",  "video/ogg"        },
    { "mov",  "video/quicktime"  },
    { "mkv",  "video/x-matroska" },
    { "avi",  "video/x-msvideo"  },
    { "wmv",  "video/x-ms-wmv"   },
    { "flv",  "video/x-flv"      },
    { "mpg",  "video/mpeg"       },
    { "mpeg", "video/mpeg"       },
    { "ts",   "video/mp2t"       },
    { "m2ts", "video/mp2t"       },
    { "3gp",  "video/3gpp"       },
    { "vob",  "video/mpeg"       },
};

static const char *match(const MimeEntry *table, size_t n, const char *path)
{
    const char *dot = strrchr(path, '.');
    if (dot == NULL)
        return NULL;

    for (size_t i = 0; i < n; i++)
    {
        if (strcasecmp(dot + 1, table[i].ext) == 0)
            return table[i].mime;
    }

    return NULL;
}

const char *mime_for_image(const char *path)
{
    return match(image_exts, sizeof(image_exts) / sizeof(image_exts[0]), path);
}

const char *mime_for_video(const char *path)
{
    return match(video_exts, sizeof(video_exts) / sizeof(video_exts[0]), path);
}