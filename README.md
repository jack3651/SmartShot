# SmartShot

**One-key screenshots for Windows — built for games and fullscreen.**
**Windows 一鍵截圖，主打遊戲與全螢幕畫面。**

[English](#english) ｜ [繁體中文](#繁體中文)

Download / 下載：**[itch.io](https://jack3651.itch.io/smartshot)** ｜ [GitHub Releases](https://github.com/jack3651/SmartShot/releases)

---

## English

Press **PrtSc** and SmartShot saves a PNG of the screen you're looking at. No windows pop up, no editor opens — you just hear a shutter sound and keep playing.

### Features

- **One key, zero interruptions.** A successful screenshot only plays a shutter sound. Nothing steals focus from your game.
- **Captures the right screen.** Only the monitor with the active window is saved, not your whole multi-monitor desktop.
- **Backup hotkey `Ctrl+Alt+S`** for when PrtSc is taken by another app or a game.
- **Doesn't slow your game down.** PNG encoding runs on a low-priority background thread.
- **Single portable exe** (~360 KB). No installer, no runtime, no network access.
- **English and Traditional Chinese** interface (follows your Windows language; switch from the tray menu).

### Getting started

1. Download and unzip, then double-click `SmartShot.exe`. It lives in the system tray (bottom-right).
2. Press **PrtSc**. When you hear the shutter, you're all set.
3. Screenshots are saved to `Pictures\SmartShot` as `SmartShot_YYYYMMDD_HHMMSS.png`.

Right-click the tray icon for: Open save folder · Choose save folder · Start with Windows · Shutter sound · Language · Exit.

### Windows 11: PrtSc opens the Snipping Tool

By default, Windows 11 uses PrtSc to open the Snipping Tool. SmartShot detects this and offers to open the right Settings page. Turn off **"Use the Print screen key to open screen capture"** (Settings → Bluetooth & devices → Keyboard, or Accessibility → Keyboard on older builds). It takes effect immediately; no restart needed. Until then, use `Ctrl+Alt+S`.

### Games and fullscreen

- Windowed and **borderless fullscreen** games capture normally.
- Some **exclusive fullscreen** games may come out black with the free version. You'll hear a warning or error sound instead of the shutter. Switching the game to borderless mode usually fixes it. (The black image is still saved, so you never lose a real black frame.)
- Games running as administrator may not receive PrtSc. Run SmartShot as administrator too if needed.
- Some games use PrtSc for their own screenshots (e.g. World of Warcraft). Change the in-game key or use `Ctrl+Alt+S`.

### "Windows protected your PC"

SmartShot isn't digitally signed, so Windows SmartScreen may warn you on first launch. Make sure you downloaded it from the itch.io page or GitHub Releases above, then click **More info → Run anyway**. If your antivirus quarantines it, restore the file and add an exclusion, or build it yourself from source.

### Settings and uninstalling

Settings are stored in the registry at `HKCU\Software\SmartShot`. "Start with Windows" adds an entry under `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`. To uninstall: untick "Start with Windows", exit SmartShot, delete the exe, and optionally delete the registry key above.

### Privacy

SmartShot has no network code. It never uploads anything; screenshots stay in the folder you choose. (If that folder is synced by OneDrive, OneDrive will sync it. SmartShot tells you when that's the case.)

### Build from source

Requires MinGW-w64 (GCC) or MSVC.

```
build_mingw.bat      :: MinGW-w64: windres + g++
build_msvc.bat       :: MSVC: run from a Developer Command Prompt
```

### License

SmartShot is free software under the **GNU General Public License v3.0**. See [LICENSE](LICENSE).

**Contributing:** By submitting a pull request, you agree that your contribution is licensed under GPL-3.0 and that the author may also use it under other licenses (including in SmartShot Pro, a paid edition that adds DXGI capture for exclusive fullscreen games).

---

## 繁體中文

按 **PrtSc**，SmartShot 就把你正在看的那個螢幕存成 PNG。不跳視窗、不開編輯器，只會聽到一聲快門，然後繼續玩。

### 特色

- **一鍵、零打擾**：截圖成功只播快門聲，不會搶走遊戲的焦點。
- **截對螢幕**：只存目前視窗所在的那個螢幕，不會把多螢幕整個桌面拼在一起。
- **備援熱鍵 `Ctrl+Alt+S`**：PrtSc 被其他程式或遊戲佔用時使用。
- **不拖慢遊戲**：PNG 編碼交給低優先權的背景執行緒。
- **單一免安裝 exe**（約 360 KB），不需要安裝任何執行環境，也不連網。
- **繁體中文與英文介面**（預設跟 Windows 語言，可從系統列選單切換）。

### 開始使用

1. 下載並解壓縮，雙擊 `SmartShot.exe`。程式會常駐在右下角的系統列。
2. 按一下 **PrtSc**，聽到快門聲就完成設定。
3. 截圖存在「圖片\SmartShot」，檔名是 `SmartShot_日期_時間.png`。

對系統列圖示按右鍵：開啟儲存資料夾・選擇儲存資料夾・開機時自動啟動・截圖快門聲・語言・結束。

### Windows 11：PrtSc 會開啟剪取工具

Windows 11 預設把 PrtSc 交給剪取工具。SmartShot 偵測到時會詢問是否打開設定頁，請關閉 **「使用 Print Screen 鍵開啟螢幕擷取」**（設定 → 藍牙與裝置 → 鍵盤；較舊的版本在 協助工具 → 鍵盤）。關掉後立即生效，不用重開程式。在那之前可以先用 `Ctrl+Alt+S`。

### 遊戲與全螢幕

- 視窗模式與**無邊框全螢幕**的遊戲可以正常截圖。
- 部分**獨佔全螢幕**遊戲用免費版可能截到全黑，這時會聽到警告音或錯誤音，而不是快門聲。把遊戲改成無邊框模式通常就能解決。（全黑的圖也會照樣存檔，不會漏掉真正的黑畫面。）
- 以系統管理員權限執行的遊戲可能收不到 PrtSc，需要時請也用系統管理員身分執行 SmartShot。
- 有些遊戲自己也用 PrtSc 截圖（例如魔獸世界），請改掉遊戲內的截圖鍵，或改用 `Ctrl+Alt+S`。

### 「Windows 已保護您的電腦」

SmartShot 沒有數位簽章，第一次執行時 Windows SmartScreen 可能會跳出警告。請先確認檔案是從上方的 itch.io 頁面或 GitHub Releases 下載的，再按 **「其他資訊 → 仍要執行」**。如果防毒軟體把檔案隔離了，可以還原並加入排除清單，或自行從原始碼編譯。

### 設定與移除

設定存在登錄檔 `HKCU\Software\SmartShot`；「開機時自動啟動」會在 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` 加一筆。移除方式：取消「開機時自動啟動」、結束 SmartShot、刪除 exe，需要的話再刪除上面的登錄檔機碼。

### 隱私

SmartShot 沒有任何連網程式碼，不會上傳任何東西，截圖只會存在你選的資料夾。（如果那個資料夾有 OneDrive 同步，OneDrive 會把它同步上去，SmartShot 會在第一次執行時提醒你。）

### 自行編譯

需要 MinGW-w64（GCC）或 MSVC。

```
build_mingw.bat      :: MinGW-w64：windres + g++
build_msvc.bat       :: MSVC：在 Developer Command Prompt 中執行
```

### 授權

SmartShot 是以 **GNU 通用公共授權條款第 3 版（GPL-3.0）** 釋出的自由軟體，詳見 [LICENSE](LICENSE)。

**貢獻：** 送出 Pull Request 即表示你同意你的貢獻以 GPL-3.0 授權，且作者也可以用其他授權使用它（包括 SmartShot Pro：加入 DXGI 擷取、支援獨佔全螢幕遊戲的付費版）。
