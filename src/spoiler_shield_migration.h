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

/// @file spoiler_shield_migration.h
/// @brief One-time migration of legacy spoiler shield configuration
/// @ingroup video main_ui
///
/// The shield's options started out as "Video/Draft Cover/*" and the blur
/// effect (Effect = 2) was later removed. Those old keys no longer exist in the
/// defaults, so they would be silently dropped while loading the user profile.
/// This migration maps the surviving keys to their current names before that
/// happens, so an existing install keeps its settings.

#pragma once

#include "spoiler_shield_geometry.h"

#include <libaegisub/option.h>
#include <libaegisub/option_value.h>

#include <memory>
#include <string>
#include <vector>

namespace spoiler {

namespace detail {
inline agi::OptionValue *Find(std::vector<std::unique_ptr<agi::OptionValue>> &values, std::string const &name) {
	for (auto &value : values)
		if (value->GetName() == name)
			return value.get();
	return nullptr;
}
}

/// Migrate legacy "Video/Draft Cover/*" values to "Video/Spoiler Shield/*".
///
/// Operates on the values read from the user profile, before the merge with the
/// defaults. Options the user already stores under the new names win; the old
/// values only fill in keys the new profile does not set. The removed
/// "Blur Strength" option has no current equivalent and is not copied. Any
/// value that is not a real effect (including the old blur mode 2) becomes the
/// solid fill, and a malformed reveal rectangle is dropped so the default is
/// used. Running it more than once is a no-op.
inline void MigrateLegacyOptions(std::vector<std::unique_ptr<agi::OptionValue>> &values) {
	auto present = [&](std::string const &name) {
		return detail::Find(values, name) != nullptr;
	};

	struct ScalarRename {
		char const *legacy;
		char const *current;
		agi::OptionType type;
	};

	static constexpr ScalarRename scalar_renames[] = {
		{"Video/Draft Cover/Enabled", "Video/Spoiler Shield/Enabled", agi::OptionType::Bool},
		{"Video/Draft Cover/Color", "Video/Spoiler Shield/Color", agi::OptionType::Color},
		{"Video/Draft Cover/Mosaic Block", "Video/Spoiler Shield/Mosaic Block", agi::OptionType::Int},
	};

	for (auto const &rename : scalar_renames) {
		if (present(rename.current))
			continue;

		agi::OptionValue *legacy = detail::Find(values, rename.legacy);
		if (!legacy || legacy->GetType() != rename.type)
			continue;

		switch (rename.type) {
			case agi::OptionType::Bool:
				values.push_back(std::make_unique<agi::OptionValueBool>(rename.current, legacy->GetBool()));
				break;
			case agi::OptionType::Color:
				values.push_back(std::make_unique<agi::OptionValueColor>(rename.current, legacy->GetColor()));
				break;
			case agi::OptionType::Int:
				values.push_back(std::make_unique<agi::OptionValueInt>(rename.current, legacy->GetInt()));
				break;
			default:
				break;
		}
	}

	// Effect: only solid (0) and mosaic (1) remain. Blur (2) and any other value
	// would leave the shield in an unusable state, so they fall back to solid.
	if (!present("Video/Spoiler Shield/Effect")) {
		if (agi::OptionValue *legacy = detail::Find(values, "Video/Draft Cover/Effect")) {
			if (legacy->GetType() == agi::OptionType::Int) {
				int64_t effect = legacy->GetInt();
				if (effect != 0 && effect != 1)
					effect = 0;
				values.push_back(std::make_unique<agi::OptionValueInt>("Video/Spoiler Shield/Effect", effect));
			}
		}
	}

	// Reveal rectangle: copy only when the legacy string still parses as a valid
	// rectangle, otherwise keep the current default rather than persisting
	// garbage that the display would have to reject again later.
	if (!present("Video/Spoiler Shield/Reveal Rect")) {
		if (agi::OptionValue *legacy = detail::Find(values, "Video/Draft Cover/Reveal Rect")) {
			if (legacy->GetType() == agi::OptionType::String) {
				Rect rect;
				if (ParseRect(legacy->GetString(), rect))
					values.push_back(std::make_unique<agi::OptionValueString>("Video/Spoiler Shield/Reveal Rect", legacy->GetString()));
			}
		}
	}
}

/// Install the legacy-option migration for the profile loaded by @p options.
/// Must be called before ConfigUser().
inline void RegisterOptionsMigration(agi::Options &options) {
	options.SetConfigMigrator(&MigrateLegacyOptions);
}

} // namespace spoiler
