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

/// @file video_spoiler_shield.cpp
/// @brief Spoiler shield: occlude the video preview except an adjustable reveal area
/// @ingroup video main_ui
///
/// The spoiler shield lets a user time/typeset subtitles without watching the
/// picture. Everything in the video content rectangle is hidden (solid colour
/// or mosaic) except a single normalized reveal rectangle that the user can move
/// and resize. Because subtitles are burned into the video frame by the async
/// provider before it reaches the display, hiding the frame also hides them.
/// This only affects the on-screen preview; exports keep the original picture.

#include "video_display.h"

#include "video_frame.h"
#include "compat.h"
#include "format.h"
#include "options.h"

#include <libaegisub/color.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

#ifdef HAVE_OPENGL_GL_H
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace {
const char *OPT_ENABLED = "Video/Spoiler Shield/Enabled";
const char *OPT_EFFECT = "Video/Spoiler Shield/Effect";
const char *OPT_COLOR = "Video/Spoiler Shield/Color";
const char *OPT_MOSAIC = "Video/Spoiler Shield/Mosaic Block";
const char *OPT_RECT = "Video/Spoiler Shield/Reveal Rect";

const double DEFAULT_X = 0.1;
const double DEFAULT_Y = 0.68;
const double DEFAULT_W = 0.8;
const double DEFAULT_H = 0.26;
}

bool VideoDisplay::SpoilerShieldOn() const {
	return OPT_GET(OPT_ENABLED)->GetBool();
}

void VideoDisplay::ReloadSpoilerShieldRect() {
	// Effect 2 used to mean the removed blur mode; fall back to the solid fill.
	int effect = (int)OPT_GET(OPT_EFFECT)->GetInt();
	if (effect != 0 && effect != 1)
		OPT_SET(OPT_EFFECT)->SetInt(0);

	double x = DEFAULT_X, y = DEFAULT_Y, w = DEFAULT_W, h = DEFAULT_H;
	std::string s = OPT_GET(OPT_RECT)->GetString();
	if (std::sscanf(s.c_str(), "%lf,%lf,%lf,%lf", &x, &y, &w, &h) != 4) {
		x = DEFAULT_X; y = DEFAULT_Y; w = DEFAULT_W; h = DEFAULT_H;
	}

	w = std::clamp(w, 0.02, 1.0);
	h = std::clamp(h, 0.02, 1.0);
	x = std::clamp(x, 0.0, 1.0 - w);
	y = std::clamp(y, 0.0, 1.0 - h);

	shield_rect_x = x;
	shield_rect_y = y;
	shield_rect_w = w;
	shield_rect_h = h;
}

void VideoDisplay::PersistSpoilerShieldRect() {
	OPT_SET(OPT_RECT)->SetString(agi::format("%g,%g,%g,%g",
		shield_rect_x, shield_rect_y, shield_rect_w, shield_rect_h));
}

void VideoDisplay::ResetSpoilerShieldRect() {
	shield_rect_x = DEFAULT_X;
	shield_rect_y = DEFAULT_Y;
	shield_rect_w = DEFAULT_W;
	shield_rect_h = DEFAULT_H;
	PersistSpoilerShieldRect();
}

