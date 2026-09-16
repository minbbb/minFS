#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

int  http_server_init(void);
int  http_server_rebind(void);
int  http_server_poll(void);
void http_server_exit(void);

#endif