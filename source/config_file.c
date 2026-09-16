#include <stdio.h>
#include <string.h>

#include "config.h"
#include "config_file.h"
#include "path_util.h"

static ServerConfig cfg = { SERVER_PORT, "/" };

void server_config_reload(void)
{
    int port = SERVER_PORT;
    char start[4096] = "/";

    FILE *f = fopen(CONFIG_FILE_PATH, "r");
    if (f != NULL)
    {
        char line[256];

        while (fgets(line, sizeof(line), f) != NULL)
        {
            char key[32];

            if (sscanf(line, " %31[^=] =", key) != 1)
                continue;

            if (strcmp(key, "port") == 0)
            {
                int value;

                if (sscanf(line, " %31[^=] = %d", key, &value) == 2)
                    port = value;
            }
            else if (strcmp(key, "start_path") == 0)
            {
                char raw[4096];

                if (sscanf(line, " %31[^=] = %4095[^\r\n]", key, raw) == 2)
                {
                    char *end = raw + strlen(raw);

                    while (end > raw && (end[-1] == ' ' || end[-1] == '\t'))
                        end--;
                    *end = 0;

                    fs_to_logical_path(start, sizeof(start), raw);
                }
            }
        }

        fclose(f);
    }

    if (port < 1 || port > 65535)
        port = SERVER_PORT;

    cfg.port = port;
    snprintf(cfg.start_path, sizeof(cfg.start_path), "%s", start);
}

const ServerConfig *server_config_get(void)
{
    return &cfg;
}