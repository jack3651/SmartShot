// ============================================================
//  產生 SmartShot 程式圖示（smartshot.ico）與預覽圖
//  設計：藍紫漸層圓角方塊＋白色觀景窗四角框＋中央四芒星
//  編譯：g++ -municode -O2 -o make_icon.exe make_icon.cpp -static -lgdiplus -lole32
//  用法：make_icon.exe <輸出 .ico> [預覽 .png]
// ============================================================
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <math.h>
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

// 四芒星：外圈 4 個尖端、內圈 4 個凹點
static void AddSparkle(GraphicsPath& p, REAL cx, REAL cy, REAL r, REAL inner) {
    PointF pts[8];
    for (int i = 0; i < 8; ++i) {
        const double a = -M_PI / 2 + i * M_PI / 4;
        const REAL rr = (i % 2 == 0) ? r : r * inner;
        pts[i] = PointF(cx + (REAL)(cos(a) * rr), cy + (REAL)(sin(a) * rr));
    }
    p.AddPolygon(pts, 8);
}

static void Draw(Graphics& g, int S) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    g.Clear(Color(0, 0, 0, 0));

    // 小尺寸時線條相對加粗，避免糊掉
    const bool tiny = S <= 32;
    const REAL margin = S * (tiny ? 0.0f : 0.04f);
    RectF bg(margin, margin, S - 2 * margin, S - 2 * margin);

    GraphicsPath bgPath;
    AddRoundRect(bgPath, bg, bg.Width * 0.22f);
    LinearGradientBrush grad(PointF(bg.X, bg.Y), PointF(bg.GetRight(), bg.GetBottom()),
                             Color(255, 59, 130, 246),    // #3B82F6
                             Color(255, 124, 58, 237));   // #7C3AED
    g.FillPath(&grad, &bgPath);

    // 觀景窗四角框
    const REAL inset = bg.X + bg.Width * (tiny ? 0.19f : 0.22f);
    const REAL outer   = bg.GetRight() - (inset - bg.X);
    const REAL len   = bg.Width * (tiny ? 0.15f : 0.20f);
    Pen pen(Color(255, 255, 255, 255), bg.Width * (tiny ? 0.10f : 0.075f));
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    pen.SetLineJoin(LineJoinRound);
    const PointF corners[4][3] = {
        { PointF(inset, inset + len), PointF(inset, inset), PointF(inset + len, inset) },
        { PointF(outer - len, inset),   PointF(outer, inset),   PointF(outer, inset + len) },
        { PointF(outer, outer - len),     PointF(outer, outer),     PointF(outer - len, outer) },
        { PointF(inset + len, outer),   PointF(inset, outer),   PointF(inset, outer - len) },
    };
    for (const auto& c : corners) g.DrawLines(&pen, c, 3);

    // 中央四芒星
    GraphicsPath star;
    const REAL cx = bg.X + bg.Width / 2, cy = bg.Y + bg.Height / 2;
    AddSparkle(star, cx, cy, bg.Width * (tiny ? 0.21f : 0.18f), tiny ? 0.38f : 0.28f);
    SolidBrush white(Color(255, 255, 255, 255));
    g.FillPath(&white, &star);
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

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        fwprintf(stderr, L"用法：make_icon.exe <輸出 .ico> [預覽 .png]\n");
        return 1;
    }
    GdiplusStartupInput gsi;
    ULONG_PTR tok;
    GdiplusStartup(&tok, &gsi, NULL);
    bool ok = WriteIco(argv[1]);
    if (ok && argc >= 3) WritePreview(argv[2]);
    GdiplusShutdown(tok);
    return ok ? 0 : 1;
}
