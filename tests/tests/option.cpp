// Copyright (c) 2011, Thomas Goyne <plorkyeran@aegisub.org>
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

#include <libaegisub/fs.h>
#include <libaegisub/option.h>
#include <libaegisub/option_value.h>

#include <main.h>
#include <util.h>

#include "../../src/spoiler_shield_migration.h"

#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

static const char default_opt[] = "{\"Valid\" : \"This is valid\"}";
static const char all_types[] = R"raw({
	"Integer" : 0,
	"Double" : 0.1,
	"String" : "",
	"Color" : "rgb(0, 0, 0)",
	"Boolean" : false,
	"Array" : {
		"Integer" : [{ "int" : 0 }, {"int" : 0}],
		"Double" : [{ "double" : 0.1 }, {"double" : 0.1}],
		"String" : [{ "string" : "" }, {"string" : ""}],
		"Color" : [{ "color" : "rgb(0,0,0)" }, {"color" : "rgb(0,0,0)"}],
		"Boolean" : [{ "bool" : false }, {"bool" : false}]
	}
})raw";

class lagi_option : public libagi {
protected:
	agi::fs::path conf_ok;

	void SetUp() override {
		conf_ok = util::test_data_dir() / "options/string.json";
	}
};

TEST_F(lagi_option, construct_from_file) {
	EXPECT_NO_THROW(agi::Options(conf_ok, default_opt, agi::Options::FLUSH_SKIP));
}

TEST_F(lagi_option, nonexistent_file) {
	EXPECT_NO_THROW(agi::Options("", default_opt, agi::Options::FLUSH_SKIP));
}

TEST_F(lagi_option, get_existing_option) {
	agi::Options opt(conf_ok, default_opt, agi::Options::FLUSH_SKIP);
	ASSERT_NO_THROW(opt.Get("Valid"));
	ASSERT_NO_THROW(opt.Get("Valid")->GetString());
	EXPECT_STREQ("This is valid", opt.Get("Valid")->GetString().c_str());
}

TEST_F(lagi_option, get_nonexistant_option) {
	agi::Options opt(conf_ok, default_opt, agi::Options::FLUSH_SKIP);
	EXPECT_THROW(opt.Get("Nonexistent option"), agi::InternalError);
}

TEST_F(lagi_option, flush_skip) {
	agi::fs::Copy(util::test_data_dir() / "options/string.json", "data/options/tmp");
	{
		agi::Options opt("data/options/tmp", default_opt, agi::Options::FLUSH_SKIP);
		ASSERT_NO_THROW(opt.Get("Valid")->SetString(""));
	}
	EXPECT_TRUE(util::compare(util::test_data_dir() / "options/string.json", "data/options/tmp"));
}

TEST_F(lagi_option, flush_no_skip) {
	agi::fs::Copy(util::test_data_dir() / "options/string.json", "data/options/tmp");
	{
		agi::Options opt("data/options/tmp", default_opt);
		ASSERT_NO_THROW(opt.Get("Valid")->SetString(""));
	}
	EXPECT_FALSE(util::compare(util::test_data_dir() / "options/string.json", "data/options/tmp"));
}

TEST_F(lagi_option, existent_but_invalid_file_uses_default) {
	agi::Options opt(util::test_data_dir() / "options/null.json", default_opt, agi::Options::FLUSH_SKIP);
	EXPECT_NO_THROW(opt.Get("Valid")->GetString());
	EXPECT_THROW(opt.Get("Null"), agi::Exception);
}

TEST_F(lagi_option, arrays) {
	agi::Options opt("", all_types, agi::Options::FLUSH_SKIP);

	EXPECT_NO_THROW(opt.Get("Array/String")->GetListString());
	EXPECT_NO_THROW(opt.Get("Array/Integer")->GetListInt());
	EXPECT_NO_THROW(opt.Get("Array/Double")->GetListDouble());
	EXPECT_NO_THROW(opt.Get("Array/Boolean")->GetListBool());
	EXPECT_NO_THROW(opt.Get("Array/Color")->GetListColor());
}

