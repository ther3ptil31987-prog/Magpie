#include <cassert>
#include <filesystem>
#include <iostream>
#include <memory>
#include "DlssNrStatus.h"
#include <string_view>

namespace Magpie {
struct Logger {
	static Logger& Get() { static Logger value; return value; }
	void Error(std::string_view) {}
};
struct DeviceRef {
	void* value = nullptr;
	void* get() const { return value; }
};
struct DLSSNRFilter {
	struct Impl {
		DlssNrFailure failure;
		std::string libraryInfo;
		inline static int initCount = 0, shutdownCount = 0;
		DeviceRef device12;
		std::shared_ptr<Impl> snippetSession;
		int snippetCreateFeature = 0, snippetEvaluateFeature = 0, snippetReleaseFeature = 0;
		bool useSignedSnippet = false, runtimeOwner = false;
		~Impl() { if (runtimeOwner) ++shutdownCount; }
	};
};
inline bool InitializeSignedSnippetSession(DLSSNRFilter::Impl& impl, const std::filesystem::path&) {
	++DLSSNRFilter::Impl::initCount;
	impl.runtimeOwner = true;
	impl.snippetCreateFeature = 1;
	impl.snippetEvaluateFeature = 2;
	impl.snippetReleaseFeature = 3;
	return true;
}
#include "DLSSNRSessionUnderTest.h"
}

int main() {
	using namespace Magpie;
	using Impl = DLSSNRFilter::Impl;
	int device, otherDevice;
	{
		auto first = std::make_unique<Impl>(); first->device12.value = &device;
		auto second = std::make_unique<Impl>(); second->device12.value = &device;
		auto third = std::make_unique<Impl>(); third->device12.value = &device;
		assert(InitializeSignedSnippet(*first, {}));
		assert(InitializeSignedSnippet(*second, {}));
		assert(InitializeSignedSnippet(*third, {}));
		assert(Impl::initCount == 1);
		assert(first->snippetSession == second->snippetSession && second->snippetSession == third->snippetSession);
		assert(third->snippetCreateFeature == 1 && third->snippetEvaluateFeature == 2 && third->snippetReleaseFeature == 3);
		Impl incompatible; incompatible.device12.value = &otherDevice;
		assert(!InitializeSignedSnippet(incompatible, {}));
		first.reset(); second.reset();
		assert(Impl::shutdownCount == 0 && third->snippetSession && third->useSignedSnippet);
		third.reset();
		assert(Impl::shutdownCount == 1);
	}
	{
		Impl restarted; restarted.device12.value = &otherDevice;
		assert(InitializeSignedSnippet(restarted, {}));
		assert(Impl::initCount == 2);
	}
	assert(Impl::shutdownCount == 2);
	std::cout << "DLSSNR runtime session: one initialization, shared exports, last-owner shutdown, device isolation and restart passed.\n";
}
