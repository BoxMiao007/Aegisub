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
/// picture. Everything in the video content rectangle is hidden (solid, mosaic
/// or blur) except a single normalized reveal rectangle that the user can move
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
const char *OPT_BLUR = "Video/Spoiler Shield/Blur Strength";
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

	int effect = (int)OPT_GET(OPT_EFFECT)->GetInt();
	if (effect == 0) return; // solid fill needs no texture

	int block = effect == 1
		? (int)OPT_GET(OPT_MOSAIC)->GetInt()
		: (int)OPT_GET(OPT_BLUR)->GetInt();
	block = std::clamp(block, 2, 128);

	VideoFrame const& f = *shield_src;
	int pw = std::max(1, (f.width + block - 1) / block);
	int ph = std::max(1, (f.height + block - 1) / block);

	bool rebuild = shield_tex_dirty
		|| block != shield_block
		|| shield_pw != pw || shield_ph != ph
		|| (int)shield_pixels.size() != pw * ph * 4;
	if (!rebuild) return;

	// Box-average each block into one RGBA texel. Averaging (rather than point
	// sampling) makes the downsampled image usable for both mosaic (nearest
	// upscale -> crisp blocks) and blur (linear upscale -> soft image).
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

	int effect = (int)OPT_GET(OPT_EFFECT)->GetInt();
	bool textured = effect != 0;
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
		GLint filter = effect == 1 ? GL_NEAREST : GL_LINEAR;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
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

	// Reveal rectangle outline + corner handles
	glColor3ub(40, 160, 255);
	glLineWidth(2.0f);
	glBegin(GL_LINE_LOOP);
	glVertex2d(rx, ry); glVertex2d(rr, ry); glVertex2d(rr, rb); glVertex2d(rx, rb);
	glEnd();

	const double hs = 4.0;
	double corners[4][2] = {{rx, ry}, {rr, ry}, {rr, rb}, {rx, rb}};
	glBegin(GL_QUADS);
	for (auto& c : corners) {
		glVertex2d(c[0] - hs, c[1] - hs); glVertex2d(c[0] + hs, c[1] - hs);
		glVertex2d(c[0] + hs, c[1] + hs); glVertex2d(c[0] - hs, c[1] + hs);
	}
	glEnd();
	glLineWidth(1.0f);
}

bool VideoDisplay::HandleSpoilerShieldMouse(wxMouseEvent &event) {
	if (!content_width || !content_height) return true;

	double sf = scale_factor ? scale_factor : 1.0;
	double cxl = content_left / sf;
	double cyl = content_top / sf;
	double cwl = content_width / sf;
	double chl = content_height / sf;
	if (cwl <= 0 || chl <= 0) return true;

	double px = (double)event.GetX();
	double py = (double)event.GetY();

	double rx = cxl + shield_rect_x * cwl;
	double ry = cyl + shield_rect_y * chl;
	double rr = rx + shield_rect_w * cwl;
	double rb = ry + shield_rect_h * chl;

	const double handle = 10.0;
	const double minw = 0.03, minh = 0.03;

	if (event.LeftDown()) {
		if (std::abs(px - rx) <= handle && std::abs(py - ry) <= handle)
			shield_drag = ShieldDrag::TL;
		else if (std::abs(px - rr) <= handle && std::abs(py - ry) <= handle)
			shield_drag = ShieldDrag::TR;
		else if (std::abs(px - rx) <= handle && std::abs(py - rb) <= handle)
			shield_drag = ShieldDrag::BL;
		else if (std::abs(px - rr) <= handle && std::abs(py - rb) <= handle)
			shield_drag = ShieldDrag::BR;
		else if (px >= rx && px <= rr && py >= ry && py <= rb) {
			shield_drag = ShieldDrag::Move;
			shield_drag_last = Vector2D(px, py);
		}
		else
			shield_drag = ShieldDrag::None;

		if (shield_drag != ShieldDrag::None && !HasCapture())
			CaptureMouse();
		return true;
	}

	if (event.Dragging() && event.LeftIsDown() && shield_drag != ShieldDrag::None) {
		if (shield_drag == ShieldDrag::Move) {
			double dx = (px - shield_drag_last.X()) / cwl;
			double dy = (py - shield_drag_last.Y()) / chl;
			shield_rect_x = std::clamp(shield_rect_x + dx, 0.0, 1.0 - shield_rect_w);
			shield_rect_y = std::clamp(shield_rect_y + dy, 0.0, 1.0 - shield_rect_h);
			shield_drag_last = Vector2D(px, py);
		}
		else {
			double nx = std::clamp((px - cxl) / cwl, 0.0, 1.0);
			double ny = std::clamp((py - cyl) / chl, 0.0, 1.0);
			double x1 = shield_rect_x, y1 = shield_rect_y;
			double x2 = shield_rect_x + shield_rect_w, y2 = shield_rect_y + shield_rect_h;
			switch (shield_drag) {
				case ShieldDrag::TL: x1 = std::min(nx, x2 - minw); y1 = std::min(ny, y2 - minh); break;
				case ShieldDrag::TR: x2 = std::max(nx, x1 + minw); y1 = std::min(ny, y2 - minh); break;
				case ShieldDrag::BL: x1 = std::min(nx, x2 - minw); y2 = std::max(ny, y1 + minh); break;
				case ShieldDrag::BR: x2 = std::max(nx, x1 + minw); y2 = std::max(ny, y1 + minh); break;
				default: break;
			}
			shield_rect_x = std::clamp(x1, 0.0, 1.0);
			shield_rect_y = std::clamp(y1, 0.0, 1.0);
			shield_rect_w = std::clamp(x2 - shield_rect_x, minw, 1.0 - shield_rect_x);
			shield_rect_h = std::clamp(y2 - shield_rect_y, minh, 1.0 - shield_rect_y);
		}
		Render();
		return true;
	}

	if (event.LeftUp()) {
		if (shield_drag != ShieldDrag::None) {
			shield_drag = ShieldDrag::None;
			if (HasCapture()) ReleaseMouse();
			PersistSpoilerShieldRect();
		}
		return true;
	}

	return true;
}
