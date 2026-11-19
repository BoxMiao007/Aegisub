#pragma once

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>

namespace spoiler {

struct Rect {
	double x = 0.1, y = 0.68, w = 0.8, h = 0.26;
};

inline bool ParseRect(std::string const& text, Rect& out) {
	std::istringstream input(text);
	input.imbue(std::locale::classic());
	Rect r;
	char a, b, c;
	if (!(input >> r.x >> a >> r.y >> b >> r.w >> c >> r.h) || a != ',' || b != ',' || c != ','
		|| !(input >> std::ws).eof() || !std::isfinite(r.x) || !std::isfinite(r.y)
		|| !std::isfinite(r.w) || !std::isfinite(r.h) || r.w <= 0 || r.h <= 0)
		return false;

	// 拖动时按逻辑像素限制最小尺寸；读取配置时不再按视频宽高的百分比放大。
	r.w = std::min(r.w, 1.0);
	r.h = std::min(r.h, 1.0);
	r.x = std::clamp(r.x, 0.0, 1.0 - r.w);
	r.y = std::clamp(r.y, 0.0, 1.0 - r.h);
	out = r;
	return true;
}

inline std::string FormatRect(Rect const& r) {
	std::ostringstream output;
	output.imbue(std::locale::classic());
	output << std::setprecision(std::numeric_limits<double>::max_digits10)
		<< r.x << ',' << r.y << ',' << r.w << ',' << r.h;
	return output.str();
}

struct WindowRect {
	double left, top, right, bottom;

	double Width() const { return right - left; }
	double Height() const { return bottom - top; }
};

inline WindowRect ToWindow(Rect const& r, WindowRect const& video) {
	return {video.left + r.x * video.Width(), video.top + r.y * video.Height(),
		video.left + (r.x + r.w) * video.Width(), video.top + (r.y + r.h) * video.Height()};
}

enum class Grab : unsigned { Outside = 0, Left = 1, Right = 2, Top = 4, Bottom = 8, Move = 16 };

constexpr Grab operator|(Grab a, Grab b) {
	return static_cast<Grab>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

inline bool HasEdge(Grab grab, Grab edge) {
	return (static_cast<unsigned>(grab) & static_cast<unsigned>(edge)) != 0;
}

/// Which border, corner or interior of the reveal rectangle (window
/// coordinates) a pointer grabs.
inline Grab HitTest(WindowRect const& r, double x, double y) {
	constexpr double band = 8;
	if (x < r.left - band || x > r.right + band || y < r.top - band || y > r.bottom + band)
		return Grab::Outside;
	auto closest_edge = [](double p, double first, double last, Grab low, Grab high) {
		double a = std::abs(p - first), b = std::abs(p - last);
		return std::min(a, b) > band ? Grab::Outside : (a <= b ? low : high);
	};
	auto edges = closest_edge(x, r.left, r.right, Grab::Left, Grab::Right)
		| closest_edge(y, r.top, r.bottom, Grab::Top, Grab::Bottom);
	return edges == Grab::Outside ? Grab::Move : edges;
}

inline void Move(Rect& r, double dx, double dy) {
	r.x = std::clamp(r.x + dx, 0.0, 1.0 - r.w);
	r.y = std::clamp(r.y + dy, 0.0, 1.0 - r.h);
}

/// Resize the reveal rectangle by dragging one or more of its borders.
/// @param video The video content rectangle in window coordinates. This must
///              not be the reveal rectangle itself: using the box being
///              resized as the reference makes every drag step feed its own
///              previous size back in, which makes the box jump.
inline void Resize(Rect& r, Grab grab, WindowRect const& video, double x, double y) {
	auto resize_axis = [grab](double& pos, double& size, double pointer, double extent, Grab low, Grab high) {
		double minimum = 24.0 / extent;
		if (HasEdge(grab, low)) {
			double fixed = pos + size;
			pos = std::clamp(pointer / extent, 0.0, std::max(0.0, fixed - minimum));
			size = fixed - pos;
		}
		else if (HasEdge(grab, high)) {
			double end = std::clamp(pointer / extent, std::min(1.0, pos + minimum), 1.0);
			size = end - pos;
		}
	};
	resize_axis(r.x, r.w, x - video.left, video.Width(), Grab::Left, Grab::Right);
	resize_axis(r.y, r.h, y - video.top, video.Height(), Grab::Top, Grab::Bottom);
}

// 为 OpenGL 1.1 的纹理分配二次幂长度，保证扩容时不会溢出或越过驱动上限。
inline int TextureExtent(int used, int limit) {
	if (used <= 0 || used > limit) return 0;
	int size = 1;
	while (size < used) {
		if (size > limit / 2) return 0;
		size *= 2;
	}
	return size;
}

struct TextureAxis {
	int used = 0;
	int allocated = 0;
	double uv = 0;
};

inline TextureAxis MosaicAxis(int pixels, int block, int limit) {
	if (pixels <= 0 || block <= 0) return {};
	int used = 1 + (pixels - 1) / block;
	int allocated = TextureExtent(used, limit);
	if (!allocated) return {};
	// 不拉伸最后一个不足整块的格子，也不采样纹理的填充区。
	return {used, allocated, double(used) / allocated};
}

} // namespace spoiler
