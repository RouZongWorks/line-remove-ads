# Line Remove Ads

<p align="center"><img src="assets/icon.png" width="128" alt="Line Remove Ads icon"></p>

A tiny Windows tray utility that removes the advertisement banners from **LINE for Windows** —
the banner at the bottom of the main window and the one above the conversation in community chats.

<sub>一個小型的 Windows 系統匣工具，用來移除 **LINE 電腦版** 的廣告橫幅——包含主視窗下方的橫幅，以及社群聊天室對話上方的橫幅。</sub>

- Lives in the system tray only: no window, no console.
- Does **not** modify, replace or inject anything into LINE's files. It patches the running process
  in memory, so LINE's file integrity checks are never triggered.
- Starts with Windows (can be turned off) and re-applies itself every time LINE is launched.
- Single native executable, no installer, no runtime dependencies (~200 KB).

<sub>
- 只存在於系統匣：沒有視窗、沒有主控台。<br>
- **不會**修改、替換或注入 LINE 的任何檔案，只在記憶體中修補執行中的行程，因此不會觸發 LINE 的檔案完整性檢查。<br>
- 可隨 Windows 啟動（可關閉），並在每次 LINE 啟動時自動重新套用。<br>
- 單一原生執行檔，免安裝、無需執行階段依賴（約 200 KB）。
</sub>

> **Disclaimer.** This project is not affiliated with, endorsed by or sponsored by LINE Corporation.
> "LINE" is a trademark of its owner. Modifying a third-party application may violate its terms of
> service. This software is provided for educational purposes, as-is, and you use it at your own risk.
>
> <sub>**免責聲明。** 本專案與 LINE Corporation 無關，亦未獲其認可或贊助。「LINE」為其所有者之商標。修改第三方應用程式可能違反其服務條款。本軟體僅供教育用途，依現狀提供，使用風險自負。</sub>

## Download and use

<sub>下載與使用</sub>

