// ============================================================
//  產生 SmartShot 程式圖示（smartshot.ico）與預覽圖
//  設計：綠色漸層圓角方塊＋雙山（前白後淡綠）＋黃色太陽
//  編譯：g++ -municode -O2 -o make_icon.exe make_icon.cpp -static -lgdiplus -lole32
//  用法：make_icon.exe <輸出 .ico> [預覽 .png]
//        make_icon.exe --cover <輸出 .png>   itch.io 封面（630x500）
// ============================================================
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <stdio.h>
#include <vector>

using namespace Gdiplus;

static const int kSizes[] = { 16, 20, 24, 32, 40, 48, 64, 256 };

static void AddRoundRect(GraphicsPath& p, RectF r, REAL rad) {
    const REAL d = rad * 2;
    p.AddArc(r.X, r.Y, d, d, 180, 90);
    p.AddArc(r.GetRight() - d, r.Y, d, d, 270, 90);
    p.AddArc(r.GetRight() - d, r.GetBottom() - d, d, d, 0, 90);
    p.AddArc(r.X, r.GetBottom() - d, d, d, 90, 90);
    p.CloseFigure();
}

static void Draw(Graphics& g, int S) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    g.Clear(Color(0, 0, 0, 0));

    // 小尺寸時元素相對加大，避免糊掉
    const bool tiny = S <= 32;
    const REAL margin = S * (tiny ? 0.0f : 0.04f);
    RectF bg(margin, margin, S - 2 * margin, S - 2 * margin);

    GraphicsPath bgPath;
    AddRoundRect(bgPath, bg, bg.Width * 0.22f);
    LinearGradientBrush grad(PointF(bg.X, bg.Y), PointF(bg.GetRight(), bg.GetBottom()),
                             Color(255, 34, 197, 94),     // #22C55E
                             Color(255, 21, 101, 52));    // #156534
    g.FillPath(&grad, &bgPath);

    // 以底色方塊為單位座標（0～1）
    auto P = [&](REAL x, REAL y) { return PointF(bg.X + bg.Width * x, bg.Y + bg.Height * y); };

    // 黃色太陽（右上）
    const REAL sunR = bg.Width * (tiny ? 0.13f : 0.11f);
    const PointF sun = P(0.74f, tiny ? 0.25f : 0.24f);
    SolidBrush yellow(Color(255, 250, 204, 21));         // #FACC15
    g.FillEllipse(&yellow, sun.X - sunR, sun.Y - sunR, sunR * 2, sunR * 2);

    // 雙山：45 度斜邊，山腳留一條綠色地面，淺色工作列上才看得出方塊輪廓；左右超出由圓角裁掉
    g.SetClip(&bgPath);
    SolidBrush white(Color(255, 255, 255, 255));
    const REAL base = tiny ? 0.80f : 0.78f;
    const REAL bx = 0.72f, by = tiny ? 0.40f : 0.42f;   // 後山山頂
    const REAL fx = 0.38f, fy = tiny ? 0.26f : 0.30f;   // 前山山頂
    const PointF back[3]  = { P(bx - (base - by), base), P(bx, by), P(bx + (base - by), base) };
    const PointF front[3] = { P(fx - (base - fy), base), P(fx, fy), P(fx + (base - fy), base) };
    SolidBrush pale(Color(255, 187, 247, 208));          // #BBF7D0 後山較淡
    g.FillPolygon(&pale, back, 3);
    g.FillPolygon(&white, front, 3);
    g.ResetClip();
}

// 取 32bpp straight-alpha BGRA，top-down
static std::vector<BYTE> Render(int S) {
    Bitmap bmp(S, S, PixelFormat32bppARGB);
    {
        Graphics g(&bmp);
        Draw(g, S);
    }
    std::vector<BYTE> px((size_t)S * S * 4);
    BitmapData bd;
    Rect r(0, 0, S, S);
    bmp.LockBits(&r, ImageLockModeRead, PixelFormat32bppARGB, &bd);
    for (int y = 0; y < S; ++y)
        memcpy(&px[(size_t)y * S * 4], (BYTE*)bd.Scan0 + (size_t)y * bd.Stride, (size_t)S * 4);
    bmp.UnlockBits(&bd);
    return px;
}

