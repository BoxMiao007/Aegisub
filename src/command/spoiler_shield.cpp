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

/// @file command/spoiler_shield.cpp
/// @brief Commands for the spoiler shield
/// @ingroup command

#include "command.h"

#include "../compat.h"
#include "../format.h"
#include "../include/aegisub/context.h"
#include "../libresrc/libresrc.h"
#include "../options.h"
#include "../project.h"
#include "../spoiler_shield_options.h"
#include "../video_display.h"

#include <wx/colordlg.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/stattext.h>

namespace {
using cmd::Command;

/// Commands that are only useful with a video loaded
struct spoiler_shield_command : public Command {
	CMD_TYPE(COMMAND_VALIDATE)
	bool Validate(const agi::Context *c) override {
		return !!c->project->VideoProvider();
	}
};

class SpoilerShieldStrengthDialog final : public wxDialog {
	wxSlider *slider;
public:
	SpoilerShieldStrengthDialog(wxWindow *parent, wxString const& title, int value, int min, int max)
	: wxDialog(parent, wxID_ANY, title)
	{
		auto *sizer = new wxBoxSizer(wxVERTICAL);
		auto *label = new wxStaticText(this, wxID_ANY, fmt_wx("%d", value));
		slider = new wxSlider(this, wxID_ANY, value, min, max, wxDefaultPosition, wxSize(280, -1));
		slider->Bind(wxEVT_SLIDER, [label](wxCommandEvent& e) { label->SetLabel(fmt_wx("%d", e.GetInt())); });
		sizer->Add(label, wxSizerFlags().Expand().Border(wxALL, 8));
		sizer->Add(slider, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		sizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));
		SetSizerAndFit(sizer);
		CentreOnParent();
	}

	int GetStrength() const { return slider->GetValue(); }
};

struct video_spoiler_shield final : public spoiler_shield_command {
	CMD_NAME("video/spoiler_shield")
	CMD_ICON(spoiler_shield)
	STR_MENU("&Spoiler Shield")
	STR_DISP("Spoiler Shield")
	STR_HELP("Cover the video with a blocking layer, showing only an adjustable window so you can work without watching the picture")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_TOGGLE)

	bool IsActive(const agi::Context *) override {
		return OPT_GET(spoiler::Enabled)->GetBool();
	}
	void operator()(agi::Context *) override {
		OPT_SET(spoiler::Enabled)->SetBool(!OPT_GET(spoiler::Enabled)->GetBool());
	}
};

struct video_spoiler_shield_solid final : public spoiler_shield_command {
	CMD_NAME("video/spoiler_shield/effect/solid")
	STR_MENU("&Solid Colour")
	STR_DISP("Solid Colour")
	STR_HELP("Fill the hidden area with a solid colour")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)
	bool IsActive(const agi::Context *) override { return spoiler::GetEffect() == spoiler::Effect::Solid; }
	void operator()(agi::Context *) override { spoiler::SetEffect(spoiler::Effect::Solid); }
};

struct video_spoiler_shield_mosaic final : public spoiler_shield_command {
	CMD_NAME("video/spoiler_shield/effect/mosaic")
	STR_MENU("&Mosaic")
	STR_DISP("Mosaic")
	STR_HELP("Pixelate the hidden area (block size is adjustable)")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)
	bool IsActive(const agi::Context *) override { return spoiler::GetEffect() == spoiler::Effect::Mosaic; }
	void operator()(agi::Context *) override { spoiler::SetEffect(spoiler::Effect::Mosaic); }
};

struct video_spoiler_shield_color final : public spoiler_shield_command {
	CMD_NAME("video/spoiler_shield/color")
	STR_MENU("&Colour...")
	STR_DISP("Colour...")
	STR_HELP("Choose the colour used by the solid fill")
	CMD_TYPE(COMMAND_VALIDATE)
	void operator()(agi::Context *c) override {
		wxColourData data;
		data.SetColour(to_wx(OPT_GET(spoiler::Color)->GetColor()));
		wxColourDialog dialog(c->parent, &data);
		if (dialog.ShowModal() == wxID_OK)
			OPT_SET(spoiler::Color)->SetColor(from_wx(dialog.GetColourData().GetColour()));
	}
};

struct video_spoiler_shield_strength final : public spoiler_shield_command {
	CMD_NAME("video/spoiler_shield/strength")
	STR_MENU("Mosaic &Strength...")
	STR_DISP("Mosaic Strength...")
	STR_HELP("Adjust the mosaic block size used to hide the area")
	CMD_TYPE(COMMAND_VALIDATE)

	bool Validate(const agi::Context *c) override {
		return spoiler_shield_command::Validate(c) && spoiler::GetEffect() == spoiler::Effect::Mosaic;
	}
	void operator()(agi::Context *c) override {
		SpoilerShieldStrengthDialog dialog(c->parent, _("Mosaic Strength"), (int)OPT_GET(spoiler::MosaicBlock)->GetInt(), 2, 96);
		if (dialog.ShowModal() == wxID_OK)
			OPT_SET(spoiler::MosaicBlock)->SetInt(dialog.GetStrength());
	}
};

struct video_spoiler_shield_reset final : public spoiler_shield_command {
	CMD_NAME("video/spoiler_shield/reset")
	STR_MENU("&Reset Reveal Area")
	STR_DISP("Reset Reveal Area")
	STR_HELP("Reset the reveal area to its default position and size")
	CMD_TYPE(COMMAND_VALIDATE)
	void operator()(agi::Context *c) override {
		c->videoDisplay->ResetSpoilerShieldRect();
	}
};
}

namespace cmd {
void init_spoiler_shield() {
	reg(std::make_unique<video_spoiler_shield>());
	reg(std::make_unique<video_spoiler_shield_solid>());
	reg(std::make_unique<video_spoiler_shield_mosaic>());
	reg(std::make_unique<video_spoiler_shield_color>());
	reg(std::make_unique<video_spoiler_shield_strength>());
	reg(std::make_unique<video_spoiler_shield_reset>());
}
}
