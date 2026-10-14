#pragma once
#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace Magpie {

// Patch only rate values. Keep all other bytes, including BOM, case, spacing,
// comments, section order and line endings. UTF-16 is deliberately unsupported.
inline bool ClearRtssRateExpression(const std::string& input, std::string& output) {
	if (input.find('\0') != std::string::npos) return false;
	const bool bom = input.starts_with("\xEF\xBB\xBF");
	if (input.starts_with("\xFF\xFE") || input.starts_with("\xFE\xFF")) return false;
	auto trim = [](std::string_view s) {
		const auto start = s.find_first_not_of(" \t\r\n");
		return start == s.npos ? std::string_view{} : s.substr(start, s.find_last_not_of(" \t\r\n") - start + 1);
	};
	auto lower = [](std::string_view s) {
		std::string result(s);
		for (char& c : result) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
		return result;
	};
	struct Edit { size_t start, length; std::string text; };
	std::vector<Edit> edits;
	std::array<bool, 2> found{};
	bool inRate = false, seenRate = false;
	size_t insertion = input.size();
	for (size_t pos = bom ? 3 : 0; pos < input.size();) {
		const auto newline = input.find('\n', pos);
		const size_t end = newline == input.npos ? input.size() : newline + 1;
		const auto line = std::string_view(input).substr(pos, end - pos);
		const auto text = trim(line);
		if (!text.empty() && text.front() == '[' && text.back() == ']') {
			if (inRate) insertion = pos;
			inRate = lower(trim(text.substr(1, text.size() - 2))) == "framerate";
			if (inRate) { if (seenRate) return false; seenRate = true; }
		} else if (inRate && !text.empty() && text.front() != ';' && text.front() != '#') {
			const auto equal = text.find('=');
			if (equal != text.npos) {
				const auto key = lower(trim(text.substr(0, equal)));
				const int index = key == "limit" ? 0 : key == "limittime" ? 1 : -1;
				if (index >= 0) {
					if (found[index]) return false;
					found[index] = true;
					const auto value = trim(text.substr(equal + 1));
					if (value.empty()) return false;
					edits.push_back({ size_t(value.data() - input.data()), value.size(), "0" });
				}
			}
		}
		pos = end;
	}
	const std::string eol = input.find("\r\n") != input.npos ? "\r\n" : "\n";
	std::string addition;
	if (!seenRate) addition = "[Framerate]" + eol;
	if (!found[0]) addition += "Limit=0" + eol;
	if (!found[1]) addition += "LimitTime=0" + eol;
	if (!addition.empty()) {
		if (insertion && input[insertion - 1] != '\n') addition.insert(0, eol);
		edits.push_back({ insertion, 0, std::move(addition) });
	}
	std::sort(edits.begin(), edits.end(), [](const Edit& a, const Edit& b) { return a.start > b.start; });
	output = input;
	for (const auto& edit : edits) output.replace(edit.start, edit.length, edit.text);
	return true;
}

}