static bool GetPngClsid(CLSID* c) {
    return SUCCEEDED(CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}", c));
}

static std::vector<BYTE> EncodePng(int S, const std::vector<BYTE>& px) {
    Bitmap bmp(S, S, S * 4, PixelFormat32bppARGB, const_cast<BYTE*>(px.data()));
    IStream* st = NULL;
    CreateStreamOnHGlobal(NULL, TRUE, &st);
    CLSID png;
    GetPngClsid(&png);
    bmp.Save(st, &png, NULL);
    STATSTG ss;
    st->Stat(&ss, STATFLAG_NONAME);
    std::vector<BYTE> out((size_t)ss.cbSize.QuadPart);
    LARGE_INTEGER zero = {};
    st->Seek(zero, STREAM_SEEK_SET, NULL);
    ULONG got = 0;
    st->Read(out.data(), (ULONG)out.size(), &got);
    st->Release();
    return out;
}

// ICO 內的 DIB：BITMAPINFOHEADER（高度×2）＋bottom-up BGRA＋全 0 的 AND 遮罩
static std::vector<BYTE> EncodeDib(int S, const std::vector<BYTE>& px) {
    BITMAPINFOHEADER bh = {};
    bh.biSize = sizeof(bh);
    bh.biWidth = S;
    bh.biHeight = S * 2;
    bh.biPlanes = 1;
    bh.biBitCount = 32;
    const size_t maskRow = ((S + 31) / 32) * 4;
    std::vector<BYTE> out(sizeof(bh) + (size_t)S * S * 4 + maskRow * S, 0);
    memcpy(out.data(), &bh, sizeof(bh));
    for (int y = 0; y < S; ++y)
        memcpy(&out[sizeof(bh) + (size_t)y * S * 4], &px[(size_t)(S - 1 - y) * S * 4], (size_t)S * 4);
    return out;
}

static bool WriteIco(const wchar_t* path) {
    std::vector<std::vector<BYTE>> imgs;
    for (int S : kSizes) {
        std::vector<BYTE> px = Render(S);
        imgs.push_back(S >= 256 ? EncodePng(S, px) : EncodeDib(S, px));
    }
    const int n = (int)imgs.size();
    std::vector<BYTE> out(6 + 16 * n);
    WORD* h = (WORD*)out.data();
    h[0] = 0; h[1] = 1; h[2] = (WORD)n;
    DWORD offset = (DWORD)out.size();
    for (int i = 0; i < n; ++i) {
        BYTE* e = &out[6 + 16 * i];
        e[0] = kSizes[i] >= 256 ? 0 : (BYTE)kSizes[i];
        e[1] = e[0];
        *(WORD*)(e + 4)  = 1;
        *(WORD*)(e + 6)  = 32;
        *(DWORD*)(e + 8)  = (DWORD)imgs[i].size();
        *(DWORD*)(e + 12) = offset;
        offset += (DWORD)imgs[i].size();
    }
    for (auto& img : imgs) out.insert(out.end(), img.begin(), img.end());

    FILE* f = _wfopen(path, L"wb");
    if (!f) return false;
    fwrite(out.data(), 1, out.size(), f);
    fclose(f);
    return true;
}

