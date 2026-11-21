// Copyright (c) 2007, Rodrigo Braz Monteiro
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

/// @file subtitles_provider_csri.cpp
/// @brief Wrapper for CSRI-based subtitle renderers
/// @ingroup subtitle_rendering
///

#ifdef WITH_CSRI
#include "subtitles_provider_csri.h"

#include "include/aegisub/subtitles_provider.h"
#include "subtitle_format_ass.h"
#include "video_frame.h"

#include <libaegisub/dialogue_text_mask.h>

#include <mutex>
#include <string>

#ifdef WIN32
#define CSRIAPI
#endif

#include <csri/csri.h>

namespace {
// CSRI renderers are not required to be thread safe (and VSFilter very much
// is not)
std::mutex csri_mutex;

struct closer {
	void operator()(csri_inst *inst) { if (inst) csri_close(inst); }
};

class CSRISubtitlesProvider final : public SubtitlesProvider {
	std::unique_ptr<csri_inst, closer> instance;
	std::unique_ptr<csri_inst, closer> masked_instance;
	csri_rend *renderer = nullptr;
	std::string source_data;
	std::string masked_data;
	std::string masked_token;
	bool replacement_enabled = false;
	std::string replacement_token;

	/// 按当前替换符生成（或复用）预览专用的 CSRI 实例。调用方需持有 csri_mutex。
	csri_inst *EnsureMaskedInstance() {
		std::string token = agi::ass::NormalizeMaskToken(replacement_token);
		if (masked_instance && masked_token == token)
			return masked_instance.get();
		masked_instance.reset();
		masked_data = agi::ass::MaskAssData(source_data, token);
		masked_instance.reset(csri_open_mem(renderer, masked_data.data(), masked_data.size(), nullptr));
		masked_token = token;
		return masked_instance.get();
	}

	void LoadSubtitles(const char *data, size_t len) override {
		std::lock_guard<std::mutex> lock(csri_mutex);
		source_data.assign(data, len);
		instance.reset(csri_open_mem(renderer, data, len, nullptr));
		masked_instance.reset();
		masked_data.clear();
		masked_token.clear();
	}

public:
	CSRISubtitlesProvider(std::string subType);

	void SetTextReplacement(bool enabled, std::string const& token) override {
		replacement_enabled = enabled;
		replacement_token = token;
	}

	void DrawSubtitles(VideoFrame &dst, double time) override;
};

CSRISubtitlesProvider::CSRISubtitlesProvider(std::string type) {
	std::lock_guard<std::mutex> lock(csri_mutex);
	for (csri_rend *cur = csri_renderer_default(); cur; cur = csri_renderer_next(cur)) {
		if (type == csri_renderer_info(cur)->name) {
			renderer = cur;
			break;
		}
	}

	if (!renderer)
		throw agi::InternalError("CSRI renderer vanished between initial list and creation?");
}

void CSRISubtitlesProvider::DrawSubtitles(VideoFrame &dst, double time) {
	if (!instance) return;

	csri_frame frame;
	if (dst.flipped) {
		frame.planes[0] = dst.data.data() + (dst.height-1) * dst.width * 4;
		frame.strides[0] = -(signed)dst.width * 4;
	}
	else {
		frame.planes[0] = dst.data.data();
		frame.strides[0] = dst.width * 4;
	}
	frame.pixfmt = CSRI_F_BGR_;

	csri_fmt format = { frame.pixfmt, static_cast<unsigned>(dst.width), static_cast<unsigned>(dst.height) };

	std::lock_guard<std::mutex> lock(csri_mutex);
	csri_inst *active = replacement_enabled ? EnsureMaskedInstance() : instance.get();
	if (!active) return;
	if (!csri_request_fmt(active, &format))
		// Add 1e-9 to guard against floating point imprecision errors on int -> float -> *1000 -> int round trips
		csri_render(active, &frame, time + 1e-9);
}
}

namespace csri {
std::vector<std::string> List() {
	std::vector<std::string> final;
	for (csri_rend *cur = csri_renderer_default(); cur; cur = csri_renderer_next(cur)) {
		std::string name(csri_renderer_info(cur)->name);
		if (name.find("aegisub") != name.npos)
			final.insert(final.begin(), name);
		else
			final.push_back(name);
	}
	return final;
}

std::unique_ptr<SubtitlesProvider> Create(std::string const& name, agi::BackgroundRunner *) {
	return std::make_unique<CSRISubtitlesProvider>(name);
}
}
#endif // WITH_CSRI
