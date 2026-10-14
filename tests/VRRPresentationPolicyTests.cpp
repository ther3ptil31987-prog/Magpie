#include "VRRPresentationPolicy.h"
#include <iostream>
#include <stdexcept>

using namespace Magpie;
static unsigned checks = 0;
static void Check(bool condition, const char* message) {
	++checks;
	if (!condition) throw std::runtime_error(message);
}
static VRRPresentationPolicy Create(bool requested = true, bool capable = true) {
	VRRPresentationPolicy policy;
	policy.Configure(requested, capable);
	policy.PathAvailable(true);
	policy.Environment(VRRUnavailableReason::None);
	policy.Created(policy.CreationFlags() | DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
	policy.AltEnterDisabled(true);
	return policy;
}
int main() {
	Check(!Create(false).PresentFlags(), "Default/off session allows tearing");
	Check(!Create(true, false).PresentFlags(), "Unsupported DXGI capability allows tearing");
	for (bool requested : {false, true}) for (bool capable : {false, true}) {
		const auto policy = Create(requested, capable);
		Check(bool(policy.PresentFlags()) == (requested && capable), "Explicit opt-in/capability contract broken");
		Check(bool(policy.ResizeFlags() & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) == (requested && capable),
			"Swap-chain capability doesn't match explicit opt-in");
	}
	for (const auto reason : {VRRUnavailableReason::OutputUnknown, VRRUnavailableReason::SpanningOutputs,
		VRRUnavailableReason::SoftwareAdapter, VRRUnavailableReason::RemoteSession,
		VRRUnavailableReason::CrossAdapter, VRRUnavailableReason::StaleDisplayTopology}) {
		auto policy = Create();
		const auto immutableFlags = policy.ResizeFlags();
		policy.Environment(reason);
		Check(!policy.PresentFlags() && policy.Reason() == reason, "Unsafe output remains eligible");
		Check(policy.ResizeFlags() == immutableFlags, "Output transition changed immutable creation flags");
		policy.Environment(VRRUnavailableReason::None);
		Check(policy.PresentFlags() == DXGI_PRESENT_ALLOW_TEARING, "Valid output did not resume tearing");
	}
	auto policy = Create();
	const auto immutableFlags = policy.ResizeFlags();
	policy.PathAvailable(false);
	Check(!policy.PresentFlags() && policy.Reason() == VRRUnavailableReason::PathUnavailable,
		"DComp path still advertises explicit DXGI VRR");
	Check(policy.ResizeFlags() == immutableFlags, "Temporary DComp path changed swap-chain flags");
	policy.PathAvailable(true);
	Check(policy.PresentFlags() == DXGI_PRESENT_ALLOW_TEARING, "Returning from DComp did not resume request");
	policy.AltEnterDisabled(false);
	Check(!policy.PresentFlags(), "Unguarded exclusive-fullscreen switch allows tearing");
	policy.AltEnterDisabled(true);
	policy.RejectPresent();
	Check(!policy.PresentFlags() && policy.Reason() == VRRUnavailableReason::PresentRejected,
		"Contract failure was not latched");
	policy.Environment(VRRUnavailableReason::None); policy.PathAvailable(false); policy.PathAvailable(true);
	Check(!policy.PresentFlags() && policy.ResizeFlags() == immutableFlags,
		"Resize/path transition cleared rejection or altered immutable flags");
	policy.Configure(true, true); policy.PathAvailable(true); policy.Environment(VRRUnavailableReason::None);
	Check(policy.CreationFlags() == DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING, "Fresh session didn't clear rejection");
	policy.RejectCreation();
	Check(!policy.CreationFlags(), "Rejected creation retried with the same tearing flag");
	policy.Created(DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT); policy.AltEnterDisabled(true);
	Check(!policy.PresentFlags() && policy.Reason() == VRRUnavailableReason::CreationRejected,
		"Creation fallback falsely allows tearing");
	policy = Create();
	policy.Created(DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
	Check(!policy.PresentFlags() && policy.Reason() == VRRUnavailableReason::SwapChainWithoutTearing,
		"Proxy without tearing capability falsely allows tearing");
	std::cout << "VRR presentation policy: " << checks << " checks passed (not display activation proof)\n";
}
