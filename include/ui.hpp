#pragma once
#include <gfx.hpp>
#include <uix.hpp>
// colors for the UI
using color_t = gfx::color<gfx::rgb_pixel<16>>; // native
using color32_t = gfx::color<gfx::rgba_pixel<32>>; // uix

// the screen template instantiation aliases
using screen_t = uix::screen<gfx::rgb_pixel<16>>;
using surface_t = screen_t::control_surface_type;

// the control template instantiation aliases
using svg_clock_t = uix::svg_clock<surface_t>;
using label_t = uix::label<surface_t>;
using canvas_t = uix::canvas<surface_t>;

// the screen/control declarations
extern screen_t main_screen;
extern svg_clock_t ana_clock;
extern label_t weekday;
extern label_t dig_clock;
extern label_t timezone;
extern canvas_t wifi_icon;
extern canvas_t battery_icon;
