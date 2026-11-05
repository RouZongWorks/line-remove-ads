// Generates assets/icon.ico (multi-size, PNG-compressed) and assets/icon.png.
// Usage: make_icon.exe <output-directory>
#include "icon_draw.h"
#include <objidl.h>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")

static bool PngClsid(CLSID* out) {
    UINT count = 0, bytes = 0;
    Gdiplus::GetImageEncodersSize(&count, &bytes);
    if (!bytes) return false;
    std::vector<char> buf(bytes);
    auto* enc = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.data());
    Gdiplus::GetImageEncoders(count, bytes, enc);
    for (UINT i = 0; i < count; ++i) {
        if (wcscmp(enc[i].MimeType, L"image/png") == 0) { *out = enc[i].Clsid; return true; }
    }
    return false;
}

static std::vector<uint8_t> EncodePng(Gdiplus::Bitmap& bmp, const CLSID& png) {
    std::vector<uint8_t> data;
    IStream* stream = nullptr;
    if (CreateStreamOnHGlobal(nullptr, TRUE, &stream) != S_OK) return data;
    if (bmp.Save(stream, &png, nullptr) == Gdiplus::Ok) {
        HGLOBAL mem = nullptr;
        GetHGlobalFromStream(stream, &mem);
        SIZE_T size = GlobalSize(mem);
        STATSTG st;
        if (stream->Stat(&st, STATFLAG_NONAME) == S_OK) size = (SIZE_T)st.cbSize.QuadPart;
        const uint8_t* p = static_cast<const uint8_t*>(GlobalLock(mem));
        data.assign(p, p + size);
        GlobalUnlock(mem);
    }
    stream->Release();
    return data;
}

#pragma pack(push, 1)
struct IconDir   { uint16_t reserved, type, count; };
struct IconEntry { uint8_t w, h, colors, reserved; uint16_t planes, bpp; uint32_t bytes, offset; };
#pragma pack(pop)

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) { fprintf(stderr, "usage: make_icon <output-directory>\n"); return 2; }
    std::wstring dir = argv[1];
    if (!dir.empty() && dir.back() != L'\\' && dir.back() != L'/') dir += L'\\';

    Gdiplus::GdiplusStartupInput in;
    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartup(&token, &in, nullptr);
    int rc = 1;
    {
        CLSID png;
        if (!PngClsid(&png)) { fprintf(stderr, "PNG encoder not found\n"); return 1; }

        // Draw once at 512 px and downscale, so every size is anti-aliased the same way.
        Gdiplus::Bitmap master(512, 512, PixelFormat32bppARGB);
        DrawAppIcon(master);

        const int sizes[] = { 16, 20, 24, 32, 40, 48, 64, 128, 256 };
        std::vector<std::vector<uint8_t>> images;
        for (int s : sizes) {
            Gdiplus::Bitmap bmp(s, s, PixelFormat32bppARGB);
            {
                Gdiplus::Graphics g(&bmp);
                g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
                g.Clear(Gdiplus::Color(0, 0, 0, 0));
                g.DrawImage(&master, Gdiplus::Rect(0, 0, s, s), 0, 0, 512, 512, Gdiplus::UnitPixel);
            }
            images.push_back(EncodePng(bmp, png));
            if (images.back().empty()) { fprintf(stderr, "encode failed for %d\n", s); return 1; }
        }

        FILE* f = _wfopen((dir + L"icon.ico").c_str(), L"wb");
        if (!f) { fprintf(stderr, "cannot write icon.ico\n"); return 1; }
        IconDir hdr = { 0, 1, (uint16_t)images.size() };
        fwrite(&hdr, sizeof(hdr), 1, f);
        uint32_t offset = (uint32_t)(sizeof(IconDir) + sizeof(IconEntry) * images.size());
        for (size_t i = 0; i < images.size(); ++i) {
            IconEntry e = { (uint8_t)(sizes[i] >= 256 ? 0 : sizes[i]), (uint8_t)(sizes[i] >= 256 ? 0 : sizes[i]),
                            0, 0, 1, 32, (uint32_t)images[i].size(), offset };
            fwrite(&e, sizeof(e), 1, f);
            offset += (uint32_t)images[i].size();
        }
        for (auto& img : images) fwrite(img.data(), 1, img.size(), f);
        fclose(f);

        Gdiplus::Bitmap preview(256, 256, PixelFormat32bppARGB);
        DrawAppIcon(preview);
        preview.Save((dir + L"icon.png").c_str(), &png, nullptr);
        printf("wrote icon.ico (%zu sizes) and icon.png\n", images.size());
        rc = 0;
    }
    Gdiplus::GdiplusShutdown(token);
    return rc;
}
