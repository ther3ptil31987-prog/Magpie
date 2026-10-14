#pragma once
#include <cstdint>
#include <string>
#include <utility>

namespace Magpie {

struct PresenterFailure {
	std::string context;
	uint32_t code = 0;

	// Retain the first cause even if draining or destroying the SDK also fails.
	void Record(std::string detail, uint32_t errorCode) noexcept {
		if (!context.empty()) return;
		context = std::move(detail);
		code = errorCode;
	}
};

}