1. Download the latest `LineRemoveAds-vX.Y.Z-win-x64.zip` from the
   [Releases](https://github.com/RouZongWorks/line-remove-ads/releases) page and extract it anywhere
   you want to keep it (for example `C:\Tools\Line Remove Ads`).
2. Run `Line Remove Ads.exe`. An icon (a LINE bubble with a "prohibited" sign) appears in the system tray.
   Windows may hide new tray icons under the **^** overflow arrow — drag it onto the taskbar to keep it visible.
3. Start or restart LINE. The ads are gone as soon as LINE has finished starting.

<sub>
1. 從 [Releases](https://github.com/RouZongWorks/line-remove-ads/releases) 頁面下載最新的 `LineRemoveAds-vX.Y.Z-win-x64.zip`，解壓縮到想長期存放的位置（例如 `C:\Tools\Line Remove Ads`）。<br>
2. 執行 `Line Remove Ads.exe`，系統匣會出現圖示（LINE 對話框加上禁止符號）。Windows 可能把新圖示收在 **^** 溢位箭頭裡，把它拖到工作列即可常駐顯示。<br>
3. 啟動或重新啟動 LINE，LINE 啟動完成後廣告就會消失。
</sub>

The program stores only two small registry values (the startup entry and a first-run marker) and makes no
network connections of its own.

<sub>本程式只會寫入兩個小型登錄值（開機啟動項目與首次執行標記），自身不會建立任何網路連線。</sub>

### Tray menu

<sub>系統匣選單</sub>

Right-click the tray icon:

<sub>在系統匣圖示上按右鍵：</sub>

| Item | Description |
| --- | --- |
| **Run at startup** | Checkable. Starts the program when you sign in. **Enabled by default** the first time the program runs. If you move the program to another folder, the entry is updated on the next launch. |
| **About** | Opens the project page in your browser. |
| **Exit** | Closes the program and removes the tray icon. Ads come back the next time LINE starts. |

<sub>
**Run at startup（開機執行）**：可勾選，登入時自動啟動本程式。首次執行時**預設開啟**。若把程式移到其他資料夾，下次啟動時會自動更新啟動項目。<br>
**About（關於）**：在瀏覽器中開啟專案頁面。<br>
**Exit（退出）**：關閉程式並移除系統匣圖示。下次 LINE 啟動時廣告會再出現。
</sub>

Hover over the icon to see the current status:

- `waiting for LINE` — LINE is not running yet.
- `LINE ads removed` — LINE was found and patched.
- `patch failed (LINE may have been updated)` — see [After a LINE update](#after-a-line-update).

<sub>
將滑鼠移到圖示上可查看目前狀態：<br>
`waiting for LINE`——LINE 尚未執行。<br>
`LINE ads removed`——已找到 LINE 並完成修補。<br>
`patch failed (LINE may have been updated)`——修補失敗（LINE 可能已更新），請見 [After a LINE update](#after-a-line-update)。
</sub>

### Uninstall

<sub>解除安裝</sub>

Choose **Run at startup** to untick it, choose **Exit**, then delete the folder. To remove the remaining
registry key, delete `HKEY_CURRENT_USER\Software\LineRemoveAds`. Nothing else is left behind.

<sub>點選 **Run at startup** 取消勾選，再選 **Exit**，然後刪除資料夾。若要清除殘留的登錄機碼，刪除 `HKEY_CURRENT_USER\Software\LineRemoveAds` 即可，不會留下其他東西。</sub>

## How it works

<sub>運作原理</sub>

LINE for Windows is a Qt application, and every ad it shows is requested and handled by a C++ class,
`AdvertisementManager`. The program:

<sub>LINE 電腦版是 Qt 應用程式，所有廣告都由 C++ 類別 `AdvertisementManager` 負責請求與處理。本程式的做法：</sub>

1. Polls for a running `LINE.exe` process (twice per second, using a toolhelp snapshot).
2. Scans that process's executable memory for the machine-code prologue (a byte signature) of two methods:
   - `AdvertisementManager::requestAdvertisements` — asks the server for ads.
   - `AdvertisementManager::onGetAdvertisements` — handles the ads the server returns.
3. Requires each signature to match **exactly once** — if a signature is missing or ambiguous it writes nothing.
4. Overwrites the first three bytes of each function with `31 C0 C3` (`xor eax, eax; ret`), so both return
   immediately. No ads are requested, none are processed, and none are shown or rotated in.

<sub>
1. 以 toolhelp 快照輪詢執行中的 `LINE.exe` 行程（每秒兩次）。<br>
2. 在該行程的可執行記憶體中，搜尋兩個方法的機器碼開頭（位元組特徵碼）：`AdvertisementManager::requestAdvertisements`（向伺服器請求廣告）與 `AdvertisementManager::onGetAdvertisements`（處理伺服器回傳的廣告）。<br>
3. 每個特徵碼必須**剛好符合一次**；若找不到或有歧義，就不寫入任何內容。<br>
4. 將每個函式的前三個位元組改寫為 `31 C0 C3`（`xor eax, eax; ret`），使兩者立即返回。如此不會請求廣告、不會處理廣告，也不會顯示或輪播廣告。
</sub>

Because the patch lives only in the memory of that one process, it vanishes when LINE exits and is applied
again on the next launch. Signatures are located at runtime, so they do not depend on fixed addresses
(which change on every launch). Already-patched processes are recognised and left alone.

<sub>修補只存在於該行程的記憶體中，LINE 結束後即消失，下次啟動再重新套用。特徵碼在執行時才定位，不依賴固定位址（每次啟動位址都會變）。已修補的行程會被辨識並略過。</sub>

Tested with LINE for Windows 26.6 on Windows 11 (x64).

<sub>已在 Windows 11 (x64) 上以 LINE 電腦版 26.6 測試。</sub>

## After a LINE update

<sub>LINE 更新之後</sub>

LINE updates can change the compiled code. If the signatures no longer match, the program refuses to patch
and the tooltip shows `patch failed`; LINE keeps working normally, you just see ads again. Finding the new
signatures takes a few minutes with the helper scripts in [`tools/research`](tools/research):

<sub>LINE 更新可能改變編譯後的程式碼。若特徵碼不再符合，程式會拒絕修補並在提示中顯示 `patch failed`；LINE 仍正常運作，只是會再看到廣告。使用 [`tools/research`](tools/research) 中的輔助腳本，幾分鐘即可找出新的特徵碼：</sub>

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

<sub>`<pid>` 是主要 `LINE.exe` 的行程 ID（可在工作管理員查看）。典型流程：搜尋 `[AdvertisementManager][LAD] ...` 記錄字串，找出引用它們的函式，複製函式開頭約 20–40 個位元組，然後更新 `src/main.cpp` 中的 `Targets()` 表。</sub>

## Building from source

<sub>從原始碼編譯</sub>

Requirements: Windows 10/11 x64 and Visual Studio 2019 or newer (or the Build Tools) with the
**Desktop development with C++** workload. No third-party libraries are used.

<sub>需求：Windows 10/11 x64，以及安裝「**使用 C++ 的桌面開發**」工作負載的 Visual Studio 2019 或更新版本（或 Build Tools）。不使用任何第三方函式庫。</sub>

```bat
build.bat
```

The result is `bin\Line Remove Ads.exe`. `package.bat` additionally creates the release zip in `dist\`.

<sub>輸出為 `bin\Line Remove Ads.exe`。`package.bat` 會另外在 `dist\` 建立發佈用 zip。</sub>

The application icon is generated from vector code. To change the artwork, edit
`tools/icon/icon_draw.h` and run `tools\icon\build_icon.bat`, which regenerates `assets/icon.ico`
and `assets/icon.png`.

<sub>應用程式圖示由向量程式碼產生。要修改圖案，請編輯 `tools/icon/icon_draw.h` 並執行 `tools\icon\build_icon.bat`，即會重新產生 `assets/icon.ico` 與 `assets/icon.png`。</sub>

## Project layout

<sub>專案結構</sub>

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

<sub>
`assets/`：應用程式圖示｜`src/main.cpp`：整個程式（系統匣圖示、選單、開機啟動、修補器）｜`src/app.rc`：圖示與版本資源｜`src/app.manifest`：DPI 感知與執行權限｜`tools/icon/`：向量圖示原始碼與 .ico 產生器｜`tools/research/`：LINE 更新後尋找特徵碼的輔助腳本｜`build.bat`：編譯執行檔｜`package.bat`：編譯並建立發佈 zip｜`vcenv.bat`：尋找 Visual Studio 並設定 x64 工具鏈
</sub>

## Limitations

<sub>限制</sub>

- Windows x64 only. LINE must run as the same user (not elevated differently) as this program.
- Only the `AdvertisementManager` ad path is patched. Other ad surfaces, if LINE adds them, are not covered.
- Anti-virus products occasionally flag any program that writes to another process's memory. The source is
  fully available here so you can review and build it yourself.

<sub>
- 僅支援 Windows x64。LINE 必須與本程式以相同使用者身分執行（權限提升狀態不可不同）。<br>
- 只修補 `AdvertisementManager` 這條廣告路徑；若 LINE 日後新增其他廣告位置則不涵蓋。<br>
- 防毒軟體偶爾會將任何寫入其他行程記憶體的程式標示為可疑。完整原始碼公開於此，你可以自行檢視並編譯。
</sub>

## License

<sub>授權</sub>

[MIT](LICENSE)