TEST_F(lagi_option, bad_default_throws_and_null_is_rejected) {
	EXPECT_THROW(agi::Options("", "{\"Null\" : null}", agi::Options::FLUSH_SKIP), agi::Exception);
}

TEST_F(lagi_option, nested_options) {
	const char conf[] = R"raw({ "a" : { "b" : { "c" : { "c" : "value" } } } })raw";
	ASSERT_NO_THROW(agi::Options("", conf, agi::Options::FLUSH_SKIP));
	agi::Options opt("", conf, agi::Options::FLUSH_SKIP);
	ASSERT_NO_THROW(opt.Get("a/b/c/c"));
	ASSERT_NO_THROW(opt.Get("a/b/c/c")->GetString());
	EXPECT_STREQ("value", opt.Get("a/b/c/c")->GetString().c_str());
}

TEST_F(lagi_option, heterogeneous_arrays_rejected) {
	EXPECT_NO_THROW(agi::Options("", "{ \"key\" : [ { \"bool\" : true }] }", agi::Options::FLUSH_SKIP));
	EXPECT_THROW(agi::Options("", "{ \"key\" : [ { \"bool\" : true }, { \"double\" : 1.0 } ] }", agi::Options::FLUSH_SKIP), agi::Exception);
}

TEST_F(lagi_option, set_works) {
	agi::Options opt("", all_types, agi::Options::FLUSH_SKIP);

	ASSERT_NO_THROW(opt.Get("Integer")->SetInt(1000));
	EXPECT_EQ(1000, opt.Get("Integer")->GetInt());

	ASSERT_NO_THROW(opt.Get("String")->SetString("Hello"));
	EXPECT_EQ("Hello", opt.Get("String")->GetString());
}

TEST_F(lagi_option, flush_roundtrip) {
	agi::fs::Remove("data/options/tmp");

	{
		agi::Options opt("data/options/tmp", all_types);
		EXPECT_NO_THROW(opt.Get("Integer")->SetInt(1));
		EXPECT_NO_THROW(opt.Get("Double")->SetDouble(1.1));
		EXPECT_NO_THROW(opt.Get("String")->SetString("hello"));
		EXPECT_NO_THROW(opt.Get("Color")->SetColor(agi::Color("rgb(255,255,255)")));
		EXPECT_NO_THROW(opt.Get("Boolean")->SetBool(true));

		EXPECT_NO_THROW(opt.Get("Array/Integer")->SetListInt({1}));
		EXPECT_NO_THROW(opt.Get("Array/Double")->SetListDouble({1.1}));
		EXPECT_NO_THROW(opt.Get("Array/String")->SetListString({"hello"}));
		EXPECT_NO_THROW(opt.Get("Array/Color")->SetListColor({agi::Color("rgb(255,255,255)")}));
		EXPECT_NO_THROW(opt.Get("Array/Boolean")->SetListBool({true}));
	}

	{
		agi::Options opt("data/options/tmp", all_types);
		ASSERT_NO_THROW(opt.ConfigUser());

		EXPECT_EQ(1, opt.Get("Integer")->GetInt());
		EXPECT_EQ(1.1, opt.Get("Double")->GetDouble());
		EXPECT_STREQ("hello", opt.Get("String")->GetString().c_str());
		EXPECT_STREQ("rgb(255, 255, 255)", opt.Get("Color")->GetColor().GetRgbFormatted().c_str());
		EXPECT_EQ(true, opt.Get("Boolean")->GetBool());

		EXPECT_EQ(1, opt.Get("Array/Integer")->GetListInt().size());
		EXPECT_EQ(1, opt.Get("Array/Double")->GetListDouble().size());
		EXPECT_EQ(1, opt.Get("Array/String")->GetListString().size());
		EXPECT_EQ(1, opt.Get("Array/Color")->GetListColor().size());
		EXPECT_EQ(1, opt.Get("Array/Boolean")->GetListBool().size());

		EXPECT_EQ(1, opt.Get("Array/Integer")->GetListInt().front());
		EXPECT_EQ(1.1, opt.Get("Array/Double")->GetListDouble().front());
		EXPECT_STREQ("hello", opt.Get("Array/String")->GetListString().front().c_str());
		EXPECT_STREQ("rgb(255, 255, 255)", opt.Get("Array/Color")->GetListColor().front().GetRgbFormatted().c_str());
		EXPECT_EQ(true, opt.Get("Array/Boolean")->GetListBool().front());
	}
}

