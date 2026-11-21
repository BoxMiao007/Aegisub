#pragma once

#include <libaegisub/dialogue_text_mask.h>

#include <span>
#include <string_view>

class wxDC;
class wxString;

namespace subtitle_mask {
	// 保留原文字形的排版位置；只有可读区段会交给 DC 绘制文字。
	void Draw(wxDC &dc, wxString const& text, int x, int y, bool ass_syntax = true);

	// ranges 来自整行；text 可以是其中的一段，offset 是该段在整行中的 UTF-8 字节偏移。
	void Draw(wxDC &dc, std::string_view text, int x, int y,
	          std::span<agi::ass::MaskRange const> ranges, size_t offset = 0);

	// 已按字素排好位置的控件可直接使用自己的原始单元尺寸。
	void DrawBlock(wxDC &dc, int x, int y, int width, int height);
}
