# Line Remove Ads

<p align="center"><img src="assets/icon.png" width="128" alt="Line Remove Ads icon"></p>

A tiny Windows tray utility that removes the advertisement banners from **LINE for Windows** —
the banner at the bottom of the main window and the one above the conversation in community chats.

- Lives in the system tray only: no window, no console.
- Does **not** modify, replace or inject anything into LINE's files. It patches the running process
  in memory, so LINE's file integrity checks are never triggered.
- Starts with Windows (can be turned off) and re-applies itself every time LINE is launched.
- Single native executable, no installer, no runtime dependencies (~200 KB).

> **Disclaimer.** This project is not affiliated with, endorsed by or sponsored by LINE Corporation.
> "LINE" is a trademark of its owner. Modifying a third-party application may violate its terms of
> service. This software is provided for educational purposes, as-is, and you use it at your own risk.

## Download and use

1. Download the latest `LineRemoveAds-vX.Y.Z-win-x64.zip` from the
   [Releases](https://github.com/RouZongWorks/line-remove-ads/releases) page and extract it anywhere
   you want to keep it (for example `C:\Tools\Line Remove Ads`).
2. Run `Line Remove Ads.exe`. An icon (a LINE bubble with a "prohibited" sign) appears in the system tray.
   Windows may hide new tray icons under the **^** overflow arrow — drag it onto the taskbar to keep it visible.
3. Start or restart LINE. The ads are gone as soon as LINE has finished starting.

The program stores only two small registry values (the startup entry and a first-run marker) and makes no
network connections of its own.

### Tray menu

Right-click the tray icon:

| Item | Description |
| --- | --- |
| **Run at startup** | Checkable. Starts the program when you sign in. **Enabled by default** the first time the program runs. If you move the program to another folder, the entry is updated on the next launch. |
| **About** | Opens the project page in your browser. |
| **Exit** | Closes the program and removes the tray icon. Ads come back the next time LINE starts. |

Hover over the icon to see the current status:

- `waiting for LINE` — LINE is not running yet.
- `LINE ads removed` — LINE was found and patched.
- `patch failed (LINE may have been updated)` — see [After a LINE update](#after-a-line-update).

### Uninstall

Choose **Run at startup** to untick it, choose **Exit**, then delete the folder. To remove the remaining
registry key, delete `HKEY_CURRENT_USER\Software\LineRemoveAds`. Nothing else is left behind.

## How it works

LINE for Windows is a Qt application, and every ad it shows is requested and handled by a C++ class,
`AdvertisementManager`. The program:

1. Polls for a running `LINE.exe` process (twice per second, using a toolhelp snapshot).
2. Scans that process's executable memory for the machine-code prologue (a byte signature) of two methods:
   - `AdvertisementManager::requestAdvertisements` — asks the server for ads.
   - `AdvertisementManager::onGetAdvertisements` — handles the ads the server returns.
3. Requires each signature to match **exactly once** — if a signature is missing or ambiguous it writes nothing.
4. Overwrites the first three bytes of each function with `31 C0 C3` (`xor eax, eax; ret`), so both return
   immediately. No ads are requested, none are processed, and none are shown or rotated in.

Because the patch lives only in the memory of that one process, it vanishes when LINE exits and is applied
again on the next launch. Signatures are located at runtime, so they do not depend on fixed addresses
(which change on every launch). Already-patched processes are recognised and left alone.

Tested with LINE for Windows 26.6 on Windows 11 (x64).

## After a LINE update

LINE updates can change the compiled code. If the signatures no longer match, the program refuses to patch
and the tooltip shows `patch failed`; LINE keeps working normally, you just see ads again. Finding the new
signatures takes a few minutes with the helper scripts in [`tools/research`](tools/research):

```text
cd tools\research
python -I imgstr.py <pid> "AdvertisementManager"        # find ad-related strings in the LINE image
python -I xref.py   <pid> "<exact log string>"           # find code that references a string
python -I disasm.py <pid> <address> 160 200              # disassemble around it (needs: pip install capstone)
python -I callers.py <pid> <function address>            # find callers of a function
python -I memscan.py <pid> <pattern> ...                 # generic memory string search
```

`<pid>` is the process ID of the main `LINE.exe` (Task Manager). Typical workflow: search for the
`[AdvertisementManager][LAD] ...` log strings, find the function that references them, copy the first
~20–40 bytes of the function, and update the `Targets()` table in `src/main.cpp`.

## Building from source

Requirements: Windows 10/11 x64 and Visual Studio 2019 or newer (or the Build Tools) with the
**Desktop development with C++** workload. No third-party libraries are used.

```bat
build.bat
```

The result is `bin\Line Remove Ads.exe`. `package.bat` additionally creates the release zip in `dist\`.

The application icon is generated from vector code. To change the artwork, edit
`tools/icon/icon_draw.h` and run `tools\icon\build_icon.bat`, which regenerates `assets/icon.ico`
and `assets/icon.png`.

## Project layout

```text
assets/              application icon (icon.ico, icon.png)
src/main.cpp         the whole application: tray icon, menu, autostart, patcher
src/app.rc           icon and version resources
src/app.manifest     DPI awareness and execution level
tools/icon/          vector icon source and .ico generator
tools/research/      helper scripts for finding signatures after a LINE update
build.bat            builds the executable
package.bat          builds and creates the release zip
vcenv.bat            locates Visual Studio and sets up the x64 toolchain
```

## Limitations

- Windows x64 only. LINE must run as the same user (not elevated differently) as this program.
- Only the `AdvertisementManager` ad path is patched. Other ad surfaces, if LINE adds them, are not covered.
- Anti-virus products occasionally flag any program that writes to another process's memory. The source is
  fully available here so you can review and build it yourself.

## License

[MIT](LICENSE)
