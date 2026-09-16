#ifndef CONFIG_H
#define CONFIG_H

#define SERVER_PORT        8080
#define LOG_FILE_PATH      "sdmc:/switch/minFS.log"
#define LOG_FLAG_PATH      "sdmc:/atmosphere/contents/420000000007E5AC/flags/logging.flag"
#define CONFIG_FILE_PATH   "sdmc:/switch/minfs.ini"
#define NOTIFY_DIR_PATH    "sdmc:/config/ultrahand/notifications"
#define POLL_INTERVAL_NS   (5 * 1000 * 1000)
#define REBIND_DELAY_NS    (1000 * 1000 * 1000)

#endif