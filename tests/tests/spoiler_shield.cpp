// Copyright (c) 2026, BoxMiao007
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//
// Aegisub Project http://www.aegisub.org/

#include "../../src/spoiler_shield_geometry.h"

#include <main.h>

#include <cmath>

using spoiler::Grab;
using spoiler::Rect;
using spoiler::WindowRect;

namespace {
bool Near(double a, double b) { return std::abs(a - b) < 1e-9; }
}

TEST(lagi_spoiler_shield, parse_valid) {
	Rect r;
	ASSERT_TRUE(spoiler::ParseRect("0.1,0.2,0.3,0.4", r));
	EXPECT_TRUE(Near(r.x, 0.1));
	EXPECT_TRUE(Near(r.y, 0.2));
	EXPECT_TRUE(Near(r.w, 0.3));
	EXPECT_TRUE(Near(r.h, 0.4));

	// Whitespace around the numbers is tolerated.
	ASSERT_TRUE(spoiler::ParseRect(" 0.5 , 0.25 , 0.5 , 0.5 ", r));
	EXPECT_TRUE(Near(r.x, 0.5));
	EXPECT_TRUE(Near(r.w, 0.5));
}

TEST(lagi_spoiler_shield, parse_rejects_invalid) {
	Rect r;
	EXPECT_FALSE(spoiler::ParseRect("", r));
	EXPECT_FALSE(spoiler::ParseRect("1,2,3", r));
	EXPECT_FALSE(spoiler::ParseRect("1,2,3,4,5", r));
	EXPECT_FALSE(spoiler::ParseRect("a,b,c,d", r));
	EXPECT_FALSE(spoiler::ParseRect("0.1,0.2,0,0.4", r));
	EXPECT_FALSE(spoiler::ParseRect("0.1,0.2,0.3,-0.4", r));
	EXPECT_FALSE(spoiler::ParseRect("nan,0.2,0.3,0.4", r));
	EXPECT_FALSE(spoiler::ParseRect("0.1,inf,0.3,0.4", r));
}

TEST(lagi_spoiler_shield, parse_clamps_to_bounds) {
	Rect r;
	// Fully inside is preserved.
	ASSERT_TRUE(spoiler::ParseRect("0.1,0.1,0.2,0.2", r));
	EXPECT_TRUE(Near(r.x, 0.1));

	// Walking off the edge slides the rectangle back into the frame instead of
	// shrinking or dropping it.
	ASSERT_TRUE(spoiler::ParseRect("0.9,0.9,0.6,0.6", r));
	EXPECT_TRUE(Near(r.x, 0.4));
	EXPECT_TRUE(Near(r.y, 0.4));
	EXPECT_TRUE(Near(r.w, 0.6));
	EXPECT_TRUE(Near(r.h, 0.6));

	// Oversized dimensions clamp to the full frame.
	ASSERT_TRUE(spoiler::ParseRect("0,0,5,5", r));
	EXPECT_TRUE(Near(r.w, 1.0));
	EXPECT_TRUE(Near(r.h, 1.0));
}

TEST(lagi_spoiler_shield, format_round_trip) {
	Rect r;
	ASSERT_TRUE(spoiler::ParseRect(spoiler::FormatRect(Rect{0.125, 0.678, 0.5, 0.25}), r));
	EXPECT_TRUE(Near(r.x, 0.125));
	EXPECT_TRUE(Near(r.y, 0.678));
	EXPECT_TRUE(Near(r.w, 0.5));
	EXPECT_TRUE(Near(r.h, 0.25));
}

TEST(lagi_spoiler_shield, to_window) {
	WindowRect video{10, 20, 110, 220};
	WindowRect r = spoiler::ToWindow(Rect{0.5, 0.5, 0.5, 0.5}, video);
	EXPECT_TRUE(Near(r.left, 60));
	EXPECT_TRUE(Near(r.top, 120));
	EXPECT_TRUE(Near(r.right, 110));
	EXPECT_TRUE(Near(r.bottom, 220));
}