void VideoDisplay::RebuildShieldTextureIfNeeded() {
	if (!shield_src) return;

	// Only the mosaic needs a texture; the solid fill is drawn in flat colour.
	if (OPT_GET(OPT_EFFECT)->GetInt() != 1) return;

	int block = std::clamp((int)OPT_GET(OPT_MOSAIC)->GetInt(), 2, 128);

	VideoFrame const& f = *shield_src;
	int pw = std::max(1, (f.width + block - 1) / block);
	int ph = std::max(1, (f.height + block - 1) / block);

	bool rebuild = shield_tex_dirty
		|| block != shield_block
		|| shield_pw != pw || shield_ph != ph
		|| (int)shield_pixels.size() != pw * ph * 4;
	if (!rebuild) return;

	// Box-average each block into one RGBA texel and upscale it with nearest
	// neighbour filtering, so every block becomes one crisp colour square.
	// Averaging keeps a block's colour stable while the frame changes.
	shield_pixels.assign((size_t)pw * ph * 4, 255);
	for (int oy = 0; oy < ph; ++oy) {
		int py0 = oy * block;
		int py1 = std::min(f.height, py0 + block);
		for (int ox = 0; ox < pw; ++ox) {
			int x0 = ox * block;
			int x1 = std::min(f.width, x0 + block);
			int64_t sb = 0, sg = 0, sr = 0, n = 0;
			for (int py = py0; py < py1; ++py) {
				int row = f.flipped ? (f.height - 1 - py) : py;
				if (row < 0 || row >= f.height) continue;
				unsigned char const *p = f.data.data()
					+ (size_t)row * f.pitch + (size_t)x0 * 4;
				for (int x = x0; x < x1; ++x, p += 4) {
					sb += p[0]; sg += p[1]; sr += p[2]; ++n;
				}
			}
			unsigned char *d = shield_pixels.data() + ((size_t)oy * pw + ox) * 4;
			if (n) {
				d[0] = (unsigned char)(sr / n);
				d[1] = (unsigned char)(sg / n);
				d[2] = (unsigned char)(sb / n);
				d[3] = 255;
			}
		}
	}

	shield_pw = pw;
	shield_ph = ph;
	shield_block = block;

	if (!shield_tex) glGenTextures(1, &shield_tex);
	glBindTexture(GL_TEXTURE_2D, shield_tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pw, ph, 0, GL_RGBA, GL_UNSIGNED_BYTE, shield_pixels.data());

	shield_tex_dirty = false;
}

void VideoDisplay::RenderSpoilerShield(int, int) {
	if (!content_width || !content_height) return;

	double sf = scale_factor ? scale_factor : 1.0;
	double cxl = content_left / sf;
	double cyl = content_top / sf;
	double cwl = content_width / sf;
	double chl = content_height / sf;
	double cright = cxl + cwl;
	double cbottom = cyl + chl;

	// Reveal rectangle in device (logical) coordinates
	double rx = cxl + shield_rect_x * cwl;
	double ry = cyl + shield_rect_y * chl;
	double rr = rx + shield_rect_w * cwl;
	double rb = ry + shield_rect_h * chl;

	struct Rect { double x1, y1, x2, y2; };
	// Cover everything inside the video content rectangle except the reveal rect
	Rect rects[4] = {
		{ cxl, cyl, cright, ry },                                    // above
		{ cxl, rb, cright, cbottom },                                // below
		{ cxl, std::max(cyl, ry), rx, std::min(cbottom, rb) },       // left
		{ rr, std::max(cyl, ry), cright, std::min(cbottom, rb) },    // right
	};

	bool textured = OPT_GET(OPT_EFFECT)->GetInt() == 1;
	if (textured) {
		RebuildShieldTextureIfNeeded();
		textured = shield_tex != 0 && shield_pw > 0;
	}

	glDisable(GL_BLEND);

	if (!textured) {
		agi::Color col = OPT_GET(OPT_COLOR)->GetColor();
		glDisable(GL_TEXTURE_2D);
		glColor3ub(col.r, col.g, col.b);
		glBegin(GL_QUADS);
		for (auto& r : rects) {
			if (r.x2 <= r.x1 || r.y2 <= r.y1) continue;
			glVertex2d(r.x1, r.y1); glVertex2d(r.x2, r.y1);
			glVertex2d(r.x2, r.y2); glVertex2d(r.x1, r.y2);
		}
		glEnd();
	}
	else {
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, shield_tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glColor3ub(255, 255, 255);
		glBegin(GL_QUADS);
		for (auto& r : rects) {
			if (r.x2 <= r.x1 || r.y2 <= r.y1) continue;
			double u1 = (r.x1 - cxl) / cwl, u2 = (r.x2 - cxl) / cwl;
			double v1 = (r.y1 - cyl) / chl, v2 = (r.y2 - cyl) / chl;
			glTexCoord2d(u1, v1); glVertex2d(r.x1, r.y1);
			glTexCoord2d(u2, v1); glVertex2d(r.x2, r.y1);
			glTexCoord2d(u2, v2); glVertex2d(r.x2, r.y2);
			glTexCoord2d(u1, v2); glVertex2d(r.x1, r.y2);
		}
		glEnd();
		glDisable(GL_TEXTURE_2D);
	}

	// Reveal rectangle outline; handles on the corners and on the midpoint of
	// every border show that each border can be dragged, not just the corners.
	glColor3ub(40, 160, 255);
	glLineWidth(2.0f);
	glBegin(GL_LINE_LOOP);
	glVertex2d(rx, ry); glVertex2d(rr, ry); glVertex2d(rr, rb); glVertex2d(rx, rb);
	glEnd();

	const double hs = 4.0;
	double mx = (rx + rr) * 0.5;
	double my = (ry + rb) * 0.5;
	double handles[8][2] = {
		{rx, ry}, {rr, ry}, {rr, rb}, {rx, rb},
		{mx, ry}, {rr, my}, {mx, rb}, {rx, my}
	};
	glBegin(GL_QUADS);
	for (auto& h : handles) {
		glVertex2d(h[0] - hs, h[1] - hs); glVertex2d(h[0] + hs, h[1] - hs);
		glVertex2d(h[0] + hs, h[1] + hs); glVertex2d(h[0] - hs, h[1] + hs);
	}
	glEnd();
	glLineWidth(1.0f);
}

