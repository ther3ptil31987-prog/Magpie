#pragma once
#include "ScalingModeEffectItem.g.h"
#include "ScalingTypeItem.g.h"
#include "EffectParametersViewModel.h"
#include "Event.h"

namespace Magpie {
struct EffectItem;
}

namespace winrt::Magpie::implementation {

struct ScalingTypeItem : ScalingTypeItemT<ScalingTypeItem> {
	ScalingTypeItem(hstring name, hstring desc) : _name(std::move(name)), _desc(std::move(desc)) {}

	hstring Name() const noexcept { return _name; }

	hstring Desc() const noexcept { return _desc; }

private:
	hstring _name;
	hstring _desc;
};

struct ScalingModeEffectItem : ScalingModeEffectItemT<ScalingModeEffectItem>,
                               wil::notify_property_changed_base<ScalingModeEffectItem> {
	ScalingModeEffectItem(uint32_t scalingModeIdx, uint32_t effectIdx);

	hstring Name() const noexcept {
		return _name;
	}

	uint32_t ScalingModeIdx() const noexcept {
		return _scalingModeIdx;
	}

	void ScalingModeIdx(uint32_t value) noexcept;

	uint32_t EffectIdx() const noexcept {
		return _effectIdx;
	}

	void EffectIdx(uint32_t value) noexcept;

	bool CanScale() const noexcept;

	bool HasParameters() const noexcept;
	hstring IssueDescription() const noexcept;

	IVector<IInspectable> ScalingTypes() noexcept;

	int ScalingType() const noexcept;
	void ScalingType(int value);

	bool IsShowScaleFactors() const noexcept;
	bool IsShowScalingPixels() const noexcept;

	double ScaleFactorX() const noexcept;
	void ScaleFactorX(double value);

	double ScaleFactorY() const noexcept;
	void ScaleFactorY(double value);

	double ScalingPixelsX() const noexcept;
	void ScalingPixelsX(double value);

	double ScalingPixelsY() const noexcept;
	void ScalingPixelsY(double value);

	winrt::Magpie::EffectParametersViewModel Parameters() const noexcept {
		return _parametersViewModel
			? winrt::Magpie::EffectParametersViewModel(*_parametersViewModel)
			: winrt::Magpie::EffectParametersViewModel{ nullptr };
	}

	void Remove();

	bool CanDrag() const noexcept;

	void RefreshDragState();

	::Magpie::Event<uint32_t> Removed;

private:
	bool _IsRemoved() const noexcept;
	bool _IsNvidiaVfxSameSize() const noexcept;
	void _SaveScaleChange();

	::Magpie::EffectItem& _Data() noexcept;
	const ::Magpie::EffectItem& _Data() const noexcept;

	uint32_t _scalingModeIdx = 0;
	uint32_t _effectIdx = 0;
	hstring _name;
	const ::Magpie::EffectInfo* _effectInfo = nullptr;

	bool _lastNvidiaVfxSameSize = false;
	bool _refreshingScalingTypes = false;
	com_ptr<EffectParametersViewModel> _parametersViewModel;
	::Magpie::Event<uint32_t, uint32_t>::EventRevoker _parameterChangedRevoker;
};

}
