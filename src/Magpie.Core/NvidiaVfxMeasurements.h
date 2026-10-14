#pragma once
#include "NativeBackendTiming.h"
#include <array>
#include <cstdint>
#include <d3d11.h>
#include <utility>
#include <winrt/base.h>

namespace Magpie {
struct NvidiaVfxStageMeasurements {
	uint64_t cpuCalls = 0, gpuSamples = 0, gpuSkipped = 0;
	double cpuMs = 0, gpuSpanMs = 0;
};
struct NvidiaVfxRuntimeMeasurements {
	NvidiaVfxStageMeasurements importImage, inference, exportImage;
	uint64_t synchronizeCalls = 0;
	double synchronizeCpuMs = 0;
};
struct NvidiaVfxGpuMeasurements {
	uint64_t samples = 0, skipped = 0;
	double spanMs = 0;
};
struct NvidiaVfxFrameMeasurements {
	NvidiaVfxRuntimeMeasurements runtime;
	NvidiaVfxGpuMeasurements comparisonGpu;
	uint64_t comparisons = 0, srvCreations = 0, generatedOutputs = 0, realOutputs = 0, bypassedPairs = 0;
	double comparisonCpuMs = 0, readbackWaitCpuMs = 0;
};

// Diagnostic builds only. Queries are polled without a flush or CPU wait;
// saturated pools drop timing samples rather than delaying content. Spans may
// include GPU scheduling/idle time and must not be labelled isolated kernel time.
class NvidiaVfxD3dTimings {
public:
	int Begin(ID3D11Device* device, ID3D11DeviceContext* context) noexcept {
		if constexpr (!NativeBackendTiming::Enabled) return -1;
		_Poll(context);
		for (size_t i = 0; i < _slots.size(); ++i) {
			auto& slot = _slots[i];
			if (slot.pending || slot.active) continue;
			if (!slot.disjoint) {
				D3D11_QUERY_DESC desc{ D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };
				if (FAILED(device->CreateQuery(&desc, slot.disjoint.put()))) break;
				desc.Query = D3D11_QUERY_TIMESTAMP;
				if (FAILED(device->CreateQuery(&desc, slot.start.put())) ||
					FAILED(device->CreateQuery(&desc, slot.end.put()))) {
					slot = {}; break;
				}
			}
			context->Begin(slot.disjoint.get()); context->End(slot.start.get());
			slot.active = true; return int(i);
		}
		++_measurements.skipped; return -1;
	}
	void End(ID3D11DeviceContext* context, int ticket) noexcept {
		if constexpr (!NativeBackendTiming::Enabled) return;
		if (ticket < 0) return;
		auto& slot = _slots[size_t(ticket)];
		context->End(slot.end.get()); context->End(slot.disjoint.get());
		slot.active = false; slot.pending = true;
	}
	NvidiaVfxGpuMeasurements Take(ID3D11DeviceContext* context) noexcept {
		if constexpr (!NativeBackendTiming::Enabled) return {};
		_Poll(context); return std::exchange(_measurements, {});
	}
private:
	struct Slot {
		winrt::com_ptr<ID3D11Query> disjoint, start, end;
		bool active = false, pending = false;
	};
	void _Poll(ID3D11DeviceContext* context) noexcept {
		for (auto& slot : _slots) {
			if (!slot.pending) continue;
			D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
			UINT64 start = 0, end = 0;
			const auto a = context->GetData(slot.disjoint.get(), &disjoint, sizeof(disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH);
			const auto b = context->GetData(slot.start.get(), &start, sizeof(start), D3D11_ASYNC_GETDATA_DONOTFLUSH);
			const auto c = context->GetData(slot.end.get(), &end, sizeof(end), D3D11_ASYNC_GETDATA_DONOTFLUSH);
			if (a == S_FALSE || b == S_FALSE || c == S_FALSE) continue;
			slot.pending = false;
			if (FAILED(a) || FAILED(b) || FAILED(c) || disjoint.Disjoint || !disjoint.Frequency || end < start) {
				++_measurements.skipped; continue;
			}
			++_measurements.samples;
			_measurements.spanMs += double(end - start) * 1000. / double(disjoint.Frequency);
		}
	}
	std::array<Slot, 16> _slots;
	NvidiaVfxGpuMeasurements _measurements;
};
}