// 預覽：256 大圖＋各尺寸放大 4 倍（最近鄰），分別放在亮、暗底色上
static void WritePreview(const wchar_t* path) {
    const int W = 256 + 24 + 4 * (16 + 20 + 24 + 32) + 5 * 12, H = 256 + 16;
    Bitmap sheet(W, H * 2, PixelFormat32bppARGB);
    Graphics g(&sheet);
    g.SetInterpolationMode(InterpolationModeNearestNeighbor);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    SolidBrush light(Color(255, 243, 243, 243)), dark(Color(255, 32, 32, 32));
    g.FillRectangle(&light, 0, 0, W, H);
    g.FillRectangle(&dark, 0, H, W, H);
    for (int row = 0; row < 2; ++row) {
        const int y0 = row * H + 8;
        std::vector<BYTE> big = Render(256);
        Bitmap b(256, 256, 256 * 4, PixelFormat32bppARGB, big.data());
        g.DrawImage(&b, 8, y0, 256, 256);
        int x = 256 + 24;
        for (int S : { 16, 20, 24, 32 }) {
            std::vector<BYTE> px = Render(S);
            Bitmap s(S, S, S * 4, PixelFormat32bppARGB, px.data());
            g.DrawImage(&s, x, y0, S * 4, S * 4);
            x += S * 4 + 12;
        }
    }
    CLSID png;
    GetPngClsid(&png);
    sheet.Save(path, &png, NULL);
}

// itch.io 封面：深綠底＋程式圖示＋名稱與標語。縮成 315x250 的縮圖時文字也要看得清楚。
static bool WriteCover(const wchar_t* path) {
    const int W = 630, H = 500;
    Bitmap cover(W, H, PixelFormat32bppARGB);
    Graphics g(&cover);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    LinearGradientBrush bg(PointF(0, 0), PointF((REAL)W, (REAL)H),
                           Color(255, 15, 36, 27),      // #0F241B
                           Color(255, 20, 64, 44));     // #14402C
    g.FillRectangle(&bg, 0, 0, W, H);

    // 圖示：另外畫好再貼上（Draw 會 Clear 整張畫布，不能直接畫在封面上）
    const int S = 200;
    std::vector<BYTE> px = Render(S);
    Bitmap icon(S, S, S * 4, PixelFormat32bppARGB, px.data());
    g.DrawImage(&icon, (W - S) / 2, 52, S, S);

    StringFormat center;
    center.SetAlignment(StringAlignmentCenter);
    FontFamily latin(L"Segoe UI"), cjk(L"Microsoft JhengHei");
    Font title(&latin, 64, FontStyleBold, UnitPixel);
    Font sub(&latin, 26, FontStyleRegular, UnitPixel);
    Font subZh(&cjk, 26, FontStyleBold, UnitPixel);
    SolidBrush white(Color(255, 255, 255, 255));
    SolidBrush yellow(Color(255, 250, 204, 21));         // 與圖示的太陽同色
    SolidBrush soft(Color(255, 187, 247, 208));          // 與後山同色
    g.DrawString(L"SmartShot", -1, &title, RectF(0, 270, (REAL)W, 80), &center, &white);
    g.DrawString(L"One-key screenshots for games", -1, &sub, RectF(0, 352, (REAL)W, 40), &center, &yellow);
    g.DrawString(L"遊戲一鍵截圖・按 PrtSc 就好", -1, &subZh, RectF(0, 396, (REAL)W, 40), &center, &soft);

    CLSID png;
    GetPngClsid(&png);
    return cover.Save(path, &png, NULL) == Ok;
}

int wmain(int argc, wchar_t** argv) {
    const bool coverMode = argc >= 2 && wcscmp(argv[1], L"--cover") == 0;
    if (argc < 2 || (coverMode && argc < 3)) {
        fwprintf(stderr, L"用法：make_icon.exe <輸出 .ico> [預覽 .png]\n"
                         L"      make_icon.exe --cover <輸出 .png>\n");
        return 1;
    }
    GdiplusStartupInput gsi;
    ULONG_PTR tok;
    GdiplusStartup(&tok, &gsi, NULL);
    bool ok;
    if (coverMode) {
        ok = WriteCover(argv[2]);
    } else {
        ok = WriteIco(argv[1]);
        if (ok && argc >= 3) WritePreview(argv[2]);
    }
    GdiplusShutdown(tok);
    return ok ? 0 : 1;
}
