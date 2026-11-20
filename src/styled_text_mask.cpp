// Copyright (c) 2026, BoxMiao007
// SPDX-License-Identifier: ISC

#include "styled_text_mask.h"

#include <libaegisub/dialogue_text_mask.h>

#include <wx/clntdata.h>
#include <wx/stc/stc.h>

namespace {
constexpr int ink = wxSTC_INDIC_CONTAINER;
constexpr int block = ink + 1;

struct SavedSelection final : wxClientData {
	int alpha, additional_alpha;
	explicit SavedSelection(wxStyledTextCtrl &ctrl) : alpha(ctrl.GetSelAlpha()), additional_alpha(ctrl.GetAdditionalSelAlpha()) { }
};

void Configure(wxStyledTextCtrl &ctrl) {
	if (!ctrl.GetClientObject()) ctrl.SetClientObject(new SavedSelection(ctrl));
	ctrl.SetSelAlpha(120);
	ctrl.SetAdditionalSelAlpha(120);
	// 隐去可能伸出字格的字形笔画；色块仍由 Scintilla 用原文排版后的坐标绘制。
	ctrl.IndicatorSetStyle(ink, wxSTC_INDIC_TEXTFORE);
	ctrl.IndicatorSetFlags(ink, wxSTC_INDICFLAG_VALUEFORE);
	ctrl.IndicatorSetStyle(block, wxSTC_INDIC_FULLBOX);
	ctrl.IndicatorSetFlags(block, wxSTC_INDICFLAG_VALUEFORE);
	ctrl.IndicatorSetAlpha(ink, wxSTC_ALPHA_OPAQUE);
	ctrl.IndicatorSetOutlineAlpha(ink, wxSTC_ALPHA_OPAQUE);
	ctrl.IndicatorSetAlpha(block, wxSTC_ALPHA_OPAQUE);
	ctrl.IndicatorSetOutlineAlpha(block, wxSTC_ALPHA_OPAQUE);
	ctrl.IndicatorSetUnder(block, false);
}

// 从原语法前景和背景取色，交替深浅，让紧邻的字素也各有一个色块。
int BlockColour(wxColour fore, wxColour back, int weight) {
	auto mix = [=](int f, int b) { return (f * weight + b * (5 - weight)) / 5; };
	return wxSTC_INDICVALUEBIT | wxColour(mix(fore.Red(), back.Red()), mix(fore.Green(), back.Green()), mix(fore.Blue(), back.Blue())).GetRGB();
}
}

namespace subtitle_mask {
void Apply(wxStyledTextCtrl &ctrl, bool enabled, bool ass_syntax) {

	const int old_indicator = ctrl.GetIndicatorCurrent();
	const int old_value = ctrl.GetIndicatorValue();
	const int length = ctrl.GetTextLength();
	if (!enabled) {
		if (auto saved = dynamic_cast<SavedSelection *>(ctrl.GetClientObject())) {
			ctrl.SetSelAlpha(saved->alpha);
			ctrl.SetAdditionalSelAlpha(saved->additional_alpha);
			ctrl.SetClientObject(nullptr);
		}
		for (int id : {ink, block}) {
			ctrl.SetIndicatorCurrent(id);
			ctrl.IndicatorClearRange(0, length);
		}
	}
	else {
		for (int id : {ink, block}) {
			ctrl.SetIndicatorCurrent(id);
			ctrl.IndicatorClearRange(0, length);
		}
		Configure(ctrl);
		auto ranges = agi::ass::DialogueMaskRanges({ctrl.GetCharacterPointer(), static_cast<size_t>(length)}, ass_syntax);
		int end = 0, last_style = -1, background = 0, shades[2]{};
		bool alternate = false;
		auto clear_gap = [&](int next) {
			if (next == end) return;
			for (int id : {ink, block}) {
				ctrl.SetIndicatorCurrent(id);
				ctrl.IndicatorClearRange(end, next - end);
			}
		};
		for (auto const& range : ranges) {
			int start = static_cast<int>(range.start), count = static_cast<int>(range.length);
			clear_gap(start);
			int style = ctrl.GetStyleAt(start);
			if (style != last_style) {
				last_style = style;
				auto bg = ctrl.StyleGetBackground(style);
				auto fg = ctrl.StyleGetForeground(style);
				background = wxSTC_INDICVALUEBIT | bg.GetRGB();
				shades[0] = BlockColour(fg, bg, 4);
				shades[1] = BlockColour(fg, bg, 3);
			}
			ctrl.SetIndicatorCurrent(ink);
			ctrl.SetIndicatorValue(background);
			ctrl.IndicatorFillRange(start, count);
			ctrl.SetIndicatorCurrent(block);
			ctrl.SetIndicatorValue(shades[alternate]);
			ctrl.IndicatorFillRange(start, count);
			alternate = !alternate;
			end = start + count;
		}
		clear_gap(length);
	}
	ctrl.SetIndicatorCurrent(old_indicator);
	ctrl.SetIndicatorValue(old_value);
}
}
