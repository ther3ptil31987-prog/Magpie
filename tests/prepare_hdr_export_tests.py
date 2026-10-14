"""Use the production PNG encoder and save dispatcher in an isolated WIC test."""
from pathlib import Path
import sys

repo = Path(__file__).resolve().parents[1]
out = Path(sys.argv[1])
source = (repo / 'src/Magpie.Core/TextureHelper.cpp').read_text(encoding='utf-8-sig')
functions = source[source.index('static void RecordTextureSaveError('):source.rfind('\n}')]
# Exclusive screenshot commits are exercised by the screenshot disk fixture;
# this existing test isolates the ordinary encoder/format dispatcher.
exclusive_start = functions.index('bool TextureHelper::SaveTextureExclusive(')
exclusive_end = functions.index('bool TextureHelper::SaveTexture(', exclusive_start)
functions = functions[:exclusive_start] + functions[exclusive_end:]
preamble = r'''
#include <windows.h>
#include <winrt/base.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <span>
#include <string>
#include <vector>
#include <ranges>
#include <wincodec.h>
#include "TextureHelper.h"
#include "TextureExportPixels.h"
namespace Magpie {
struct Logger {
    static Logger& Get(){ static Logger instance; return instance; }
    void ComError(const char*, HRESULT){}
    void Error(const char*){}
};
DXGI_FORMAT savedDdsFormat = DXGI_FORMAT_UNKNOWN;
struct DDSHelper {
    static bool Save(const wchar_t*, uint32_t, uint32_t, DXGI_FORMAT format,
        std::span<uint8_t>, uint32_t, TextureSaveError*) { savedDdsFormat = format; return true; }
};
'''
main = r'''
int wmain(int argc, wchar_t** argv) {
    assert(argc == 2);
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    using namespace Magpie;
    const std::wstring filename = std::wstring(argv[1]) + L"/HdrEncoded.png";
    std::array<uint16_t, 4> half{0x3c00, 0x3c00, 0x3c00, 0x3800};
    const auto bytes = std::span(reinterpret_cast<uint8_t*>(half.data()), sizeof(half));
    ColorDescription color;
    color.primaries = HdrColorPrimaries::Rec709; color.transfer = HdrTransferFunction::Linear; color.sdrWhiteNits = 80;
    TextureSaveError error;
    assert(TextureHelper::SaveTexture(filename.c_str(), 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, bytes, 8, &error, &color));
    winrt::com_ptr<IWICImagingFactory2> factory;
    assert(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.put()))));
    winrt::com_ptr<IWICBitmapDecoder> decoder;
    assert(SUCCEEDED(factory->CreateDecoderFromFilename(filename.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, decoder.put())));
    winrt::com_ptr<IWICBitmapFrameDecode> frame;
    assert(SUCCEEDED(decoder->GetFrame(0, frame.put())));
    winrt::com_ptr<IWICFormatConverter> converted;
    assert(SUCCEEDED(factory->CreateFormatConverter(converted.put())));
    assert(SUCCEEDED(converted->Initialize(frame.get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom)));
    std::array<uint8_t, 4> decoded{};
    assert(SUCCEEDED(converted->CopyPixels(nullptr, 4, 4, decoded.data())));
    assert((decoded == std::array<uint8_t,4>{188,188,188,128}));
    assert(TextureHelper::SaveTexture(L"Intermediate.dds", 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, bytes, 8, &error, &color));
    assert(savedDdsFormat == DXGI_FORMAT_R16G16B16A16_FLOAT && half[0] == 0x3c00);
    std::cout << "PASS: production WIC PNG encode/decode preserves mapped SDR RGB and alpha; DDS dispatcher retains actual FP16 format\n";
}
'''
(out / 'HdrExportTests.cpp').write_text(preamble + functions + '\n}\n' + main, encoding='utf-8')
