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
#include "spoiler_shield_geometry.h"
#include "spoiler_shield_options.h"

#include "video_frame.h"
#include "compat.h"
#include "options.h"

#include <libaegisub/color.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#ifdef HAVE_OPENGL_GL_H
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

bool VideoDisplay::SpoilerShieldOn() const {
	return OPT_GET(spoiler::Enabled)->GetBool();
}

void VideoDisplay::ReloadSpoilerShieldRect() {
	// Effect decoding and legacy migration live elsewhere: GetEffect() maps an
	// unknown or removed mode to Solid, and startup migration rewrites old keys.
	spoiler::Rect rect;
	if (spoiler::ParseRect(OPT_GET(spoiler::RevealRect)->GetString(), rect))
		shield_rect = rect;
	else
		shield_rect = spoiler::Rect{};
}

void VideoDisplay::PersistSpoilerShieldRect() {
	OPT_SET(spoiler::RevealRect)->SetString(spoiler::FormatRect(shield_rect));
}

void VideoDisplay::ResetSpoilerShieldRect() {
	CancelSpoilerShieldDrag(false);
	shield_rect = spoiler::Rect{};
	PersistSpoilerShieldRect();
}

void VideoDisplay::CancelSpoilerShieldDrag(bool persist) {
	bool active = shield_drag != spoiler::Grab::Outside;
	shield_drag = spoiler::Grab::Outside;
	if (!active) return;
	// Capture may already be gone if the window lost focus mid-drag.
	if (HasCapture()) ReleaseMouse();
	if (persist) PersistSpoilerShieldRect();
}

void VideoDisplay::RebuildShieldTextureIfNeeded() {
	if (!shield_src) return;

	// Only the mosaic needs a texture; the solid fill is drawn in flat colour.
	if (spoiler::GetEffect() != spoiler::Effect::Mosaic) {
		shield_tex_failed = false;
		return;
	}

	int block = std::clamp((int)OPT_GET(spoiler::MosaicBlock)->GetInt(), 32, 128);

	VideoFrame const& f = *shield_src;
	if (f.width <= 0 || f.height <= 0 || f.pitch <= 0) return;

	// A newly uploaded frame clears a previous failure so it is retried; between
	// frames the failure stays latched so a broken driver is not hammered.
	if (shield_tex_dirty)
		shield_tex_failed = false;
	if (shield_tex_failed)
		return;

	// Clear any stale error first so our glGetError() probes below only see
	// failures from this shield's own texture work.
	while (glGetError() != GL_NO_ERROR) {}

	GLint max_tex = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex);

	// OpenGL 1.1 (including the RDP emulation Aegisub supports) only guarantees
	// power-of-two textures, so the downsampled image is padded up to POT sizes.
	// A zero allocation means the frame is larger than the driver can texture;
	// fail closed by dropping the texture so the shield falls back to solid.
	spoiler::TextureAxis ax = spoiler::MosaicAxis(f.width, block, max_tex);
	spoiler::TextureAxis ay = spoiler::MosaicAxis(f.height, block, max_tex);

	auto drop_texture = [this]() {
		if (shield_tex) glDeleteTextures(1, &shield_tex);
		shield_tex = 0;
		shield_pw = shield_ph = 0;
		shield_tex_w = shield_tex_h = 0;
		shield_tex_failed = true;
		shield_tex_dirty = false;
		while (glGetError() != GL_NO_ERROR) {}
	};

	if (!ax.allocated || !ay.allocated) {
		drop_texture();
		return;
	}

	int pw = ax.used;
	int ph = ay.used;
	int tex_w = ax.allocated;
	int tex_h = ay.allocated;

	bool rebuild = shield_tex_dirty
		|| block != shield_block
		|| shield_pw != pw || shield_ph != ph
		|| shield_tex_w != tex_w || shield_tex_h != tex_h
		|| !shield_tex;
	if (!rebuild) return;

	// Box-average each block into one RGBA texel and upscale it with nearest
	// neighbour filtering, so every block becomes one crisp colour square.
	// Averaging keeps a block's colour stable while the frame changes.
	// The padded region outside pw x ph is never sampled, so it stays zero.
	shield_pixels.assign((size_t)tex_w * tex_h * 4, 0);
	for (int oy = 0; oy < ph; ++oy) {
		int py0 = oy * block;
		int py1 = std::min(f.height, py0 + block);
		for (int ox = 0; ox < pw; ++ox) {
			int x0 = ox * block;
			int x1 = std::min(f.width, x0 + block);
			int64_t sb = 0, sg = 0, sr = 0, n = 0;
			// 播放时每帧都重算；按块内 4x4 网格抽样即可得到稳定的马赛克色，
			// 避免对 1080p 每帧遍历整幅画面造成播放卡顿。
			int step_x = std::max(1, (x1 - x0) / 4);
			int step_y = std::max(1, (py1 - py0) / 4);
			for (int py = py0; py < py1; py += step_y) {
				int row = f.flipped ? (f.height - 1 - py) : py;
				if (row < 0 || row >= f.height) continue;
				unsigned char const *p = f.data.data()
					+ (size_t)row * f.pitch + (size_t)x0 * 4;
				for (int x = x0; x < x1; x += step_x, p += (size_t)step_x * 4) {
					sb += p[0]; sg += p[1]; sr += p[2]; ++n;
				}
			}
			unsigned char *d = shield_pixels.data() + ((size_t)oy * tex_w + ox) * 4;
			if (n) {
				d[0] = (unsigned char)(sr / n);
				d[1] = (unsigned char)(sg / n);
				d[2] = (unsigned char)(sb / n);
				d[3] = 255;
			}
		}
	}

	// Drop any stale error so this shield work never poisons VideoOutGL's own
	// CHECK_ERROR, which would otherwise treat it as a fatal render failure.
	while (glGetError() != GL_NO_ERROR) {}

	if (!shield_tex) glGenTextures(1, &shield_tex);
	if (!shield_tex) {
		drop_texture();
		return;
	}

	glBindTexture(GL_TEXTURE_2D, shield_tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_w, tex_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, shield_pixels.data());

	if (glGetError() != GL_NO_ERROR) {
		drop_texture();
		return;
	}

	shield_pw = pw;
	shield_ph = ph;
	shield_tex_w = tex_w;
	shield_tex_h = tex_h;
	shield_block = block;
	shield_tex_dirty = false;
}