TEST_F(lagi_option, mixed_valid_and_invalid_in_user_conf_loads_all_valid) {
	const char def[] = "{\"1\" : false, \"2\" : 1, \"3\" : false }";
	agi::Options opt(util::test_data_dir() / "options/all_bool.json", def, agi::Options::FLUSH_SKIP);
	ASSERT_NO_THROW(opt.ConfigUser());
	EXPECT_EQ(true, opt.Get("1")->GetBool());
	EXPECT_EQ(1, opt.Get("2")->GetInt());
	EXPECT_EQ(true, opt.Get("3")->GetBool());
}

TEST_F(lagi_option, empty_object_works) {
	EXPECT_NO_THROW(agi::Options("", "{ \"obj\" : {} }", agi::Options::FLUSH_SKIP));
}

TEST_F(lagi_option, unknown_array_type) {
	EXPECT_THROW(agi::Options("", "{ \"arr\" : [ { \"float\" : 5.0 } ] }"), agi::Exception);
}

TEST_F(lagi_option, malformed_arrays) {
	EXPECT_THROW(agi::Options("", "{ \"arr\" : [ {} ] }"), agi::Exception);
	EXPECT_THROW(agi::Options("", "{ \"arr\" : [ { \"double\" : 5.0 }, {} ] }"), agi::Exception);
	EXPECT_THROW(agi::Options("", "{ \"arr\" : [ { \"double\" : 5.0, \"int\" : 5 } ] }"), agi::Exception);
}

TEST_F(lagi_option, int_vs_double) {
	agi::Options opt("", "{ \"int\" : 5, \"double\" : 5.0 }", agi::Options::FLUSH_SKIP);
	EXPECT_NO_THROW(opt.Get("int")->GetInt());
	EXPECT_NO_THROW(opt.Get("double")->GetDouble());
}

struct empty_arr_options : public agi::Options {
	empty_arr_options() : agi::Options("", "{ \"arr\" : [] }", agi::Options::FLUSH_SKIP) { }
};

#define CHECK_TYPE(str, type) \
	do { \
		agi::Options opt("", "{ \"" str "\" : \"" str "\" }", agi::Options::FLUSH_SKIP); \
		EXPECT_NO_THROW(opt.Get(str)->Get##type()); \
	} while (false)

TEST_F(lagi_option, color_vs_string) {
	CHECK_TYPE("#", String);
	CHECK_TYPE("#a", String);
	CHECK_TYPE("#abc", Color);
	CHECK_TYPE("#aabbcc", Color);
	CHECK_TYPE("#aabb", String);

	CHECK_TYPE("&", String);
	CHECK_TYPE("&H000000&", Color);
	CHECK_TYPE("&H00000000", Color);
}

static const char spoiler_defaults[] = R"raw({
	"Video" : {
		"Spoiler Shield" : {
			"Enabled" : false,
			"Effect" : 0,
			"Color" : "rgb(64, 64, 64)",
			"Mosaic Block" : 16,
			"Reveal Rect" : "0.1,0.68,0.8,0.26"
		}
	}
})raw";

static void write_option_file(const char *path, const char *json) {
	std::ofstream out(path);
	out << json;
}

