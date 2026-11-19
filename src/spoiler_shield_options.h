#pragma once

#include "options.h"

namespace spoiler {
inline constexpr char Enabled[] = "Video/Spoiler Shield/Enabled";
inline constexpr char EffectKey[] = "Video/Spoiler Shield/Effect";
inline constexpr char Color[] = "Video/Spoiler Shield/Color";
inline constexpr char MosaicBlock[] = "Video/Spoiler Shield/Mosaic Block";
inline constexpr char RevealRect[] = "Video/Spoiler Shield/Reveal Rect";

enum class Effect { Solid = 0, Mosaic = 1 };

inline Effect GetEffect() {
	return OPT_GET(EffectKey)->GetInt() == static_cast<int>(Effect::Mosaic) ? Effect::Mosaic : Effect::Solid;
}

inline void SetEffect(Effect effect) {
	OPT_SET(EffectKey)->SetInt(static_cast<int>(effect));
}
}