TEST(lagi_spoiler_shield, hit_test) {
	WindowRect rect{20, 20, 80, 80};

	EXPECT_EQ(Grab::Left, spoiler::HitTest(rect, 20, 50));
	EXPECT_EQ(Grab::Right, spoiler::HitTest(rect, 80, 50));
	EXPECT_EQ(Grab::Top, spoiler::HitTest(rect, 50, 20));
	EXPECT_EQ(Grab::Bottom, spoiler::HitTest(rect, 50, 80));
	EXPECT_EQ(Grab::Move, spoiler::HitTest(rect, 50, 50));
	EXPECT_EQ(Grab::Outside, spoiler::HitTest(rect, 200, 200));

	Grab corner = spoiler::HitTest(rect, 20, 20);
	EXPECT_TRUE(spoiler::HasEdge(corner, Grab::Left));
	EXPECT_TRUE(spoiler::HasEdge(corner, Grab::Top));

	Grab corner2 = spoiler::HitTest(rect, 80, 80);
	EXPECT_TRUE(spoiler::HasEdge(corner2, Grab::Right));
	EXPECT_TRUE(spoiler::HasEdge(corner2, Grab::Bottom));
}

TEST(lagi_spoiler_shield, move_clamps) {
	Rect r{0.1, 0.1, 0.2, 0.2};
	spoiler::Move(r, 0.9, 0.9);
	EXPECT_TRUE(Near(r.x, 0.8));
	EXPECT_TRUE(Near(r.y, 0.8));
	EXPECT_TRUE(Near(r.w, 0.2));

	spoiler::Move(r, -2.0, -2.0);
	EXPECT_TRUE(Near(r.x, 0.0));
	EXPECT_TRUE(Near(r.y, 0.0));
}

TEST(lagi_spoiler_shield, resize_minimum_is_24px) {
	WindowRect video{0, 0, 800, 600};

	// Dragging the right edge far past the left edge must keep a 24 logical pixel
	// minimum width rather than collapsing the reveal rectangle.
	Rect r{0.2, 0.2, 0.5, 0.5};
	spoiler::Resize(r, Grab::Right, video, 0, 300);
	EXPECT_TRUE(Near(r.w, 24.0 / 800.0));
	EXPECT_TRUE(Near(r.x, 0.2));

	// Same for the bottom edge.
	Rect r2{0.2, 0.2, 0.5, 0.5};
	spoiler::Resize(r2, Grab::Bottom, video, 400, 0);
	EXPECT_TRUE(Near(r2.h, 24.0 / 600.0));
	EXPECT_TRUE(Near(r2.y, 0.2));

	// A normal resize keeps the opposite edge fixed.
	Rect r3{0.2, 0.2, 0.5, 0.5};
	spoiler::Resize(r3, Grab::Left, video, 400, 300);
	EXPECT_TRUE(Near(r3.x, 0.5));
	EXPECT_TRUE(Near(r3.w, 0.2));
}

TEST(lagi_spoiler_shield, texture_extent_power_of_two) {
	EXPECT_EQ(1, spoiler::TextureExtent(1, 4096));
	EXPECT_EQ(128, spoiler::TextureExtent(100, 4096));
	EXPECT_EQ(128, spoiler::TextureExtent(128, 4096));
	EXPECT_EQ(256, spoiler::TextureExtent(129, 4096));
	EXPECT_EQ(2048, spoiler::TextureExtent(2048, 2048));

	// Too large for the driver, or garbage input, fails closed with 0.
	EXPECT_EQ(0, spoiler::TextureExtent(100, 64));
	EXPECT_EQ(0, spoiler::TextureExtent(2049, 2048));
	EXPECT_EQ(0, spoiler::TextureExtent(0, 4096));
	EXPECT_EQ(0, spoiler::TextureExtent(10, 0));
}

TEST(lagi_spoiler_shield, mosaic_axis) {
	// 1920 wide with 16px blocks => 120 texels, padded to 128.
	auto ax = spoiler::MosaicAxis(1920, 16, 4096);
	EXPECT_EQ(120, ax.used);
	EXPECT_EQ(128, ax.allocated);
	EXPECT_TRUE(Near(ax.uv, 120.0 / 128.0));

	// A single block collapses to a 1x1 texture.
	auto small = spoiler::MosaicAxis(10, 16, 4096);
	EXPECT_EQ(1, small.used);
	EXPECT_EQ(1, small.allocated);

	// Unsupported size fails closed.
	auto bad = spoiler::MosaicAxis(100000, 2, 1024);
	EXPECT_EQ(0, bad.allocated);
}
