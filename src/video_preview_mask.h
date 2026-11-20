// Copyright (c) 2026, BoxMiao007
// SPDX-License-Identifier: ISC
#pragma once

#include <atomic>
#include <cstdint>

class VideoPreviewMask {
public:
	struct Token {
		uint64_t source;
		uint64_t state;
		bool Masked() const { return state & 1; }
	};
private:
	inline static std::atomic<uint64_t> next_source{1};
	const uint64_t source = next_source.fetch_add(1);
	std::atomic<uint64_t> state{0};
public:
	explicit VideoPreviewMask(bool masked = false) : state(masked ? 1 : 0) { }
	Token Capture() const { return {source, state.load()}; }
	bool Accept(Token token) const { return token.source == source && token.state == state.load(); }
	bool Set(bool masked) {
		auto old = state.load();
		for (;;) {
			if (static_cast<bool>(old & 1) == masked) return false;
			auto next = (((old >> 1) + 1) << 1) | static_cast<uint64_t>(masked);
			if (state.compare_exchange_weak(old, next)) return true;
		}
	}
};
