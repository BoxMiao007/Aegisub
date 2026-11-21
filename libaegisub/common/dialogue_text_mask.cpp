// Copyright (c) 2026, BoxMiao007
// SPDX-License-Identifier: ISC

#include "libaegisub/dialogue_text_mask.h"
#include "libaegisub/unicode.h"

#include <cctype>

#include <unicode/uchar.h>
#include <unicode/utf8.h>

namespace agi::ass {
namespace {
bool Escaped(std::string_view text, size_t pos) {
	size_t slashes = 0;
	while (pos && text[pos - 1] == 92) { --pos; ++slashes; }
	return slashes % 2 != 0;
}

bool ValidUtf8(std::string_view text) {
	int32_t i = 0;
	while (i < static_cast<int32_t>(text.size())) {
		UChar32 cp;
		U8_NEXT(text.data(), i, static_cast<int32_t>(text.size()), cp);
		if (cp < 0) return false;
	}
	return true;
}

bool Readable(std::string_view grapheme) {
	bool base = false;
	int32_t i = 0;
	while (i < static_cast<int32_t>(grapheme.size())) {
		UChar32 c;
		U8_NEXT(grapheme.data(), i, static_cast<int32_t>(grapheme.size()), c);
		if (c < 0) return false;
		auto category = U_GET_GC_MASK(c);
		if ((category & U_GC_P_MASK) || u_isUWhiteSpace(c)) base = true;
		else if (!(category & (U_GC_M_MASK | U_GC_CF_MASK))) return false;
	}
	return base;
}

void AppendClusters(std::vector<MaskRange>& out, std::string_view text, size_t offset) {
	if (text.empty()) return;
	BreakIterator characters;
	characters.set_text(text);
	for (; !characters.done(); characters.next()) {
		auto cluster = characters.current();
		size_t p = cluster.data() - text.data();
		if (!Readable(cluster)) out.push_back({offset + p, cluster.size()});
	}
}

void AppendPlain(std::vector<MaskRange>& out, std::string_view text, size_t offset, bool ass) {
	if (!ass) { AppendClusters(out, text, offset); return; }
	size_t start = 0;
	for (size_t pos = 0; pos + 1 < text.size(); ++pos) {
		if (text[pos] == 92 && !Escaped(text, pos) &&
			(text[pos + 1] == 'N' || text[pos + 1] == 'n' || text[pos + 1] == 'h')) {
			AppendClusters(out, text.substr(start, pos - start), offset + start);
			pos += 1;
			start = pos + 1;
		}
	}
	AppendClusters(out, text.substr(start), offset + start);
}
}

std::vector<MaskRange> DialogueMaskRanges(std::string_view text, bool ass_syntax) {
	std::vector<MaskRange> ranges;
	if (!text.empty() && !ValidUtf8(text)) return {{0, text.size()}};
	size_t plain = 0;
	if (ass_syntax) {
		for (size_t pos = 0; pos < text.size(); ++pos) {
			if (text[pos] != '{' || Escaped(text, pos)) continue;
			auto close = text.find('}', pos + 1);
			if (close == text.npos) break;
			// ASS 标签不嵌套，首个右括号结束整个块；块内备注按用户选择保持可读。
			AppendPlain(ranges, text.substr(plain, pos - plain), plain, true);
			plain = close + 1;
			pos = close;
		}
	}
	AppendPlain(ranges, text.substr(plain), plain, ass_syntax);
	return ranges;
}

namespace {
void AppendReplacement(std::string& out, std::string_view text, std::string_view token) {
	if (text.empty()) return;
	BreakIterator characters;
	characters.set_text(text);
	for (; !characters.done(); characters.next()) {
		auto cluster = characters.current();
		if (Readable(cluster)) out.append(cluster);
		else out.append(token);
	}
}

int ParseDrawingLevel(std::string_view tag) {
	int level = -1;
	for (std::size_t i = 0; i + 1 < tag.size(); ++i) {
		if (tag[i] != 92 || tag[i + 1] != 'p') continue;
		std::size_t j = i + 2;
		int value = 0;
		bool digits = false;
		while (j < tag.size() && std::isdigit(static_cast<unsigned char>(tag[j]))) {
			value = value * 10 + (tag[j] - '0');
			digits = true;
			++j;
		}
		if (digits) level = value;
	}
	return level;
}
}

std::string NormalizeMaskToken(std::string_view token) {
	if (token.empty() || !ValidUtf8(token)) return "X";
	for (char c : token) {
		if (c == '{' || c == '}' || c == 92 || c == '\n' || c == '\r')
			return "X";
	}
	return std::string(token);
}

std::string DialogueMaskReplace(std::string_view text, std::string_view token, bool ass_syntax) {
	std::string masked = NormalizeMaskToken(token);
	std::string out;
	out.reserve(text.size());
	int drawing = 0;

	auto append_plain = [&](std::string_view segment) {
		std::size_t start = 0;
		for (std::size_t pos = 0; pos + 1 < segment.size(); ++pos) {
			if (segment[pos] == 92 && !Escaped(segment, pos) &&
			    (segment[pos + 1] == 'N' || segment[pos + 1] == 'n' || segment[pos + 1] == 'h')) {
				if (drawing > 0) out.append(segment.substr(start, pos - start));
				else AppendReplacement(out, segment.substr(start, pos - start), masked);
				out.push_back(92);
				out.push_back(segment[pos + 1]);
				pos += 1;
				start = pos + 1;
			}
		}
		if (drawing > 0) out.append(segment.substr(start));
		else AppendReplacement(out, segment.substr(start), masked);
	};

	if (!ass_syntax) {
		append_plain(text);
		return out;
	}

	std::size_t plain = 0;
	for (std::size_t pos = 0; pos < text.size(); ++pos) {
		if (text[pos] != '{' || Escaped(text, pos)) continue;
		auto close = text.find('}', pos + 1);
		if (close == text.npos) break;
		append_plain(text.substr(plain, pos - plain));
		out.append(text.substr(pos, close - pos + 1));
		int level = ParseDrawingLevel(text.substr(pos + 1, close - pos - 1));
		if (level >= 0) drawing = level;
		plain = close + 1;
		pos = close;
	}
	append_plain(text.substr(plain));
	return out;
}
}
