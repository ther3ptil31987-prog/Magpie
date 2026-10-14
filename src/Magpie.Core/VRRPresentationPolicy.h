#pragma once
#include <cstdint>
#include <dxgi1_6.h>

namespace Magpie {

// Eligibility means Magpie can request windowed DXGI tearing. It never means
// that a monitor supports VRR or that a vendor driver has activated G-SYNC.
enum class VRRUnavailableReason : uint32_t {
	None, Disabled, TearingUnsupported, PathUnavailable, OutputUnknown,
	SpanningOutputs, SoftwareAdapter, RemoteSession, CrossAdapter,
	CreationRejected, AltEnterUnsafe, PresentRejected, SwapChainWithoutTearing, StaleDisplayTopology
};

class VRRPresentationPolicy {
public:
	void Configure(bool requested, bool tearingSupported) noexcept {
		*this = {};
		_requested = requested;
		_tearingSupported = tearingSupported;
	}
	void Environment(VRRUnavailableReason reason) noexcept { _environment = reason; }
	void PathAvailable(bool available) noexcept { _pathAvailable = available; }
	void RejectCreation() noexcept { _creationRejected = true; }
	void RejectPresent() noexcept { _presentRejected = true; }
	void AltEnterDisabled(bool disabled) noexcept { _altEnterDisabled = disabled; }
	void Created(uint32_t flags) noexcept { _creationFlags = flags; _created = true; }
	bool Requested() const noexcept { return _requested; }
	bool TearingSupported() const noexcept { return _tearingSupported; }
	bool HasSwapChain() const noexcept { return _created; }
	uint32_t CreationFlags() const noexcept {
		return _CanRequest() && !_creationRejected ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
	}
	// ALLOW_TEARING is immutable for the lifetime of an existing swap chain.
	// Environment changes and a failed Present disable submission flags only.
	uint32_t ResizeFlags() const noexcept { return _creationFlags; }
	uint32_t PresentFlags() const noexcept {
		return Reason() == VRRUnavailableReason::None &&
			(_creationFlags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) ? DXGI_PRESENT_ALLOW_TEARING : 0;
	}
	VRRUnavailableReason Reason() const noexcept {
		if (!_requested) return VRRUnavailableReason::Disabled;
		if (!_tearingSupported) return VRRUnavailableReason::TearingUnsupported;
		if (!_pathAvailable) return VRRUnavailableReason::PathUnavailable;
		if (_creationRejected) return VRRUnavailableReason::CreationRejected;
		if (_presentRejected) return VRRUnavailableReason::PresentRejected;
		if (!_altEnterDisabled) return VRRUnavailableReason::AltEnterUnsafe;
		if (_created && !(_creationFlags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING))
			return VRRUnavailableReason::SwapChainWithoutTearing;
		return _environment;
	}
	bool operator==(const VRRPresentationPolicy&) const = default;

private:
	bool _CanRequest() const noexcept {
		return _requested && _tearingSupported && _pathAvailable &&
			_environment == VRRUnavailableReason::None;
	}
	bool _requested = false, _tearingSupported = false, _pathAvailable = false;
	bool _creationRejected = false, _presentRejected = false, _altEnterDisabled = false;
	bool _created = false;
	uint32_t _creationFlags = 0;
	VRRUnavailableReason _environment = VRRUnavailableReason::OutputUnknown;
};

}