bool VideoDisplay::ShieldRectWindow(double *x1, double *y1, double *x2, double *y2) const {
	if (!content_width || !content_height) return false;

	double sf = scale_factor ? scale_factor : 1.0;
	double cwl = content_width / sf;
	double chl = content_height / sf;
	if (cwl <= 0 || chl <= 0) return false;

	*x1 = content_left / sf + shield_rect_x * cwl;
	*y1 = content_top / sf + shield_rect_y * chl;
	*x2 = *x1 + shield_rect_w * cwl;
	*y2 = *y1 + shield_rect_h * chl;
	return true;
}

VideoDisplay::ShieldDrag VideoDisplay::ShieldHitTest(double px, double py) const {
	double rx, ry, rr, rb;
	if (!ShieldRectWindow(&rx, &ry, &rr, &rb)) return ShieldDrag::None;

	// Any of the four borders can be grabbed, not only the corners: the grab
	// band is a few pixels wide on both sides of each border, and the corners
	// win where two bands overlap.
	const double hit = 8.0;
	bool left = std::abs(px - rx) <= hit;
	bool right = std::abs(px - rr) <= hit;
	bool top = std::abs(py - ry) <= hit;
	bool bottom = std::abs(py - rb) <= hit;
	bool in_x = px >= rx - hit && px <= rr + hit;
	bool in_y = py >= ry - hit && py <= rb + hit;

	if (left && top) return ShieldDrag::TL;
	if (right && top) return ShieldDrag::TR;
	if (left && bottom) return ShieldDrag::BL;
	if (right && bottom) return ShieldDrag::BR;
	if (left && in_y) return ShieldDrag::Left;
	if (right && in_y) return ShieldDrag::Right;
	if (top && in_x) return ShieldDrag::Top;
	if (bottom && in_x) return ShieldDrag::Bottom;
	if (px > rx && px < rr && py > ry && py < rb) return ShieldDrag::Move;
	return ShieldDrag::None;
}

void VideoDisplay::UpdateShieldCursor(double px, double py) {
	switch (ShieldHitTest(px, py)) {
		case ShieldDrag::Left:
		case ShieldDrag::Right:
			SetCursor(wxCursor(wxCURSOR_SIZEWE));
			break;
		case ShieldDrag::Top:
		case ShieldDrag::Bottom:
			SetCursor(wxCursor(wxCURSOR_SIZENS));
			break;
		case ShieldDrag::TL:
		case ShieldDrag::BR:
			SetCursor(wxCursor(wxCURSOR_SIZENWSE));
			break;
		case ShieldDrag::TR:
		case ShieldDrag::BL:
			SetCursor(wxCursor(wxCURSOR_SIZENESW));
			break;
		case ShieldDrag::Move:
			SetCursor(wxCursor(wxCURSOR_SIZING));
			break;
		default:
			SetCursor(wxCursor(wxCURSOR_ARROW));
			break;
	}
}

