#ifndef CONFIG_FILE_H
#define CONFIG_FILE_H

typedef struct
{
    int port;
    char start_path[4096];
} ServerConfig;

void server_config_reload(void);
const ServerConfig *server_config_get(void);

#endif