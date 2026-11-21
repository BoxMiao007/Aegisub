// Copyright (c) 2026, BoxMiao007
// SPDX-License-Identifier: ISC

#include <libaegisub/dialogue_text_mask.h>

#include <main.h>

#include <initializer_list>
#include <string_view>

namespace {
using agi::ass::DialogueMaskRanges;
using agi::ass::MaskRange;
using agi::ass::DialogueMaskReplace;
using agi::ass::NormalizeMaskToken;

void expect_ranges(std::string_view text, std::initializer_list<MaskRange> expected, bool ass_syntax = true) {
	auto actual = DialogueMaskRanges(text, ass_syntax);
	ASSERT_EQ(expected.size(), actual.size());
	auto want = expected.begin();
	for (auto const& got : actual) {
		EXPECT_EQ(want->start, got.start);
		EXPECT_EQ(want->length, got.length);
		++want;
	}
}
}

TEST(lagi_dialogue_text_mask, hides_each_readable_grapheme) {
	expect_ranges("A1", {{0, 1}, {1, 1}});
	expect_ranges("中", {{0, 3}});
	expect_ranges("e\xCC\x81", {{0, 3}});
	expect_ranges("\xF0\x9F\x87\xA8\xF0\x9F\x87\xB3", {{0, 8}}); // regional-indicator flag
	expect_ranges("\xF0\x9F\x91\xA9\xF0\x9F\x8F\xBD\xE2\x80\x8D\xF0\x9F\x92\xBB", {{0, 15}}); // emoji ZWJ sequence
}

TEST(lagi_dialogue_text_mask, keeps_unicode_punctuation_and_whitespace) {
	expect_ranges(" .,!?:;()[]{}\t\r\n\xC2\xA0\xE3\x80\x80\xEF\xBC\x81", {});
	expect_ranges("a\xE3\x80\x80" "b", {{0, 1}, {4, 1}});
}

TEST(lagi_dialogue_text_mask, keeps_only_supported_ass_escapes) {
	expect_ranges("a\\N!\\n中\\h2\\q", {{0, 1}, {6, 3}, {11, 1}, {13, 1}});
	expect_ranges("\\\\N", {{2, 1}});
	expect_ranges("\\N\\n\\h", {}, true);
	expect_ranges("\\N\\n\\h", {{1, 1}, {3, 1}, {5, 1}}, false);
}

TEST(lagi_dialogue_text_mask, preserves_closed_blocks_and_masks_surrounding_text) {
	expect_ranges("a{note}{\\pos(1,2)}字\\Nb", {{0, 1}, {18, 3}, {23, 1}});
	expect_ranges("{unfinished{tag}secret", {{16, 1}, {17, 1}, {18, 1}, {19, 1}, {20, 1}, {21, 1}});
}

TEST(lagi_dialogue_text_mask, malformed_and_escaped_braces_fail_closed) {
	expect_ranges("{unterminated secret", {
		{1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1}, {6, 1}, {7, 1}, {8, 1}, {9, 1}, {10, 1}, {11, 1}, {12, 1},
		{14, 1}, {15, 1}, {16, 1}, {17, 1}, {18, 1}, {19, 1}
	});
	expect_ranges("\\{literal\\}", {
		{2, 1}, {3, 1}, {4, 1}, {5, 1}, {6, 1}, {7, 1}, {8, 1}
	});
}

TEST(lagi_dialogue_text_mask, drawing_text_and_symbols_are_not_exempt) {
	expect_ranges("{\\p1}m 10 10{\\p0}", {{5, 1}, {7, 1}, {8, 1}, {10, 1}, {11, 1}});
	expect_ranges("$+\xE2\x88\x91\xC2\xB2\xE2\x98\x85", {
		{0, 1}, {1, 1}, {2, 3}, {5, 2}, {7, 3}
	}, false);
}

TEST(lagi_dialogue_text_mask, replacement_keeps_punctuation_and_replaces_each_grapheme) {
	EXPECT_EQ("XX\xEF\xBC\x8C" "XX\xEF\xBC\x81",
		DialogueMaskReplace("\xE5\x88\xA5\xE5\x8E\xBB\xEF\xBC\x8C" "B2" "\xEF\xBC\x81", "X"));
	EXPECT_EQ("XX", DialogueMaskReplace("e\xCC\x81Z", "X"));
	EXPECT_EQ("####", DialogueMaskReplace("ab", "##"));
}

TEST(lagi_dialogue_text_mask, replacement_keeps_tags_escapes_and_drawings) {
	EXPECT_EQ("{\\i1}X\\NX{\\p1}m 0 0 l 10 10{\\p0}X",
		DialogueMaskReplace("{\\i1}\xE5\x88\xA5\\N\xE5\x8E\xBB{\\p1}m 0 0 l 10 10{\\p0}\xE5\x90\x8E", "X"));
}

TEST(lagi_dialogue_text_mask, replacement_token_is_normalized) {
	EXPECT_EQ("X", NormalizeMaskToken(""));
	EXPECT_EQ("X", NormalizeMaskToken("{x}"));
	EXPECT_EQ("X", NormalizeMaskToken("a\\b"));
	EXPECT_EQ("ab", NormalizeMaskToken("ab"));
	EXPECT_EQ("X", DialogueMaskReplace("a", "{x}"));
}

TEST(lagi_dialogue_text_mask, malformed_utf8_is_masked) {
	expect_ranges("a\xFF" "b", {{0, 3}});
	expect_ranges("a\xE2\x82", {{0, 3}});
}