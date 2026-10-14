// Generated copy helper uses fake D3D endpoints; no GPU or live application.
#define NOMINMAX
#include <windows.h>
#include "VRRPerformance.h"
#include "VRROutputProbe.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

using namespace Magpie;
struct D3D11_TEXTURE2D_DESC { UINT Width, Height; };
struct ID3D11Texture2D {
	D3D11_TEXTURE2D_DESC desc;
	void GetDesc(D3D11_TEXTURE2D_DESC* value) { *value = desc; }
};
struct ID3D11RenderTargetView {};
struct FakeContext {
	unsigned clears = 0, copies = 0, regions = 0;
	void ClearRenderTargetView(ID3D11RenderTargetView*, const FLOAT*) { ++clears; }
	void CopyResource(ID3D11Texture2D*, ID3D11Texture2D*) { ++copies; }
	void CopySubresourceRegion(ID3D11Texture2D*, UINT, UINT, UINT, UINT, ID3D11Texture2D*, UINT, const void*) { ++regions; }
};
struct FakeResources {
	FakeContext context;
	FakeContext* GetD3DDC() { return &context; }
};
struct ScalingWindow {
	static ScalingWindow& Get() { static ScalingWindow window; return window; }
	const RECT& RendererRect() const { static const RECT rect{0, 0, 1920, 1080}; return rect; }
};
struct Renderer {
	FakeResources _frontendResources;
	RECT _destRect{0, 0, 1920, 1080};
	void _CopySceneToTarget(ID3D11Texture2D*, ID3D11Texture2D*, ID3D11RenderTargetView*, POINT) noexcept;
};
/* PRODUCTION_SCENE_COPY */

int main() {
	using Event = VRRPerformance::Event;
	VRRPerformance::Start(false);
	VRRPerformance::Record(Event::FullSceneCopy);
	assert(VRRPerformance::Read()[size_t(Event::FullSceneCopy)] == 0);
	VRRPerformance::Start(true);
	Renderer renderer;
	ID3D11Texture2D scene{{1920, 1080}}, target{{1920, 1080}};
	ID3D11RenderTargetView rtv;
	renderer._CopySceneToTarget(&scene, &target, &rtv, {});
	assert(renderer._frontendResources.context.copies == 1 && !renderer._frontendResources.context.clears);
	scene.desc.Width = 1600;
	renderer._CopySceneToTarget(&scene, &target, &rtv, {});
	assert(renderer._frontendResources.context.regions == 1 && renderer._frontendResources.context.clears == 1);
	scene.desc = target.desc;
	renderer._CopySceneToTarget(&scene, &target, &rtv, {8, 0});
	assert(renderer._frontendResources.context.regions == 2 && renderer._frontendResources.context.clears == 2);
	const auto copies = VRRPerformance::Read();
	assert(copies[size_t(Event::FullSceneCopy)] == 1 && copies[size_t(Event::RegionSceneCopy)] == 2);
	assert(copies[size_t(Event::TargetClear)] == 2);
	VRRPerformance::AuxiliaryReasons(1 | 4);
	VRRPerformance::AuxiliaryReasons(0);
	const auto reasons = VRRPerformance::Read();
	assert(reasons[size_t(Event::AuxCursorDirty)] == 1 && reasons[size_t(Event::AuxInputDirty)] == 1);
	assert(reasons[size_t(Event::AuxUnclassified)] == 1 && reasons[size_t(Event::AuxPanelDirty)] == 0);
	std::vector<std::thread> threads;
	for (unsigned i = 0; i < 4; ++i) threads.emplace_back([] {
		for (unsigned j = 0; j < 20000; ++j) VRRPerformance::Record(Event::CaptureAccepted);
	});
	for (auto& thread : threads) thread.join();
	assert(VRRPerformance::Read()[size_t(Event::CaptureAccepted)] == 80000);
	{
		VRRPerformance::PresentScope scope(true);
	}
	const auto present = VRRPerformance::Read();
	assert(present[size_t(Event::PresentCall)] == 1 && present[size_t(Event::TearingPresentCall)] == 1);
	VRRPerformance::Stop(); VRRPerformance::Record(Event::CaptureAccepted);
	assert(VRRPerformance::Read()[size_t(Event::CaptureAccepted)] == 80000);
	VRRPerformance::Start(true);
	assert(VRRPerformance::Read() == VRRPerformance::Sample{});
	VRRPerformance::Stop();

	VRROutputProbe probe;
	VRROutputProbeKey key;
	key.monitor = 1; key.topologyCurrent = true; key.geometryValid = true;
	key.windowRect = {0, 0, 800, 600}; key.monitorRect = {0, 0, 1920, 1080};
	assert(probe.NeedsEnumeration(key, 1000, false));
	assert(!probe.NeedsEnumeration(key, 1500, false));
	key.windowRect[0] = 10;
	assert(!probe.NeedsEnumeration(key, 2000, false));
	assert(probe.NeedsEnumeration(key, 6000, false));
	key.monitor = 2;
	assert(probe.NeedsEnumeration(key, 6001, false));
	key.monitorRect[2] = 2560;
	assert(probe.NeedsEnumeration(key, 6002, false));
	key.topologyCurrent = false;
	assert(probe.NeedsEnumeration(key, 6003, false));
	probe.Invalidate();
	assert(probe.NeedsEnumeration(key, 6004, false));
	assert(probe.NeedsEnumeration(key, 6005, true));
	std::cout << "VRR performance: opt-in counters, full/partial scene copies, concurrent counting, output cache invalidation passed\n";
}