bool VideoDisplay::HandleSpoilerShieldMouse(wxMouseEvent &event) {
	double rx, ry, rr, rb;
	if (!ShieldRectWindow(&rx, &ry, &rr, &rb)) return true;

	double px = (double)event.GetX();
	double py = (double)event.GetY();

	if (event.LeftDown()) {
		shield_drag = ShieldHitTest(px, py);
		if (shield_drag == ShieldDrag::Move)
			shield_drag_last = Vector2D(px, py);
		if (shield_drag != ShieldDrag::None && !HasCapture())
			CaptureMouse();
		UpdateShieldCursor(px, py);
		return true;
	}

	if (event.Dragging() && event.LeftIsDown() && shield_drag != ShieldDrag::None) {
		double sf = scale_factor ? scale_factor : 1.0;
		double cwl = content_width / sf;
		double chl = content_height / sf;

		if (shield_drag == ShieldDrag::Move) {
			double dx = (px - shield_drag_last.X()) / cwl;
			double dy = (py - shield_drag_last.Y()) / chl;
			shield_rect_x = std::clamp(shield_rect_x + dx, 0.0, 1.0 - shield_rect_w);
			shield_rect_y = std::clamp(shield_rect_y + dy, 0.0, 1.0 - shield_rect_h);
			shield_drag_last = Vector2D(px, py);
		}
		else {
			// The border opposite the grabbed one stays put, and the window keeps
			// a minimum size of 24 logical pixels.
			double nx = std::clamp((px - content_left / sf) / cwl, 0.0, 1.0);
			double ny = std::clamp((py - content_top / sf) / chl, 0.0, 1.0);
			double x1 = shield_rect_x, y1 = shield_rect_y;
			double x2 = shield_rect_x + shield_rect_w, y2 = shield_rect_y + shield_rect_h;
			double minw = std::min(0.5, 24.0 / cwl);
			double minh = std::min(0.5, 24.0 / chl);

			switch (shield_drag) {
				case ShieldDrag::Left:
					x1 = std::clamp(nx, 0.0, std::max(0.0, x2 - minw));
					break;
				case ShieldDrag::Right:
					x2 = std::clamp(nx, std::min(1.0, x1 + minw), 1.0);
					break;
				case ShieldDrag::Top:
					y1 = std::clamp(ny, 0.0, std::max(0.0, y2 - minh));
					break;
				case ShieldDrag::Bottom:
					y2 = std::clamp(ny, std::min(1.0, y1 + minh), 1.0);
					break;
				case ShieldDrag::TL:
					x1 = std::clamp(nx, 0.0, std::max(0.0, x2 - minw));
					y1 = std::clamp(ny, 0.0, std::max(0.0, y2 - minh));
					break;
				case ShieldDrag::TR:
					x2 = std::clamp(nx, std::min(1.0, x1 + minw), 1.0);
					y1 = std::clamp(ny, 0.0, std::max(0.0, y2 - minh));
					break;
				case ShieldDrag::BL:
					x1 = std::clamp(nx, 0.0, std::max(0.0, x2 - minw));
					y2 = std::clamp(ny, std::min(1.0, y1 + minh), 1.0);
					break;
				case ShieldDrag::BR:
					x2 = std::clamp(nx, std::min(1.0, x1 + minw), 1.0);
					y2 = std::clamp(ny, std::min(1.0, y1 + minh), 1.0);
					break;
				default:
					break;
			}

			shield_rect_x = x1;
			shield_rect_y = y1;
			shield_rect_w = x2 - x1;
			shield_rect_h = y2 - y1;
		}
		Render();
		return true;
	}

	if (event.LeftUp()) {
		if (shield_drag != ShieldDrag::None) {
			shield_drag = ShieldDrag::None;
			if (HasCapture()) ReleaseMouse();
			PersistSpoilerShieldRect();
			UpdateShieldCursor(px, py);
		}
		return true;
	}

	// Hovering a border or a handle previews the resize direction in the cursor.
	UpdateShieldCursor(px, py);
	return true;
}
