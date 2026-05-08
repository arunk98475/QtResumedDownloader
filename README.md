# Qt Resumed Downloader

A small **Qt Widgets + Network** demo that downloads HTTP/HTTPS files with **resume support**, **automatic retries**, and a simple **callbacks-based** core you can reuse in other apps.

---

## Features

| Area | Behavior |
|------|----------|
| **Resume** | Data is written to a `\<filename>.part` file next to the final target. On restart (or retry), downloaded bytes are reused and the client sends `Range: bytes=\<offset\>-` when appropriate. |
| **Completion** | On success the `.part` file is renamed to the final filename (existing final file is replaced). |
| **Server quirks** | If the server ignores `Range` and returns `200` while a resume was requested, the downloader truncates and restarts once from the beginning. |
| **`416`** | If the remaining range is invalid because the file is already complete (some servers reply `416`), the downloader finalizes by renaming `.part` if needed. |
| **Retries** | On network or transient file errors the downloader backs off exponentially (about 1s → 2s → 4s → … → 64s) and tries again via an internal timer (not Qt “network reachable” APIs). |
| **Abort** | `FileDownloader::abort()` stops the request and retry timer, removes the `.part` file, clears the current URL/target state, and blocks further retries until `download()` is called again. |
| **Hooks** | `FileDownloadEvents` receives progress, completion (with **full output path**), and errors. |

---

## Requirements

- **Qt** with modules: `core`, `gui`, `widgets`, **`network`**
- **C++17** toolchain (matches `QtResumedDownloader.pro`)

Works with typical Qt 5 and Qt 6 installs (CMake not required; project uses **qmake**).

---

## Build

### Qt Creator

1. Open **`QtResumedDownloader.pro`**.
2. Configure a kit (same Qt version as above).
3. Build and run.

### Command line (example)

From the project directory:

```bash
qmake QtResumedDownloader.pro
make   # or nmake / jom on Windows MSVC
```

The produced binary is your platform’s standard app / executable name for the `.pro` target.

---

## Using the sample UI

1. Enter an **http** or **https** URL.
2. Choose a **download folder** (browse with **…**).
3. Click **Download**.

Progress and messages appear on the **status bar**; the progress bar shows percentage when total size is known.

> **Note:** The bundled `MainWindow` is a minimal example. Review `onDownloadCompleted` if you need to **keep** the saved file—adjust there for your product behavior.

---

## Reusing `FileDownloader`

The logic lives in **`filedownloader.h`** / **`filedownloader.cpp`**.

1. Implement **`FileDownloadEvents`**:

   - `onProgressChanged(float)` — 0–100 when size is known  
   - `onDownloadCompleted(const QString& outputFilePath)` — final path after rename  
   - `onError(const QString&)` — errors and retry messages  

2. Construct **`FileDownloader`** with your events object.

3. Call **`download(url, outputFolder)`** to start (or restart) a job.

4. Optionally call **`abort()`** to cancel, delete `.part`, and stop retries.

Output layout:

- **Partial:** `\<folder\>/\<derived-name\>.part`  
- **Done:** `\<folder\>/\<derived-name\>` (name from URL path, or `download.bin` if empty)

---

## Project layout

| Path | Role |
|------|------|
| `QtResumedDownloader.pro` | qmake project (network + widgets) |
| `main.cpp` | Application entry |
| `mainwindow.*` / `mainwindow.ui` | Sample window |
| `filedownloader.*` | Downloader + event interface |
| `LICENSE` | GPL v3 (see file for full text) |

---

## License

This project is licensed under the **GNU General Public License v3.0** — see [`LICENSE`](LICENSE).
