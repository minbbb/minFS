# minFS

A browser-based file manager for the Nintendo Switch running Atmosphere. minFS runs a small HTTP server on the console (default port `8080`) so you can browse the SD card from a PC browser: list folders, download files, and view images and videos inline.

Two builds are provided:

- **Sysmodule** - runs in the background on Atmosphere and is the recommended way to use minFS.
- **NRO** - a homebrew app.

## Screenshots

![Directory listing](pics/screen1.jpg)

![Media viewer](pics/screen2.jpg)

## Requirements

- A Switch with custom firmware (Atmosphere)
- A Wi-Fi or LAN connection
- Ultrahand (optional) - console IP display, port selector, and logging toggle

## Installation

### Sysmodule

1. Copy the built `minFS.nsp` into the module's contents slot as **`exefs.nsp`**: `sdmc:/atmosphere/contents/420000000007E5AC/exefs.nsp`.
2. Copy `toolbox.json` from the repository root to `sdmc:/atmosphere/contents/420000000007E5AC/toolbox.json`.
3. Optional - auto-start at boot: create an empty file `sdmc:/atmosphere/contents/420000000007E5AC/flags/boot2.flag`.
4. Enable the module in Ultrahand → **Sysmodules** (or reboot with `boot2.flag` present).

### Homebrew app

1. Copy `minFS.nro` to the SD card, e.g. `sdmc:/switch/`.
2. Launch it from hbmenu. The address to open is printed on the console.

### Ultrahand package (optional)

1. Copy the `ultrahand/minFS/` folder to `sdmc:/switch/.packages/` on the SD card.
2. In Ultrahand, open the package list and select **minFS**. It provides the status row (IP, port, start path), a `[*Server Port]` selector, a `[Logging]` toggle, and log management (`[View Log]` / `[Clear Log]`).

## Usage

1. Find the console IP (Ultrahand status row, your router, or the NRO screen).
2. Open `http://<ip>:8080` in a browser on the same network (use the configured port if you changed it).

## Configuration

Runtime options live in `sdmc:/switch/minfs.ini`:

```ini
[server]
port = 8080
start_path = sdmc:/switch
```

| Key          | Default       | Description                                         |
| ------------ | ------------- | --------------------------------------------------- |
| `port`       | `8080`        | TCP port the HTTP server binds to                   |
| `start_path` | `/` (SD root) | Folder shown when the web UI opens (SD path or `/`) |

Notes:

- A missing or invalid `port` falls back to `8080`; a missing `start_path` falls back to the SD root.
- Settings are read once at startup - changing the file requires restarting the module.

### Logging (sysmodule)

- Log file: `sdmc:/switch/minFS.log`.
- Logging is enabled while the flag file `sdmc:/atmosphere/contents/420000000007E5AC/flags/logging.flag` exists. The flag is checked on every write, so toggling it applies immediately (the Ultrahand `[Logging]` row does exactly this).
- Build with `make ENABLE_LOGGING=1` to force logging on regardless of the flag.
- File logging is a sysmodule-only feature; the NRO dev build prints debug output to the console instead.

## HTTP API

All routes accept `GET` only.

| Route                         | Purpose                                                                              |
| ----------------------------- | ------------------------------------------------------------------------------------ |
| `/`                           | Web UI (`text/html`)                                                                 |
| `/css/styles.css`, `/js/*.js` | Embedded UI assets (compiled into the binary)                                        |
| `/api?path=...`               | Directory listing as JSON; omitted `path` → `start_path`                             |
| `/download?path=...`          | Downloads a file (`Content-Disposition`, basename as filename)                       |
| `/img?path=...`               | Serves an image inline (`image/*`); other extensions → `415`                         |
| `/video?path=...`             | Serves a video inline (`video/*`) with byte-range seek; unsatisfiable ranges → `416` |

Listing format:

```json
{
    "path": "/",
    "parent": null,
    "items": [
        { "name": "switch", "dir": true, "size": 0 },
        { "name": "note.txt", "dir": false, "size": 123 }
    ]
}
```

Example:

```
curl -s 'http://192.168.1.42:8080/api?path=%2F' | head
```

## Build

Requires devkitPro + libnx. On Windows, build from the devkitPro MSYS2 login shell - stock PowerShell has no `aarch64-none-elf-gcc`. The Makefile hard-errors if `DEVKITPRO` is unset.

| Command                 | Result                              |
| ----------------------- | ----------------------------------- |
| `make`                  | Sysmodule `minFS.nsp`               |
| `make nro`              | Dev build `minFS.nro` for hbmenu    |
| `make ENABLE_LOGGING=1` | Sysmodule with forced file logging  |
| `make clean`            | Removes build artifacts and outputs |

Web assets under `web/` are embedded into the binary as C string literals at build time - nothing under `web/` is read from disk at runtime.
