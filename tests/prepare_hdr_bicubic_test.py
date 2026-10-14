"""Build a small harness around the actual Renderer::_AppendBicubic body.

The compiler/drawer collaborators record their contracts; HDR route selection
and boundary preparation use production classes. This checks session/cache and
upstream-size handling without requiring the UI or a GPU SDK.
"""
from pathlib import Path
import sys

repo = Path(__file__).resolve().parents[1]
output = Path(sys.argv[1]).resolve()
output.mkdir(parents=True, exist_ok=True)
renderer = (repo / "src/Magpie.Core/Renderer.cpp").read_text(encoding="utf-8-sig")

def function(signature):
    begin = renderer.index(signature)
    brace = renderer.index("{", begin)
    depth = 1
    end = brace + 1
    while depth:
        depth += (renderer[end] == "{") - (renderer[end] == "}")
        end += 1
    return renderer[begin:end]

prefix = r'''
#include "HdrEffectBoundary.h"
#include "EffectProtocolCatalogC.h"
#include "NvidiaVfxParameters.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <memory>
#include <optional>
#include <vector>
using namespace Magpie;
struct MockTexture : ID3D11Texture2D {
    D3D11_TEXTURE2D_DESC desc{};
    MockTexture(UINT size, DXGI_FORMAT format) { desc.Width=desc.Height=size; desc.Format=format; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    void STDMETHODCALLTYPE GetDevice(ID3D11Device**) override {}
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT*, void*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown*) override { return E_NOTIMPL; }
    void STDMETHODCALLTYPE GetType(D3D11_RESOURCE_DIMENSION*) override {}
    void STDMETHODCALLTYPE SetEvictionPriority(UINT) override {}
    UINT STDMETHODCALLTYPE GetEvictionPriority() override { return 0; }
    void STDMETHODCALLTYPE GetDesc(D3D11_TEXTURE2D_DESC* result) override { *result=desc; }
};
enum class ScalingType { Fill, Fit, Absolute };
struct EffectOption { std::string name; std::map<std::string,float> parameters; ScalingType scalingType{}; std::pair<float,float> scale{}; };
struct ScalingOptions {
    bool hdr=false;
    bool IsHdrCompatibilityEnabled() const { return hdr; }
    bool IsWindowedMode() const { return true; }
};
struct ScalingWindow {
    ScalingOptions options;
    static ScalingWindow& Get() { static ScalingWindow w; return w; }
    ScalingOptions& Options() { return options; }
    RECT RendererRect() const { return {0,0,1920,1080}; }
};
namespace Win32Helper {
static SIZE GetSizeOfRect(const RECT& rect) { return {rect.right-rect.left, rect.bottom-rect.top}; }
}
struct Logger { static Logger& Get() { static Logger l; return l; } void Error(const char*) {} };
struct EffectDesc { std::string name; DXGI_FORMAT input{}, output{}; bool hdr=false; };
static int compiles[2]{};
static bool failed=false;
static std::optional<EffectDesc> CompileEffect(const EffectOption& option, bool, bool,
    DXGI_FORMAT in=DXGI_FORMAT_UNKNOWN, DXGI_FORMAT out=DXGI_FORMAT_UNKNOWN, bool enabled=false) {
    bool hdr=enabled;
    ++compiles[hdr ? 1 : 0];
    return EffectDesc{option.name, in==DXGI_FORMAT_UNKNOWN ? DXGI_FORMAT_R8G8B8A8_UNORM : in,
        out==DXGI_FORMAT_UNKNOWN ? DXGI_FORMAT_R8G8B8A8_UNORM : out, hdr};
}
static HdrFormatRoutes GetHdrRoutesForEffect(const EffectOption& option, bool hdr) {
    if (option.name!="Bicubic") failed=true;
    return hdr ? EffectProtocolC::Bicubic() : HdrFormatRoutes{};
}
struct EffectDrawer {
    HdrEffectBoundaryContext boundary;
    std::shared_ptr<MockTexture> output;
    void SetHdrBoundary(HdrEffectBoundaryContext c) { boundary=std::move(c); }
    bool Initialize(const EffectDesc& desc, const EffectOption&, int&, int&, ID3D11Texture2D** texture) {
        bool hdr=ScalingWindow::Get().Options().hdr;
        auto format=hdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
        if (desc.hdr!=hdr || desc.input!=format || desc.output!=format) failed=true;
        D3D11_TEXTURE2D_DESC upstream{}; (*texture)->GetDesc(&upstream);
        if (hdr && (!boundary.prepared || !boundary.SelectedRoute() ||
            boundary.SelectedRoute()->inputFormat!=format ||
            boundary.SelectedRoute()->outputFormat!=format ||
            boundary.plan.requiresSdrMapping || boundary.plan.requiresBoundedMapping ||
            boundary.inputFrame.metadata.width!=upstream.Width)) failed=true;
        output=std::make_shared<MockTexture>(upstream.Width,format); *texture=output.get();
        return !failed;
    }
};
struct FrameSource {
    MockTexture capture{16, DXGI_FORMAT_R16G16B16A16_FLOAT};
    HdrFrame GetCanonicalFrame() {
        HdrFrame f; f.texture=&capture;
        f.metadata.width=f.metadata.height=16; f.metadata.valid=true;
        f.metadata.stage=HdrFrameStage::CanonicalInput;
        f.metadata.color.primaries=HdrColorPrimaries::Rec709;
        f.metadata.color.transfer=HdrTransferFunction::Linear;
        return f;
    }
};
struct Renderer {
    FrameSource* _frameSource;
    std::vector<EffectOption> _runtimeEffectOptions;
    HdrFrameMetadata _pipelineOutputMetadata{};
    int _backendResources=0, _backendDescriptorStore=0;
    std::vector<EffectDrawer> _effectDrawers;
    bool _AppendBicubic(ID3D11Texture2D**) noexcept;
};
'''
suffix = r'''
int main() {
    FrameSource source;
    for (bool hdr : {false,true,false,true}) {
        ScalingWindow::Get().Options().hdr=hdr;
        for (UINT size : {8u,20u}) {
            Renderer renderer{&source};
            MockTexture upstream{size, hdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM};
            ID3D11Texture2D* texture=&upstream;
            if (!renderer._AppendBicubic(&texture)) failed=true;
        }
    }
    if (compiles[0]!=1 || compiles[1]!=1) failed=true;
    if (&GetBicubicDesc(false)==&GetBicubicDesc(true)) failed=true;
    std::printf("%s: actual _AppendBicubic SDR/HDR/SDR/HDR cache, formats, boundary and upstream sizes\n",
        failed ? "FAIL" : "PASS");
    return failed ? 1 : 0;
}
'''
(output / "hdr_bicubic.cpp").write_text(prefix + "\nstatic EffectDesc bicubicDescs[2];\n" +
    function("static HdrFrame MakePipelineInputFrame(") + "\n" +
    function("static EffectDesc& GetBicubicDesc(") + "\n" +
    function("bool Renderer::_AppendBicubic(") + "\n" + suffix, encoding="utf-8")
