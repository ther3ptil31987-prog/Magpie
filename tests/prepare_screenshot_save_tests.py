"""Extract production directory, WIC encoder and no-overwrite commit functions."""
from pathlib import Path
import sys
repo = Path(__file__).resolve().parents[1]
out = Path(sys.argv[1])
texture = (repo/'src/Magpie.Core/TextureHelper.cpp').read_text(encoding='utf-8-sig')
texture = texture[texture.index('static void RecordTextureSaveError('):texture.rfind('\n}')]
helper = (repo/'src/Magpie.Core/ScreenshotHelper.cpp').read_text(encoding='utf-8-sig')
helper = helper[helper.index('std::mutex& ScreenshotHelper::SaveMutex('):helper.rfind('\n}')]
preamble = r'''
#include "pch.h"
#include "TextureHelper.h"
#include "TextureExportPixels.h"
#include "ScreenshotHelper.h"
#include "ScreenshotNaming.h"
#include "StrHelper.h"
#include <wincodec.h>
#include <fstream>
#include <thread>
#include <cassert>
#include <iostream>
namespace Magpie {
struct Logger {
    static Logger& Get(){ static Logger instance; return instance; }
    void ComError(const char*, HRESULT){}
    void Win32Error(const char*){}
    void Error(const char*){}
};
// This fixture exercises PNG commits; the existing HDR fixture tests DDS routing.
struct DDSHelper {
    static bool Save(const wchar_t*, uint32_t, uint32_t, DXGI_FORMAT,
        std::span<uint8_t>, uint32_t, TextureSaveError*) { return false; }
};
'''
main = (repo/'tests/ScreenshotSaveTests.cpp').read_text(encoding='utf-8-sig')
(out/'ScreenshotSaveTests.generated.cpp').write_text(preamble+texture+helper+'\n}\n'+main, encoding='utf-8')
