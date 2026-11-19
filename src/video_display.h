// Copyright (c) 2005-2010, Rodrigo Braz Monteiro
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

/// @file video_display.h
/// @see video_display.cpp
/// @ingroup video main_ui
///

#pragma once

#include <libaegisub/signal.h>

#include "spoiler_shield_geometry.h"
#include "vector2d.h"
#include "visual_tool_vector_clip.h"

#include <memory>
#include <typeinfo>
#include <vector>
#include <wx/cursor.h>
#include <wx/glcanvas.h>

// Prototypes
class VideoController;
class VideoOutGL;
class VisualToolBase;
class wxComboBox;
class wxTextCtrl;
class wxToolBar;
struct FrameReadyEvent;
struct VideoFrame;

namespace agi {
	struct Context;
	class OptionValue;
}

class VideoDisplay final : public wxGLCanvas {
	/// Signals the display is connected to
	std::vector<agi::signal::Connection> connections;

	const agi::OptionValue* autohideTools;

	agi::Context *con;

	std::unique_ptr<wxMenu> context_menu;

	/// The size in physical pixels of the ideal viewport at the current window zoom level.
	/// Includes any letter- or pillarboxing if applicable and is unaffected by content
	/// zoom level and panning.
	///
	/// This is usually equal to the client size (multiplied by @ref scale_factor), but
	/// the actual client size is controlled by window layout and may be smaller or larger
	/// than the ideal viewport size.
	///
	/// In free size mode, the window zoom level is adjusted when the client size changes,
	/// so the viewport size should always be the same as the client size.
	///
	/// Most code refers to the ideal viewport as simply "the viewport" and
	/// "client size" is used to refer to the actual size of the canvas.
	wxSize viewportSize;

	Vector2D last_mouse_pos, mouse_pos;

	/// Distance rightward from the left edge of the viewport to the left edge of the video in physical (screen) pixels
	int content_left = 0;
	/// The width in physical (screen) pixels that the video would occupy after scaling, ignoring viewport cropping
	int content_width = 0;
	/// Distance upward from the bottom edge of the viewport to the bottom edge of the video in physical pixels; passed to @ref VideoOutGL::Render
	int content_bottom = 0;
	/// Distance downward from the top edge of the viewport to the top edge of the video in physical (screen) pixels
	int content_top = 0;
	/// The height in physical (screen) pixels that the video would occupy after scaling, ignoring viewport cropping
	int content_height = 0;

	/// The current window zoom level, that is, the ratio of the viewport size to the original video resolution.
	double windowZoomValue;

	/// The zoom level of the video inside the viewport.
	double contentZoomValue = 1;

	bool isZoomGestureActive = false;
	double contentZoomAtGestureStart = 1;
	Vector2D zoomGestureAnchorPoint = {0, 0};

	/// The video pan, in physical pixels
	/// @see viewportSize
	double pan_x = 0;
	double pan_y = 0;

	/// The video renderer
	std::unique_ptr<VideoOutGL> videoOut;
	/// Suspend drawing after a GL failure until another video is loaded.
	bool render_failed = false;

	// --- Spoiler shield: occlude the video (and baked-in subtitles) except a reveal rectangle ---
	/// Downsampled RGBA image of the current frame used for mosaic covering
	std::vector<unsigned char> shield_pixels;
	int shield_pw = 0;
	int shield_ph = 0;
	/// Power-of-two texture size the downsampled image is padded into
	int shield_tex_w = 0;
	int shield_tex_h = 0;
	int shield_block = 0;
	bool shield_tex_dirty = true;
	/// A texture upload failed for the current frame; retried on the next frame
	bool shield_tex_failed = false;
	unsigned int shield_tex = 0;
	/// Source frame kept so the shield texture can be rebuilt when the effect changes
	std::shared_ptr<VideoFrame> shield_src;
	/// Reveal rectangle in normalized video coordinates (0..1)
	spoiler::Rect shield_rect;
	/// Current drag interaction on the reveal rectangle: borders, corners or moving
	spoiler::Grab shield_drag = spoiler::Grab::Outside;
	Vector2D shield_drag_last;

	/// The active visual typesetting tool
	std::unique_ptr<VisualToolBase> tool;
	/// The toolbar used by individual typesetting tools
	wxToolBar* toolBar;

	/// The OpenGL context for this display
	std::unique_ptr<wxGLContext> glContext;

	/// The dropdown box for selecting window zoom levels
	wxComboBox *zoomBox;

	/// Whether the display can be freely resized by the user
	bool freeSize;

	/// Frame which will replace the currently visible frame on the next render
	std::shared_ptr<VideoFrame> pending_frame;

	int scale_factor;

	/// @brief Draw an overscan mask
	/// @param horizontal_percent The percent of the video reserved horizontally
	/// @param vertical_percent The percent of the video reserved vertically
	void DrawOverscanMask(float horizontal_percent, float vertical_percent) const;

