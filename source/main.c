#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <switch.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "config.h"
#include "config_file.h"
#include "http_server.h"
#include "log.h"
#include "notify.h"

static void get_local_ip(char *buf, size_t buf_size)
{
    u32 ip_addr = 0;
    nifmGetCurrentIpAddress(&ip_addr);

    struct in_addr addr;
    addr.s_addr = ip_addr;
    inet_ntop(AF_INET, &addr, buf, buf_size);
}

extern void __libnx_init_time(void);

#ifdef SYSMODULE

/* ------------------------------------------------------------------ */
/* Sysmodule mode: no console, runs in the background.                 */
/* ------------------------------------------------------------------ */

#define INNER_HEAP_SIZE (2 * 1024 * 1024)

static const SocketInitConfig socket_config = {
    .tcp_tx_buf_size        = 32 * 1024,
    .tcp_rx_buf_size        = 32 * 1024,
    .tcp_tx_buf_max_size    = 64 * 1024,
    .tcp_rx_buf_max_size    = 64 * 1024,
    .udp_tx_buf_size        = 8 * 1024,
    .udp_rx_buf_size        = 16 * 1024,
    .sb_efficiency          = 4,
    .num_bsd_sessions       = 6,
    .bsd_service_type       = BsdServiceType_System,
};

u32 __nx_applet_type = AppletType_None;

TimeServiceType __nx_time_service_type = TimeServiceType_System;

u32 __nx_fs_num_sessions = 1;

void __libnx_initheap(void)
{
    static u8 inner_heap[INNER_HEAP_SIZE];
    extern void *fake_heap_start;
    extern void *fake_heap_end;

    fake_heap_start = inner_heap;
    fake_heap_end   = inner_heap + sizeof(inner_heap);
}

void __appInit(void)
{
    Result rc;

    rc = smInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));

    rc = setsysInitialize();
    if (R_SUCCEEDED(rc))
    {
        SetSysFirmwareVersion fw;
        if (R_SUCCEEDED(setsysGetFirmwareVersion(&fw)))
            hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
        setsysExit();
    }

    rc = fsInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_FS));

    rc = fsdevMountSdmc();
    if (R_FAILED(rc))
        diagAbortWithResult(rc);

    rc = socketInitialize(&socket_config);
    if (R_FAILED(rc))
        diagAbortWithResult(rc);

    rc = nifmInitialize(NifmServiceType_System);
    if (R_FAILED(rc))
        diagAbortWithResult(rc);

    rc = timeInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(rc);

    __libnx_init_time();

    smExit();
}

void __appExit(void)
{
    nifmExit();
    socketExit();
    timeExit();
    fsdevUnmountAll();
    fsExit();
    smExit();
}

static void log_startup_info(int listen_ok, int port)
{
    char ip_str[INET_ADDRSTRLEN] = "?.?.?.?";
    get_local_ip(ip_str, sizeof(ip_str));

    log_file("== minFS (sysmodule) ==\n");
    log_file("IP: %s\n", ip_str);
    if (listen_ok)
    {
        log_file("HTTP server listening: http://%s:%d\n", ip_str, port);
        notify_ultrahand_start(ip_str, port);
    }
    else
        log_file("ERROR: failed to start web server on port %d\n", port);
}

int main(int argc, char *argv[])
{
    int ok = http_server_init() == 0;
    int port = server_config_get()->port;
    log_startup_info(ok, port);

    if (!ok)
    {
        http_server_exit();
        socketExit();
        nifmExit();
        fsdevUnmountAll();
        fsExit();
        smExit();
        return 1;
    }

    for (;;)
    {
        if (http_server_poll() < 0)
        {
            log_file("[net] listener dead while network up, rebinding...\n");

            if (http_server_rebind() == 0)
                log_file("[net] rebind OK\n");
            else
                log_file("[net] rebind FAILED\n");

            svcSleepThread(REBIND_DELAY_NS);
            continue;
        }

        svcSleepThread(POLL_INTERVAL_NS);
    }
}

#else

/* ------------------------------------------------------------------ */
/* Dev mode: plain homebrew NRO with console, for hbmenu testing.      */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    consoleInit(NULL);

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    nifmInitialize(NifmServiceType_User);
    socketInitializeDefault();
    timeInitialize();
    server_config_reload();

    char ip_str[INET_ADDRSTRLEN];
    get_local_ip(ip_str, sizeof(ip_str));
    int port = server_config_get()->port;

    printf("=== minFS ===\n\n");
    printf("Open http://%s:%d in your browser\n", ip_str, port);
    printf("Press + to quit\n\n");

    if (http_server_init() < 0)
    {
        printf("ERROR: failed to start web server\n");
        consoleUpdate(NULL);
        sleep(5);
        timeExit();
        socketExit();
        nifmExit();
        consoleExit(NULL);
        return 1;
    }

    printf("Listening on port %d...\n\n", server_config_get()->port);
    consoleUpdate(NULL);

    while (appletMainLoop())
    {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus)
            break;

        if (http_server_poll() < 0)
        {
            printf("[HTTP] listener dead while network up, rebinding...\n");
            consoleUpdate(NULL);

            if (http_server_rebind() < 0)
                printf("[HTTP] rebind FAILED\n");
            else
                printf("[HTTP] rebind OK\n");
            consoleUpdate(NULL);

            svcSleepThread(REBIND_DELAY_NS);
            continue;
        }

        svcSleepThread(POLL_INTERVAL_NS);
    }

    http_server_exit();
    timeExit();
    socketExit();
    nifmExit();
    consoleExit(NULL);
    return 0;
}

#endif /* SYSMODULE */