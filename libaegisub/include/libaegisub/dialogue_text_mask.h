// Copyright (c) 2026, BoxMiao007
// SPDX-License-Identifier: ISC

#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace agi::ass {
	struct MaskRange {
		std::size_t start;
		std::size_t length;
	};

	/// 按原文顺序返回要遮挡的字素簇，每段的起点、长度均为 UTF-8 字节数。
	/// 保留 Unicode 标点和空白；数字、符号（包括表情）以及无法解码的字节仍需遮挡。
	/// ASS 模式保留转义和已闭合的 {...}；未闭合的左括号不会使后续正文可见。
	/// 不因 \p 豁免括号外的绘图命令或坐标，以免无效绘图暴露正文。
	std::vector<MaskRange> DialogueMaskRanges(std::string_view text, bool ass_syntax = true);

	/// 返回覆盖该字节区间的字素序号；未被任何范围覆盖（标点、空白、绘图等）时返回 ranges.size()。
	std::size_t MaskRangeIndexAt(const std::vector<MaskRange>& ranges, std::size_t offset, std::size_t length);
}
