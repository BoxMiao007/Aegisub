// Copyright (c) 2026, BoxMiao007
// SPDX-License-Identifier: ISC
#include "../../src/video_preview_mask.h"
#include <main.h>

TEST(video_preview_mask, rejects_frames_from_earlier_modes) {
	VideoPreviewMask mode;
	auto original = mode.Capture();
	EXPECT_FALSE(original.Masked());
	EXPECT_TRUE(mode.Accept(original));
	EXPECT_TRUE(mode.Set(true));
	EXPECT_FALSE(mode.Accept(original));
	auto masked = mode.Capture();
	EXPECT_TRUE(masked.Masked());
	EXPECT_FALSE(mode.Set(true));
	EXPECT_TRUE(mode.Accept(masked));
	mode.Set(false);
	mode.Set(true);
	EXPECT_FALSE(mode.Accept(masked));
	EXPECT_TRUE(mode.Accept(mode.Capture()));
}

TEST(video_preview_mask, rejects_frames_from_another_provider) {
	VideoPreviewMask first, second;
	EXPECT_FALSE(second.Accept(first.Capture()));
}