	// --- Spoiler shield helpers ---
	/// (Re)load the reveal rectangle from the config
	void ReloadSpoilerShieldRect();
	/// Persist the reveal rectangle to the config
	void PersistSpoilerShieldRect();
	/// Draw the shield over the video, leaving only the reveal rectangle visible
	void RenderSpoilerShield();
	/// Handle mouse input for the reveal rectangle
	void HandleSpoilerShieldMouse(wxMouseEvent &event);
	/// Video content rectangle in window (logical pixel) coordinates
	bool ShieldVideoWindow(spoiler::WindowRect *rect) const;
	/// Reveal rectangle in window (logical pixel) coordinates; false if there is no video
	bool ShieldRectWindow(spoiler::WindowRect *rect) const;
	/// Which part of the reveal rectangle the point grabs for dragging
	spoiler::Grab ShieldHitTest(double px, double py) const;
	/// Match the mouse cursor to the current grab target under the pointer
	void UpdateShieldCursor(double px, double py);
	/// Cancel an in-flight drag, releasing mouse capture when one was active
	void CancelSpoilerShieldDrag(bool persist = true);
	/// Pause or resume visual tool interaction as the shield toggles
	void UpdateSpoilerShieldState();
	/// Rebuild the downsampled shield texture if the frame or strength changed
	void RebuildShieldTextureIfNeeded();

	/// Upload the image for the current frame to the video card
	void UploadFrameData(FrameReadyEvent&);

	/// @brief Initialize the gl context and set the active context to this one
	/// @return Could the context be set?
	bool InitContext();

	/// @brief Recompute the size of the viewport based on the current window zoom and video resolution,
	///        then resize the client area to match the viewport
	void FitClientSizeToVideo();
	/// @brief Set the viewport size to @p newSize and rescale the pan values
	///
	/// You should call @ref PositionVideo() after this.
	///
	/// In free size mode, the @p newSize argument is ignored and client size is used instead.
	///
	/// @param rescalePan Should the current pan be rescaled for the new viewport?
	/// @param newSize The new size, ignored in free size mode
	void UpdateViewportSize(bool rescalePan, wxSize newSize = wxDefaultSize);
	/// @brief Update content size and position based on the current viewport size, content zoom and pan
	///
	/// Updates @ref content_left, @ref content_width, @ref content_bottom, @ref content_top and @ref content_height
	///
	/// @param preserveContentSize Should content zoom be adjusted to maintain current content size?
	void PositionVideo(bool preserveContentSize = false);
	/// Set the window zoom level to that indicated by the dropdown
	void SetWindowZoomFromBox(wxCommandEvent&);
	/// Set the window zoom level to that indicated by the text
	void SetWindowZoomFromBoxText(wxCommandEvent&);

	/// @brief Key event handler
	void OnKeyDown(wxKeyEvent &event);
	/// @brief Mouse event handler
	void OnMouseEvent(wxMouseEvent& event);
	void OnMouseWheel(wxMouseEvent& event);
	void OnMouseLeave(wxMouseEvent& event);
	void OnGestureZoom(wxZoomGestureEvent& event);
	/// @brief Recalculate video positioning and scaling when the available area or zoom changes
	void OnSizeEvent(wxSizeEvent &event);
	void OnContextMenu(wxContextMenuEvent&);

	/// @brief Pan the video by delta
	/// @param delta Delta in logical pixels
	void Pan(Vector2D delta);

	/// @brief Transforms a client position into a zoom anchor point
	///
	/// The anchor point is a position in the video frame that should always be anchored to the center
	/// of the zoom gesture. This function returns the anchor point in a video-relative coordinate system,
	/// which ensures we can accurately track the anchor point as the video is zoomed and panned.
	///
	/// @param position Client position in logical pixels
	/// @return An anchor point that can be used with @ref ZoomAndPan()
	Vector2D GetZoomAnchorPoint(wxPoint position);

	/// @brief Zooms and pans the video using an anchor point
	///
	/// To use this function, you need to first obtain an anchor point with @ref GetZoomAnchorPoint().
	///
	/// If @p newPosition is equal to the current position of the anchor point, the video will be zoomed
	/// with the anchor point as the fixed point of the scaling transformation.
	///
	/// If @p newPosition differs from the current position of the anchor point, the video will additionally
	/// be panned to move the anchor point to the new position.
	///
	/// @param newZoomValue The new zoom value
	/// @param anchorPoint An anchor point obtained from @ref GetZoomAnchorPoint()
	/// @param newPosition New client position of the anchor point in logical pixels
	void ZoomAndPan(double newZoomValue, Vector2D anchorPoint, wxPoint newPosition);

public:
	/// @brief Constructor
	VideoDisplay(
		wxToolBar *visualSubToolBar,
		bool isDetached,
		wxComboBox *zoomBox,
		wxWindow* parent,
		agi::Context *context);
	~VideoDisplay();

	/// @brief Render the currently visible frame
	void Render();

	/// @brief Set the window zoom level
	/// @param value The new zoom level
	void SetWindowZoom(double value);
	/// @brief Get the current window zoom level
	double GetWindowZoom() const { return windowZoomValue; }

	/// @brief Reset content zoom and pan
	void ResetContentZoom();
	bool IsContentZoomActive() { return contentZoomValue != 1 || pan_x != 0 || pan_y != 0; }

	/// Whether the spoiler shield is currently enabled
	bool SpoilerShieldOn() const;

	/// Get the last seen position of the mouse in script coordinates
	Vector2D GetMousePosition() const;

	void SetTool(std::unique_ptr<VisualToolBase> new_tool);

	/// @brief Reset the spoiler shield reveal rectangle to its default position and persist it
	void ResetSpoilerShieldRect();

	void SetSubTool(int subtool) const { if (!SpoilerShieldOn() && tool) tool->SetSubTool(subtool); };

	bool ToolIsType(std::type_info const& type) const;

	int GetSubTool() const { return tool ? tool->GetSubTool() : 0; }

	/// Discard all OpenGL state
	void Unload();
};
