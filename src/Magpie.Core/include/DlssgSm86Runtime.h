#pragma once
#include <dxgi.h>
#include <filesystem>

namespace Magpie::DlssgSm86Runtime {

// Prepare before any NGX consumer. Failure disables community FG only.
// Success means the redirected runtime/backend pair is verified, not just the
// proxy mapped. It does not establish NGX capability or kernel execution.
// Any preparation failure is sticky once loading starts; restart to retry.
bool Prepare(IDXGIAdapter* adapter) noexcept;
// Only publishes the actual loaded directory after successful preparation.
bool GetFeatureDirectory(std::filesystem::path& directory) noexcept;
// A byte identity check, NOT proof of kernel execution or image correctness.
bool VerifyLoadedPair() noexcept;

}