TEST_F(lagi_option, spoiler_shield_migrates_legacy_draft_cover) {
	const char *path = "data/options/tmp_migrate_all";
	write_option_file(path, R"raw({
	"Video" : {
		"Draft Cover" : {
			"Enabled" : true,
			"Effect" : 1,
			"Color" : "rgb(10, 20, 30)",
			"Blur Strength" : 8,
			"Mosaic Block" : 40,
			"Reveal Rect" : "0.2,0.3,0.4,0.5"
		}
	}
})raw");

	agi::Options opt(path, spoiler_defaults, agi::Options::FLUSH_SKIP);
	spoiler::RegisterOptionsMigration(opt);
	ASSERT_NO_THROW(opt.ConfigUser());

	EXPECT_TRUE(opt.Get("Video/Spoiler Shield/Enabled")->GetBool());
	EXPECT_EQ(1, opt.Get("Video/Spoiler Shield/Effect")->GetInt());
	EXPECT_STREQ("rgb(10, 20, 30)", opt.Get("Video/Spoiler Shield/Color")->GetColor().GetRgbFormatted().c_str());
	EXPECT_EQ(40, opt.Get("Video/Spoiler Shield/Mosaic Block")->GetInt());
	EXPECT_STREQ("0.2,0.3,0.4,0.5", opt.Get("Video/Spoiler Shield/Reveal Rect")->GetString().c_str());

	// The removed blur strength option has no current equivalent.
	EXPECT_THROW(opt.Get("Video/Spoiler Shield/Blur Strength"), agi::InternalError);
}

TEST_F(lagi_option, spoiler_shield_maps_removed_blur_to_solid) {
	const char *path = "data/options/tmp_migrate_blur";
	write_option_file(path, R"raw({ "Video" : { "Draft Cover" : { "Effect" : 2 } } })raw");

	agi::Options opt(path, spoiler_defaults, agi::Options::FLUSH_SKIP);
	spoiler::RegisterOptionsMigration(opt);
	ASSERT_NO_THROW(opt.ConfigUser());
	EXPECT_EQ(0, opt.Get("Video/Spoiler Shield/Effect")->GetInt());
}

TEST_F(lagi_option, spoiler_shield_maps_invalid_effect_to_solid) {
	const char *path = "data/options/tmp_migrate_invalid";
	const int bad_effects[] = {7, -1, 100};
	for (int bad : bad_effects) {
		{
			std::ofstream out(path);
			out << "{ \"Video\" : { \"Draft Cover\" : { \"Effect\" : " << bad << " } } }";
		}
		agi::Options opt(path, spoiler_defaults, agi::Options::FLUSH_SKIP);
		spoiler::RegisterOptionsMigration(opt);
		ASSERT_NO_THROW(opt.ConfigUser());
		EXPECT_EQ(0, opt.Get("Video/Spoiler Shield/Effect")->GetInt()) << "legacy effect " << bad;
	}
}

TEST_F(lagi_option, spoiler_shield_explicit_new_values_win) {
	const char *path = "data/options/tmp_migrate_merge";
	write_option_file(path, R"raw({
	"Video" : {
		"Draft Cover" : {
			"Enabled" : false,
			"Effect" : 2,
			"Color" : "rgb(1, 2, 3)",
			"Mosaic Block" : 8,
			"Reveal Rect" : "0,0,1,1"
		},
		"Spoiler Shield" : {
			"Enabled" : true
		}
	}
})raw");

	agi::Options opt(path, spoiler_defaults, agi::Options::FLUSH_SKIP);
	spoiler::RegisterOptionsMigration(opt);
	ASSERT_NO_THROW(opt.ConfigUser());

	// The explicitly-set new value wins over the legacy one.
	EXPECT_TRUE(opt.Get("Video/Spoiler Shield/Enabled")->GetBool());
	// Keys not set under the new names are still filled from the legacy group.
	EXPECT_EQ(0, opt.Get("Video/Spoiler Shield/Effect")->GetInt());
	EXPECT_STREQ("rgb(1, 2, 3)", opt.Get("Video/Spoiler Shield/Color")->GetColor().GetRgbFormatted().c_str());
	EXPECT_EQ(8, opt.Get("Video/Spoiler Shield/Mosaic Block")->GetInt());
	EXPECT_STREQ("0,0,1,1", opt.Get("Video/Spoiler Shield/Reveal Rect")->GetString().c_str());
}

