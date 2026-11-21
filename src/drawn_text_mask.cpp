#include "drawn_text_mask.h"

#include "compat.h"

#include <libaegisub/unicode.h>

#include <algorithm>
#include <limits>
#include <string>
#include <vector>
#include <unicode/uchar.h>
#include <wx/dc.h>
#include <wx/string.h>

#ifdef __WXMSW__
#include <wx/msw/wrapwin.h>
#include <usp10.h>
#endif

namespace subtitle_mask {
namespace {
struct Position {
	size_t byte;
	int unit;
};

std::string Utf8(wxString const& text) {
	auto bytes = text.utf8_str();
	return bytes ? std::string(bytes.data(), bytes.length()) : std::string();
}

bool DecodePositions(std::string_view text, wxString const& wx_text, std::vector<Position>& out) {
	out.clear();
	out.push_back({0, 0});
	int unit = 0;
	for (size_t i = 0; i < text.size();) {
		unsigned char lead = static_cast<unsigned char>(text[i]);
		int count = lead < 0x80 ? 1 : lead >= 0xC2 && lead <= 0xDF ? 2 : lead >= 0xE0 && lead <= 0xEF ? 3 : lead >= 0xF0 && lead <= 0xF4 ? 4 : 0;
		if (!count || i + count > text.size()) return false;
		uint32_t cp = count == 1 ? lead : lead & ((1 << (7 - count)) - 1);
		for (int n = 1; n < count; ++n) {
			unsigned char next = static_cast<unsigned char>(text[i + n]);
			if ((next & 0xC0) != 0x80) return false;
			cp = (cp << 6) | (next & 0x3F);
		}
		if ((count == 2 && cp < 0x80) || (count == 3 && cp < 0x800) || (count == 4 && cp < 0x10000) || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
		unit += sizeof(wxChar) >= 4 ? 1 : cp > 0xFFFF ? 2 : 1;
		i += count;
		out.push_back({i, unit});
	}
	return unit == static_cast<int>(wx_text.length());
}

bool UnsupportedNonWindows(std::string_view text) {
	for (size_t i = 0; i < text.size();) {
		unsigned char lead = static_cast<unsigned char>(text[i]);
		int count = lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
		uint32_t cp = count == 1 ? lead : lead & ((1 << (7 - count)) - 1);
		for (int n = 1; n < count; ++n) cp = (cp << 6) | (static_cast<unsigned char>(text[i + n]) & 0x3F);
		auto category = u_charType(static_cast<UChar32>(cp));
		int8_t direction = u_charDirection(static_cast<UChar32>(cp));
		if (category == U_NON_SPACING_MARK || category == U_COMBINING_SPACING_MARK || category == U_ENCLOSING_MARK ||
			direction == U_RIGHT_TO_LEFT || direction == U_RIGHT_TO_LEFT_ARABIC || direction == U_RIGHT_TO_LEFT_EMBEDDING || direction == U_RIGHT_TO_LEFT_OVERRIDE || direction == U_POP_DIRECTIONAL_FORMAT ||
			category == U_FORMAT_CHAR) return true;
		i += count;
	}
	return false;
}

int PositionAt(std::vector<Position> const& positions, size_t byte) {
	auto it = std::lower_bound(positions.begin(), positions.end(), byte, [](Position const& p, size_t n) { return p.byte < n; });
	return it != positions.end() && it->byte == byte ? static_cast<int>(it - positions.begin()) : -1;
}

void Fill(wxDC& dc, int x, int y, int width, int height) {
	wxColour colour = dc.GetTextForeground();
	wxDCPenChanger pen(dc, wxPen(colour));
	wxDCBrushChanger brush(dc, wxBrush(colour));
	dc.DrawRectangle(x, y, std::max(width, 1), std::max(height, 1));
}

void DrawMasked(wxDC& dc, wxString const& display, std::string_view utf8, std::span<agi::ass::MaskRange const> ranges, size_t offset, int x, int y) {
	if (ranges.empty()) { dc.DrawText(display, x, y); return; }
	auto extent = dc.GetTextExtent(display);
	std::vector<Position> positions;
	if (!DecodePositions(utf8, display, positions)) { Fill(dc, x, y, extent.GetWidth(), extent.GetHeight()); return; }
#ifndef __WXMSW__
	if (dc.GetLayoutDirection() == wxLayout_RightToLeft || UnsupportedNonWindows(utf8)) {
		Fill(dc, x, y, extent.GetWidth(), extent.GetHeight());
		return;
	}
#endif
#ifdef __WXMSW__
	SCRIPT_STRING_ANALYSIS analysis = nullptr;
	if (!dc.GetHDC() || FAILED(ScriptStringAnalyse(dc.GetHDC(), display.wc_str(), static_cast<int>(display.length()), 0, -1, SSA_GLYPHS | SSA_FALLBACK, 0, nullptr, nullptr, nullptr, nullptr, nullptr, &analysis))) {
		Fill(dc, x, y, extent.GetWidth(), extent.GetHeight());
		return;
	}
	auto cleanup = std::unique_ptr<void, decltype([](void *p) { auto a = static_cast<SCRIPT_STRING_ANALYSIS>(p); ScriptStringFree(&a); })>(analysis, {});
#else
	wxArrayInt advances;
	if (!dc.GetPartialTextExtents(display, advances) || advances.size() < display.length()) {
		Fill(dc, x, y, extent.GetWidth(), extent.GetHeight());
		return;
	}
#endif
	auto draw_visible = [&](size_t begin, size_t end) {
		if (begin >= end) return;
		int first = PositionAt(positions, begin), last = PositionAt(positions, end);
		if (first < 0 || last <= first) return;
		int left, right;
#ifdef __WXMSW__
		if (FAILED(ScriptStringCPtoX(analysis, positions[first].unit, FALSE, &left)) || FAILED(ScriptStringCPtoX(analysis, positions[last].unit - 1, TRUE, &right))) return;
#else
		left = first ? advances[first - 1] : 0;
		right = advances[last - 1];
#endif
		auto run = to_wx(utf8.substr(begin, end - begin));
		dc.DrawText(run, x + std::min(left, right), y);
	};
	size_t visible = 0;
	for (auto const& range : ranges) {
		if (range.start < offset || range.start + range.length < range.start) { Fill(dc, x, y, extent.GetWidth(), extent.GetHeight()); return; }
		size_t begin = range.start - offset;
		if (begin > utf8.size() || range.length > utf8.size() - begin) continue;
		draw_visible(visible, begin);
		int first = PositionAt(positions, begin), last = PositionAt(positions, begin + range.length);
		if (first < 0 || last <= first) { Fill(dc, x, y, extent.GetWidth(), extent.GetHeight()); return; }
		int left, right;
#ifdef __WXMSW__
		if (FAILED(ScriptStringCPtoX(analysis, positions[first].unit, FALSE, &left)) || FAILED(ScriptStringCPtoX(analysis, positions[last].unit - 1, TRUE, &right))) { Fill(dc, x, y, extent.GetWidth(), extent.GetHeight()); return; }
#else
		left = first ? advances[first - 1] : 0;
		right = advances[last - 1];
#endif
		left = std::clamp(left, 0, extent.GetWidth());
		right = std::clamp(right, 0, extent.GetWidth());
		Fill(dc, x + std::min(left, right), y, std::abs(right - left), extent.GetHeight());
		visible = begin + range.length;
	}
	draw_visible(visible, utf8.size());
}
}

void Draw(wxDC& dc, wxString const& text, int x, int y, bool ass_syntax) {
	auto utf8 = Utf8(text);
	auto ranges = agi::ass::DialogueMaskRanges(utf8, ass_syntax);
	DrawMasked(dc, text, utf8, ranges, 0, x, y);
}

void Draw(wxDC& dc, std::string_view text, int x, int y, std::span<agi::ass::MaskRange const> ranges, size_t offset) {
	DrawMasked(dc, to_wx(text), text, ranges, offset, x, y);
}

void DrawBlock(wxDC& dc, int x, int y, int width, int height) { Fill(dc, x, y, width, height); }
}
