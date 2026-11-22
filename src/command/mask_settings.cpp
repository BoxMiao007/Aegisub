// Copyright (c) 2026, BoxMiao007
// SPDX-License-Identifier: ISC

/// @file command/mask_settings.cpp
/// @brief Settings dialog shared by the spoiler shield and subtitle text mask
/// @ingroup command

#include "command.h"

#include "../compat.h"
#include "../format.h"
#include "../include/aegisub/context.h"
#include "../libresrc/libresrc.h"
#include "../options.h"
#include "../spoiler_shield_options.h"
#include "../video_display.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/clrpicker.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace {
using cmd::Command;

class MaskSettingsDialog final : public wxDialog {
	agi::Context *context;
	wxCheckBox *shield_enabled;
	wxChoice *shield_effect;
	wxColourPickerCtrl *shield_color;
	wxSlider *mosaic_block;
	wxStaticText *mosaic_label;
	wxCheckBox *mask_enabled;
	wxCheckBox *preview_mask;
	wxTextCtrl *replacement;

	void Apply() {
		OPT_SET(spoiler::Enabled)->SetBool(shield_enabled->GetValue());
		OPT_SET(spoiler::EffectKey)->SetInt(shield_effect->GetSelection() == 1 ? 1 : 0);
		OPT_SET(spoiler::Color)->SetColor(from_wx(shield_color->GetColour()));
		OPT_SET(spoiler::MosaicBlock)->SetInt(mosaic_block->GetValue());
		OPT_SET("Subtitle/Text Mask")->SetBool(mask_enabled->GetValue());
		OPT_SET("Subtitle/Text Mask Preview")->SetBool(preview_mask->GetValue());
		OPT_SET("Subtitle/Text Mask Replacement")->SetString(from_wx(replacement->GetValue()));
	}

	void Row(wxSizer *sizer, wxString const& label, wxWindow *control) {
		auto *row = new wxBoxSizer(wxHORIZONTAL);
		row->Add(new wxStaticText(this, wxID_ANY, label), wxSizerFlags().CenterVertical().Border(wxRIGHT, 8));
		row->Add(control, wxSizerFlags().CenterVertical());
		sizer->Add(row, wxSizerFlags().Border(wxALL, 4));
	}

public:
	MaskSettingsDialog(wxWindow *parent, agi::Context *c)
	: wxDialog(parent, wxID_ANY, _("Mask Settings")), context(c)
	{
		auto *root = new wxBoxSizer(wxVERTICAL);

		auto *shield = new wxStaticBoxSizer(wxVERTICAL, this, _("Spoiler Shield"));
		shield_enabled = new wxCheckBox(this, wxID_ANY, _("Enable spoiler shield"));
		shield_enabled->SetValue(OPT_GET(spoiler::Enabled)->GetBool());
		shield->Add(shield_enabled, wxSizerFlags().Border(wxALL, 4));

		shield_effect = new wxChoice(this, wxID_ANY);
		shield_effect->Append(_("Solid Colour"));
		shield_effect->Append(_("Mosaic"));
		shield_effect->SetSelection(OPT_GET(spoiler::EffectKey)->GetInt() == 1 ? 1 : 0);
		Row(shield, _("Hide effect"), shield_effect);

		shield_color = new wxColourPickerCtrl(this, wxID_ANY, to_wx(OPT_GET(spoiler::Color)->GetColor()));
		Row(shield, _("Fill colour"), shield_color);

		mosaic_block = new wxSlider(this, wxID_ANY, (int)OPT_GET(spoiler::MosaicBlock)->GetInt(), 32, 96, wxDefaultPosition, wxSize(220, -1));
		mosaic_label = new wxStaticText(this, wxID_ANY, fmt_wx("%d", mosaic_block->GetValue()));
		mosaic_block->Bind(wxEVT_SLIDER, [this](wxCommandEvent& e) { mosaic_label->SetLabel(fmt_wx("%d", e.GetInt())); });
		auto *block_row = new wxBoxSizer(wxHORIZONTAL);
		block_row->Add(new wxStaticText(this, wxID_ANY, _("Mosaic block size")), wxSizerFlags().CenterVertical().Border(wxRIGHT, 8));
		block_row->Add(mosaic_block, wxSizerFlags().CenterVertical());
		block_row->Add(mosaic_label, wxSizerFlags().CenterVertical().Border(wxLEFT, 8));
		shield->Add(block_row, wxSizerFlags().Border(wxALL, 4));

		auto *reset = new wxButton(this, wxID_ANY, _("Reset Reveal Area"));
		reset->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
			if (context && context->videoDisplay)
				context->videoDisplay->ResetSpoilerShieldRect();
		});
		shield->Add(reset, wxSizerFlags().Border(wxALL, 4));

		auto *text = new wxStaticBoxSizer(wxVERTICAL, this, _("Subtitle text mask"));
		mask_enabled = new wxCheckBox(this, wxID_ANY, _("Enable subtitle text mask"));
		mask_enabled->SetValue(OPT_GET("Subtitle/Text Mask")->GetBool());
		text->Add(mask_enabled, wxSizerFlags().Border(wxALL, 4));

		preview_mask = new wxCheckBox(this, wxID_ANY, _("Mask subtitles in video preview"));
		preview_mask->SetValue(OPT_GET("Subtitle/Text Mask Preview")->GetBool());
		text->Add(preview_mask, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 4));

		replacement = new wxTextCtrl(this, wxID_ANY, to_wx(OPT_GET("Subtitle/Text Mask Replacement")->GetString()), wxDefaultPosition, wxSize(160, -1));
		Row(text, _("Replacement character"), replacement);
		text->Add(new wxStaticText(this, wxID_ANY, _("Empty or invalid characters fall back to X")), wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 4));

		root->Add(shield, wxSizerFlags().Expand().Border(wxALL, 8));
		root->Add(text, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));

		auto *buttons = new wxBoxSizer(wxHORIZONTAL);
		buttons->AddStretchSpacer(1);
		buttons->Add(new wxButton(this, wxID_OK, _("OK")), wxSizerFlags().Border(wxRIGHT, 6));
		buttons->Add(new wxButton(this, wxID_APPLY, _("&Apply")), wxSizerFlags().Border(wxLEFT, 6));
		buttons->Add(new wxButton(this, wxID_CANCEL, _("Cancel")), wxSizerFlags().Border(wxLEFT, 6));
		root->Add(buttons, wxSizerFlags().Expand().Border(wxALL, 8));
		SetSizerAndFit(root);
		SetMinSize(wxSize(520, -1));
		CentreOnParent();

		Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Apply(); }, wxID_APPLY);
		Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Apply(); EndModal(wxID_OK); }, wxID_OK);
		Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_CANCEL); }, wxID_CANCEL);
	}
};

struct subtitle_mask_settings final : public Command {
	CMD_NAME("subtitle/mask_settings")
	CMD_ICON(options_button)
	STR_MENU("&Mask Settings...")
	STR_DISP("Mask Settings")
	STR_HELP("Configure the spoiler shield and the subtitle text mask")

	void operator()(agi::Context *c) override {
		MaskSettingsDialog dialog(c->parent, c);
		dialog.ShowModal();
	}
};
}

namespace cmd {
void init_mask_settings() {
	reg(std::make_unique<subtitle_mask_settings>());
}
}
