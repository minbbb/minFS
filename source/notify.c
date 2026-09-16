#include <stdio.h>
#include <time.h>

#include "config.h"
#include "notify.h"

void notify_ultrahand_start(const char *ip, int port)
{
    time_t now = time(NULL);
    char path[256];

    snprintf(path, sizeof(path), "%s/minFS-%ld.notify", NOTIFY_DIR_PATH, (long)now);

    FILE *f = fopen(path, "w");
    if (f == NULL)
        return;

    fprintf(f,
        "{\"text\":\"minFS running at http://%s:%d\","
        "\"title\":\"minFS\",\"font_size\":22,\"duration\":10000,"
        "\"show_time\":\"true\",\"priority\":20}\n",
        ip, port);

    fclose(f);
}