void VideoDisplay::RenderSpoilerShield() {
	spoiler::WindowRect video;
	if (!ShieldVideoWindow(&video)) return;

	double cxl = video.left;
	double cyl = video.top;
	double cwl = video.Width();
	double chl = video.Height();
	double cright = video.right;
	double cbottom = video.bottom;

	spoiler::WindowRect reveal = spoiler::ToWindow(shield_rect, video);

	struct Rect { double x1, y1, x2, y2; };
	// Cover everything inside the video content rectangle except the reveal rect
	Rect rects[4] = {
		{ cxl, cyl, cright, reveal.top },                              // above
		{ cxl, reveal.bottom, cright, cbottom },                       // below
		{ cxl, std::max(cyl, reveal.top), reveal.left, std::min(cbottom, reveal.bottom) },   // left
		{ reveal.right, std::max(cyl, reveal.top), cright, std::min(cbottom, reveal.bottom) }, // right
	};

	bool textured = spoiler::GetEffect() == spoiler::Effect::Mosaic;
	if (textured) {
		RebuildShieldTextureIfNeeded();
		textured = shield_tex != 0 && shield_tex_w > 0 && shield_tex_h > 0;
	}

	glDisable(GL_BLEND);

	if (!textured) {
		// Fail closed: when the mosaic texture is unavailable, hide the frame with
		// the opaque solid fill instead of exposing the picture we must conceal.
		agi::Color col = OPT_GET(spoiler::Color)->GetColor();
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
		// The texture is padded to power-of-two sizes; scale UVs so only the used
		// pw x ph region is sampled.
		double ua = (double)shield_pw / shield_tex_w;
		double va = (double)shield_ph / shield_tex_h;

		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, shield_tex);
		glColor3ub(255, 255, 255);
		glBegin(GL_QUADS);
		for (auto& r : rects) {
			if (r.x2 <= r.x1 || r.y2 <= r.y1) continue;
			double u1 = (r.x1 - cxl) / cwl * ua, u2 = (r.x2 - cxl) / cwl * ua;
			double v1 = (r.y1 - cyl) / chl * va, v2 = (r.y2 - cyl) / chl * va;
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
	glVertex2d(reveal.left, reveal.top);
	glVertex2d(reveal.right, reveal.top);
	glVertex2d(reveal.right, reveal.bottom);
	glVertex2d(reveal.left, reveal.bottom);
	glEnd();

	const double hs = 4.0;
	double mx = (reveal.left + reveal.right) * 0.5;
	double my = (reveal.top + reveal.bottom) * 0.5;
	double handles[8][2] = {
		{reveal.left, reveal.top}, {reveal.right, reveal.top},
		{reveal.right, reveal.bottom}, {reveal.left, reveal.bottom},
		{mx, reveal.top}, {reveal.right, my}, {mx, reveal.bottom}, {reveal.left, my}
	};
	glBegin(GL_QUADS);
	for (auto& h : handles) {
		glVertex2d(h[0] - hs, h[1] - hs); glVertex2d(h[0] + hs, h[1] - hs);
		glVertex2d(h[0] + hs, h[1] + hs); glVertex2d(h[0] - hs, h[1] + hs);
	}
	glEnd();
	glLineWidth(1.0f);

	// The shield draw must not leave an error behind: VideoOutGL's own
	// CHECK_ERROR would attribute it to the next frame's first GL call and
	// abort video rendering even though the shield itself is only cosmetic.
	while (glGetError() != GL_NO_ERROR) {}
}

bool VideoDisplay::ShieldVideoWindow(spoiler::WindowRect *rect) const {
	if (!content_width || !content_height) return false;

	double sf = scale_factor ? scale_factor : 1.0;
	double cwl = content_width / sf;
	double chl = content_height / sf;
	if (cwl <= 0 || chl <= 0) return false;

	double left = content_left / sf;
	double top = content_top / sf;
	*rect = {left, top, left + cwl, top + chl};
	return true;
}

bool VideoDisplay::ShieldRectWindow(spoiler::WindowRect *rect) const {
	spoiler::WindowRect video;
	if (!ShieldVideoWindow(&video)) return false;
	*rect = spoiler::ToWindow(shield_rect, video);
	return true;
}

spoiler::Grab VideoDisplay::ShieldHitTest(double px, double py) const {
	spoiler::WindowRect window;
	if (!ShieldRectWindow(&window)) return spoiler::Grab::Outside;
	return spoiler::HitTest(window, px, py);
}

void VideoDisplay::UpdateShieldCursor(double px, double py) {
	spoiler::Grab grab = ShieldHitTest(px, py);
	if (grab == spoiler::Grab::Outside) {
		SetCursor(wxCursor(wxCURSOR_ARROW));
		return;
	}
	if (grab == spoiler::Grab::Move) {
		SetCursor(wxCursor(wxCURSOR_SIZING));
		return;
	}

	bool left = spoiler::HasEdge(grab, spoiler::Grab::Left);
	bool right = spoiler::HasEdge(grab, spoiler::Grab::Right);
	bool top = spoiler::HasEdge(grab, spoiler::Grab::Top);
	bool bottom = spoiler::HasEdge(grab, spoiler::Grab::Bottom);

	if ((left && top) || (right && bottom))
		SetCursor(wxCursor(wxCURSOR_SIZENWSE));
	else if ((right && top) || (left && bottom))
		SetCursor(wxCursor(wxCURSOR_SIZENESW));
	else if (left || right)
		SetCursor(wxCursor(wxCURSOR_SIZEWE));
	else
		SetCursor(wxCursor(wxCURSOR_SIZENS));
}

void VideoDisplay::HandleSpoilerShieldMouse(wxMouseEvent &event) {
	spoiler::WindowRect window;
	if (!ShieldRectWindow(&window)) return;

	double px = (double)event.GetX();
	double py = (double)event.GetY();

	if (event.LeftDown()) {
		shield_drag = ShieldHitTest(px, py);
		if (shield_drag == spoiler::Grab::Move)
			shield_drag_last = Vector2D(px, py);
		if (shield_drag != spoiler::Grab::Outside && !HasCapture())
			CaptureMouse();
		UpdateShieldCursor(px, py);
		return;
	}

	if (event.Dragging() && event.LeftIsDown() && shield_drag != spoiler::Grab::Outside) {
		// Both moving and resizing are expressed relative to the video area. Using
		// the reveal rectangle here would feed its own previous size back into the
		// calculation and make the box jump around while dragging.
		spoiler::WindowRect video;
		if (!ShieldVideoWindow(&video)) return;

		if (shield_drag == spoiler::Grab::Move) {
			spoiler::Move(shield_rect,
				(px - shield_drag_last.X()) / video.Width(),
				(py - shield_drag_last.Y()) / video.Height());
			shield_drag_last = Vector2D(px, py);
		}
		else {
			spoiler::Resize(shield_rect, shield_drag, video, px, py);
		}
		Render();
		return;
	}

	if (event.LeftUp()) {
		if (shield_drag != spoiler::Grab::Outside) {
			shield_drag = spoiler::Grab::Outside;
			if (HasCapture()) ReleaseMouse();
			PersistSpoilerShieldRect();
			UpdateShieldCursor(px, py);
		}
		return;
	}

	// Hovering a border or a handle previews the resize direction in the cursor.
	UpdateShieldCursor(px, py);
}
