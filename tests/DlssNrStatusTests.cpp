#include "DlssNrStatus.h"
#include <cassert>
#include <iostream>

using namespace Magpie;

int main() {
	// SDK result domains must never be guessed from an integer alone.
	assert(DlssNrApiFailure(DlssNrStage::CreateFeature, DlssNrCodeKind::Ngx, 0xBAD00001, "create").reason == DlssNrReason::Compatibility);
	assert(DlssNrApiFailure(DlssNrStage::InitializeLibrary, DlssNrCodeKind::Ngx, 0xBAD0000C, "init").reason == DlssNrReason::DriverOrLibraryOutOfDate);
	assert(DlssNrApiFailure(DlssNrStage::Evaluate, DlssNrCodeKind::Ngx, 0xBAD0000D, "evaluate").reason == DlssNrReason::OutOfMemory);
	assert(DlssNrApiFailure(DlssNrStage::LoadLibrary, DlssNrCodeKind::Win32, 126, "load").reason == DlssNrReason::Unknown);
	assert(DlssNrApiFailure(DlssNrStage::Initialize, DlssNrCodeKind::HResult, 0x80070005, "device").reason == DlssNrReason::AccessDenied);
	assert(DlssNrApiFailure(DlssNrStage::Initialize, DlssNrCodeKind::Ngx, 5, "init").reason == DlssNrReason::Unknown);
	assert(DlssNrApiFailure(DlssNrStage::Evaluate, DlssNrCodeKind::Seh, 0xC0000005, "evaluate").reason == DlssNrReason::RuntimeFault);

	DlssNrSessionState session;
	session.Begin(1, {true, false, true});
	assert(session.Read().effects[0].state == DlssNrState::Starting);
	assert(session.Read().effects[1].state == DlssNrState::Disabled);
	DlssNrStatus running{ .state = DlssNrState::Running };
	assert(session.Update(1, 0, running));
	const auto revision = session.Read().revision;
	assert(!session.Update(1, 0, running)); // no per-frame notifications
	assert(session.Read().revision == revision);
	DlssNrStatus failed{ .state = DlssNrState::Skipped,
		.failure = DlssNrApiFailure(DlssNrStage::Evaluate, DlssNrCodeKind::Ngx, 0xBAD00001, "evaluate") };
	assert(session.Update(1, 0, failed));
	assert(session.Read().effects[2].state == DlssNrState::Starting); // isolated instance
	assert(session.End(1));
	assert(!session.End(1));
	assert(session.Read().effects[0].state == DlssNrState::Stopped);
	assert(session.Read().effects[0].failure == failed.failure);
	assert(session.Read().effects[2].state == DlssNrState::Disabled);
	assert(!session.Update(1, 0, running)); // stopped renderer cannot publish recovery
	session.Begin(2, {true});
	assert(!session.Update(1, 0, failed));
	assert(!session.End(1)); // old destructor cannot close the replacement
	assert(!session.Update(2, 99, failed));
	assert(session.Read().effects[0].state == DlssNrState::Starting);
	assert(session.Update(2, 0, running));
	assert(session.Read().effects[0].failure.operation.empty());
	std::cout << "DLSSNR classification, state transitions, instance/generation isolation and deduplication passed.\n";
}
