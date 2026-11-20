// Copyright (c) 2026, BoxMiao007
// SPDX-License-Identifier: ISC

#pragma once

class wxStyledTextCtrl;

namespace subtitle_mask {
// 只修改显示。正文、语法样式或遮挡开关更新后调用，不修改选择、撤销栈或文档字节。
// 控件须为 UTF-8；指示器从 wxSTC_INDIC_CONTAINER 开始。
void Apply(wxStyledTextCtrl &ctrl, bool enabled, bool ass_syntax = true);
}
