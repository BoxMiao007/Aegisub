// Copyright (c) 2026, BoxMiao007
// SPDX-License-Identifier: ISC

#pragma once

#include <cstddef>
#include <string>
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

	/// 规范化替换符：空串、非法 UTF-8、含 ASS 特殊字符（{} \ 换行）时回退为 "X"。
	std::string NormalizeMaskToken(std::string_view token);

	/// 把正文中每个需要遮挡的字素替换成 token，保留标点、空白、转义、标签与 \p 绘图。
	/// 用于视频预览：只改渲染用的副本，不接触真实台词。
	std::string DialogueMaskReplace(std::string_view text, std::string_view token, bool ass_syntax = true);

	/// 对整份 ASS 文本只替换 Dialogue 行正文，供预览渲染副本使用。
	std::string MaskAssData(std::string_view data, std::string_view token);
}
