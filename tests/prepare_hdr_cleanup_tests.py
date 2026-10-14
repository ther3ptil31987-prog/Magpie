"""Exercise the actual cleanup implementation against deterministic SDK failures."""
from pathlib import Path
import sys

repo = Path(__file__).resolve().parents[1]
out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
source = (repo / 'src/RtxVideoBridge/RtxVideoBridge.cpp').read_text(encoding='utf-8-sig')
cleanup = source[source.index('HRESULT DestroyImpl('):source.index('\nHRESULT CreateImpl(')]
exports = source[source.index('extern "C" __declspec(dllexport) HRESULT WINAPI MagpieRtxHdrDestroy('):]
stub = r'''
#include "../src/RtxVideoBridge/RtxVideoBridge.h"
#include <cassert>
#include <iostream>
using NVSDK_NGX_Result = int;
#define NVSDK_NGX_SUCCEED(value) ((value) == 1)
int failure = 0, calls = 0;
bool raiseOnRelease = false;
struct Device { int released = 0; void Release(){ ++released; } };
struct Context { int released = 0; void Release(){ ++released; } };
struct Parameters {};
struct Feature {};
Device* activeDevice = nullptr;
unsigned users = 0;
bool faulted = false;
struct Instance { Context* context; Parameters* parameters; Feature* feature; };
int NVSDK_NGX_D3D11_ReleaseFeature(Feature*) { ++calls; if (raiseOnRelease) RaiseException(0xe0123456, 0, 0, nullptr); return failure == 1 ? -1 : 1; }
int NVSDK_NGX_D3D11_DestroyParameters(Parameters*) { ++calls; return failure == 2 ? -2 : 1; }
int NVSDK_NGX_D3D11_Shutdown1(Device*) { ++calls; return failure == 3 ? -3 : 1; }
'''.replace('../src/RtxVideoBridge/RtxVideoBridge.h', (repo / 'src/RtxVideoBridge/RtxVideoBridge.h').as_posix())
main = r'''
int main() {
    Parameters parameters; Feature feature;
    for (failure = 0; failure <= 3; ++failure) {
        Device device; Context context; calls = 0; users = 1; faulted = false; activeDevice = &device;
        auto* state = new Instance{ &context, &parameters, &feature };
        RtxHdrCleanupStage stage{}; uint32_t status = 0;
        const auto hr = DestroyImpl(state, &stage, &status);
        if (!failure) {
            assert(hr == S_OK && stage == RtxHdrCleanupStage::None && status == 0);
            assert(calls == 3 && !faulted && !users && !activeDevice && context.released == 1 && device.released == 1);
        } else {
            assert(hr == E_UNEXPECTED && int(stage) == failure && status == uint32_t(-failure));
            assert(calls == failure && faulted && !users && activeDevice == &device);
            assert(!context.released && !device.released);
            auto* other = new Instance{ &context, &parameters, &feature }; users = 1;
            assert(DestroyImpl(other, &stage, &status) == E_UNEXPECTED);
            assert(stage == RtxHdrCleanupStage::Faulted && calls == failure && !users);
            delete other; delete state; // test-owned mocks, never SDK resources
        }
    }
    Device device; Context context; failure = calls = 0; users = 2; faulted = false; activeDevice = &device;
    assert(DestroyImpl(new Instance{ &context, &parameters, &feature }) == S_OK);
    assert(users == 1 && calls == 2 && activeDevice == &device);
    assert(DestroyImpl(new Instance{ &context, &parameters, &feature }) == S_OK);
    assert(!users && !activeDevice && calls == 5 && context.released == 2 && device.released == 1);
    assert(DestroyImpl(nullptr) == S_OK);
    Context exceptionContext; auto* exceptionState = new Instance{ &exceptionContext, &parameters, &feature };
    users = 1; activeDevice = &device; faulted = false; calls = 0; raiseOnRelease = true;
    RtxHdrCleanupStage stage{}; uint32_t status = 0;
    assert(MagpieRtxHdrDestroyEx(exceptionState, &stage, &status) == E_UNEXPECTED);
    assert(stage == RtxHdrCleanupStage::Exception && status == 0xe0123456 && faulted);
    assert(calls == 1 && !exceptionContext.released && activeDevice == &device);
    delete exceptionState;
    std::cout << "PASS: production RTX cleanup success, each failure stage, dependent-call suppression, retained resources, fault retirement and shared users\n";
    std::cout << "PASS: production RTX extended destroy export catches SEH and reports exception stage/code without further SDK calls\n";
}
'''
(out / 'HdrCleanupTests.cpp').write_text(stub + cleanup + exports + main, encoding='utf-8')