TEST_F(lagi_option, spoiler_shield_ignores_malformed_legacy_values) {
	const char *path = "data/options/tmp_migrate_malformed";
	write_option_file(path, R"raw({
	"Video" : {
		"Draft Cover" : {
			"Enabled" : "yes",
			"Effect" : "blur",
			"Color" : "not a colour",
			"Mosaic Block" : 999,
			"Reveal Rect" : "garbage",
			"Blur Strength" : { "unexpected" : 1 }
		}
	}
})raw");

	agi::Options opt(path, spoiler_defaults, agi::Options::FLUSH_SKIP);
	spoiler::RegisterOptionsMigration(opt);
	ASSERT_NO_THROW(opt.ConfigUser());

	// Malformed values fall back to the defaults instead of being copied.
	EXPECT_FALSE(opt.Get("Video/Spoiler Shield/Enabled")->GetBool());
	EXPECT_EQ(0, opt.Get("Video/Spoiler Shield/Effect")->GetInt());
	EXPECT_STREQ("rgb(64, 64, 64)", opt.Get("Video/Spoiler Shield/Color")->GetColor().GetRgbFormatted().c_str());
	EXPECT_STREQ("0.1,0.68,0.8,0.26", opt.Get("Video/Spoiler Shield/Reveal Rect")->GetString().c_str());
	// A well-typed legacy value still migrates.
	EXPECT_EQ(999, opt.Get("Video/Spoiler Shield/Mosaic Block")->GetInt());
}

TEST_F(lagi_option, spoiler_shield_migration_runs_only_for_user_profile) {
	agi::Options opt("", spoiler_defaults, agi::Options::FLUSH_SKIP);
	spoiler::RegisterOptionsMigration(opt);

	// Additional configs loaded with ConfigNext() are not the user profile and
	// must not be migrated; their legacy keys are simply discarded.
	std::istringstream next(R"raw({ "Video" : { "Draft Cover" : { "Effect" : 1, "Mosaic Block" : 77 } } })raw");
	opt.ConfigNext(next);

	EXPECT_EQ(0, opt.Get("Video/Spoiler Shield/Effect")->GetInt());
	EXPECT_EQ(16, opt.Get("Video/Spoiler Shield/Mosaic Block")->GetInt());
}

TEST_F(lagi_option, spoiler_shield_migration_is_idempotent) {
	std::vector<std::unique_ptr<agi::OptionValue>> values;
	values.push_back(std::make_unique<agi::OptionValueBool>("Video/Draft Cover/Enabled", true));
	values.push_back(std::make_unique<agi::OptionValueInt>("Video/Draft Cover/Effect", 2));
	values.push_back(std::make_unique<agi::OptionValueInt>("Video/Draft Cover/Mosaic Block", 32));

	spoiler::MigrateLegacyOptions(values);
	auto after_first = values.size();

	spoiler::MigrateLegacyOptions(values);
	EXPECT_EQ(after_first, values.size());

	int enabled = 0, effect = 0, mosaic = 0;
	for (auto const &value : values) {
		if (value->GetName() == "Video/Spoiler Shield/Enabled") {
			EXPECT_TRUE(value->GetBool());
			++enabled;
		}
		else if (value->GetName() == "Video/Spoiler Shield/Effect") {
			EXPECT_EQ(0, value->GetInt());
			++effect;
		}
		else if (value->GetName() == "Video/Spoiler Shield/Mosaic Block") {
			EXPECT_EQ(32, value->GetInt());
			++mosaic;
		}
	}
	EXPECT_EQ(1, enabled);
	EXPECT_EQ(1, effect);
	EXPECT_EQ(1, mosaic);
}
