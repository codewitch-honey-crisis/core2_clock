#pragma once
#include <time.h>
#include <gfx.hpp>
#include <uix.hpp>

// colors for the UI
using color_t = gfx::color<gfx::rgb_pixel<16>>; // native
using color32_t = gfx::color<gfx::rgba_pixel<32>>; // uix

// the screen template instantiation aliases
using screen_t = uix::screen<gfx::rgb_pixel<16>>;
using surface_t = screen_t::control_surface_type;

template<typename ControlSurfaceType>
class vclock : public uix::canvas_control<ControlSurfaceType> {
    using base_type = uix::canvas_control<ControlSurfaceType>;
public:
    using type = vclock;
    using control_surface_type = ControlSurfaceType;
private:
    constexpr static const  uint16_t default_face_border_width = 2;
    constexpr static const  gfx::vector_pixel default_face_border_color = gfx::color<gfx::vector_pixel>::black;
    constexpr static const  gfx::vector_pixel default_face_color = gfx::color<gfx::vector_pixel>::white;
    constexpr static const  gfx::vector_pixel default_tick_border_color = gfx::color<gfx::vector_pixel>::gray;
    constexpr static const  gfx::vector_pixel default_tick_color = gfx::color<gfx::vector_pixel>::gray;
    constexpr static const  uint16_t default_tick_border_width = 2;
    constexpr static const  gfx::vector_pixel default_minute_color = gfx::color<gfx::vector_pixel>::black;
    constexpr static const  gfx::vector_pixel default_minute_border_color = gfx::color<gfx::vector_pixel>::gray;
    constexpr static const  uint16_t default_minute_border_width = 2;
    constexpr static const  gfx::vector_pixel default_hour_color = gfx::color<gfx::vector_pixel>::black;
    constexpr static const  gfx::vector_pixel default_hour_border_color = gfx::color<gfx::vector_pixel>::gray;
    constexpr static const  uint16_t default_hour_border_width = 2;
    constexpr static const  gfx::vector_pixel default_second_color = gfx::color<gfx::vector_pixel>::red;
    constexpr static const  gfx::vector_pixel default_second_border_color = gfx::color<gfx::vector_pixel>::red;
    constexpr static const  uint16_t default_second_border_width = 2;
    using fb_t = gfx::bitmap<typename control_surface_type::pixel_type,typename control_surface_type::palette_type>;
    uint16_t m_face_border_width;
    gfx::vector_pixel m_face_border_color;
    gfx::vector_pixel m_face_color;
    gfx::vector_pixel m_tick_border_color;
    gfx::vector_pixel m_tick_color;
    uint16_t m_tick_border_width;
    gfx::vector_pixel m_minute_color;
    gfx::vector_pixel m_minute_border_color;
    uint16_t m_minute_border_width;
    gfx::vector_pixel m_hour_color;
    gfx::vector_pixel m_hour_border_color;
    uint16_t m_hour_border_width;
    gfx::vector_pixel m_second_color;
    gfx::vector_pixel m_second_border_color;
    uint16_t m_second_border_width;
    time_t m_time;
    bool m_face_dirty;
    bool m_buffer_face;
    fb_t m_face_buffer;
    // compute thetas for a rotation
    static void update_transform(float rotation, float& ctheta, float& stheta) {
        float rads = gfx::math::deg2rad(rotation); // rotation * (3.1415926536f / 180.0f);
        ctheta = cosf(rads);
        stheta = sinf(rads);
    }
    // transform a point given some thetas, a center and an offset
    static gfx::pointf transform_point(float ctheta, float stheta, gfx::pointf center, gfx::pointf offset, float x, float y) {
        float rx = (ctheta * (x - (float)center.x) - stheta * (y - (float)center.y) + (float)center.x) + offset.x;
        float ry = (stheta * (x - (float)center.x) + ctheta * (y - (float)center.y) + (float)center.y) + offset.y;
        return {(float)rx, (float)ry};
    }
    gfx::gfx_result draw_clock_face(gfx::canvas& clock_canvas) {   
        constexpr static const float rot_step = 360.0f / 12.0f;
        gfx::pointf offset(0, 0);
        gfx::pointf center(0, 0);

        float rotation(0);
        float ctheta, stheta;
        gfx::ssize16 size = (gfx::ssize16)clock_canvas.dimensions();
        gfx::rectf b = gfx::sizef(size.width, size.height).bounds();
        b.inflate_inplace(-m_face_border_width - 1, -m_face_border_width - 1);
        float w = b.width();
        float h = b.height();
        if(w>h) w= h;
        gfx::rectf sr(0, w / 30, w / 30, w / 5);
        sr.center_horizontal_inplace(b);
        center = gfx::pointf(w * 0.5f + m_face_border_width + 1, w * 0.5f + m_face_border_width + 1);
        clock_canvas.fill_color(m_face_color);
        clock_canvas.stroke_color(m_face_border_color);
        clock_canvas.stroke_width(m_face_border_width);
        clock_canvas.circle(center, center.x - 1);
        gfx::gfx_result res = clock_canvas.render();
        if(res!=gfx::gfx_result::success) {
            return res;
        }
        bool toggle = false;
        clock_canvas.stroke_color(m_tick_border_color);
        clock_canvas.fill_color(m_tick_color);
        clock_canvas.stroke_width(m_tick_border_width);
        
        for (float rot = 0; rot < 360.0f; rot += rot_step) {
            rotation = rot;
            update_transform(rotation, ctheta, stheta);
            toggle = !toggle;
            if (toggle) {
                clock_canvas.move_to(transform_point(ctheta, stheta, center, offset, sr.x1, sr.y1));
                clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x2, sr.y1));
                clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x2, sr.y2));
                clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x1, sr.y2));
                clock_canvas.close_path();
            } else {
                clock_canvas.move_to(transform_point(ctheta, stheta, center, offset, sr.x1, sr.y1));
                clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x2, sr.y1));
                clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x2, sr.y2 - sr.height() * 0.5f));
                clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x1, sr.y2 - sr.height() * 0.5f));
                clock_canvas.close_path();
            }
            res=clock_canvas.render();
            if(res!=gfx::gfx_result::success) {
                return res;
            }
        }    
        return gfx::gfx_result::success;
    }
    void draw_clock_time(gfx::canvas& clock_canvas) {
        gfx::pointf offset(0, 0);
        gfx::pointf center(0, 0);
        float rotation(0);
        float ctheta, stheta;
        time_t time = m_time;
        gfx::ssize16 size = (gfx::ssize16)clock_canvas.dimensions();
        gfx::rectf b = gfx::sizef(size.width, size.height).bounds();
        b.inflate_inplace(-m_face_border_width - 1, -m_face_border_width - 1);
        float w = b.width();
        float h = b.height();
        if(w>h) w= h;
        gfx::rectf sr(0, w / 30, w / 30, w / 5);
        sr.center_horizontal_inplace(b);
        center = gfx::pointf(w * 0.5f + m_face_border_width + 1, w * 0.5f + m_face_border_width + 1);
        sr = gfx::rectf(0, w / 40, w / 16, w / 2);
        sr.center_horizontal_inplace(b);
        // create a path for the minute hand:
        rotation = (fmodf(time / 60.0f, 60) / 60.0f) * 360.0f;
        update_transform(rotation, ctheta, stheta);
        clock_canvas.move_to(transform_point(ctheta, stheta, center, offset, sr.x1 + sr.width() * 0.5f, sr.y1));
        clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x2, sr.y2));
        clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x1 + sr.width() * 0.5f, sr.y2 + (w / 20)));
        clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x1, sr.y2));
        clock_canvas.close_path();
        clock_canvas.fill_color(m_minute_color);
        clock_canvas.stroke_color(m_minute_border_color);
        clock_canvas.stroke_width(m_minute_border_width);
        clock_canvas.render(); // render the path
        // create a path for the hour hand
        sr.y1 += w / 8;
        rotation = (fmodf(time / (3600.0f), 12.0f) / (12.0f)) * 360.0f;
        update_transform(rotation, ctheta, stheta);
        clock_canvas.move_to(transform_point(ctheta, stheta, center, offset, sr.x1 + sr.width() * 0.5f, sr.y1));
        clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x2, sr.y2));
        clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x1 + sr.width() * 0.5f, sr.y2 + (w / 20)));
        clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x1, sr.y2));
        clock_canvas.close_path();
        clock_canvas.fill_color(m_hour_color);
        clock_canvas.stroke_color(m_hour_border_color);
        clock_canvas.stroke_width(m_hour_border_width);
        clock_canvas.render(); // render the path
        // create a path for the second hand
        sr.y1 -= w / 8;
        rotation = ((time % 60) / 60.0f) * 360.0f;
        update_transform(rotation, ctheta, stheta);
        clock_canvas.move_to(transform_point(ctheta, stheta, center, offset, sr.x1 + sr.width() * 0.5f, sr.y1));
        clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x2, sr.y2));
        clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x1 + sr.width() * 0.5f, sr.y2 + (w / 20)));
        clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x1, sr.y2));
        clock_canvas.close_path();
        clock_canvas.fill_color(m_second_color);
        clock_canvas.stroke_color(m_second_border_color);
        clock_canvas.stroke_width(m_second_border_width);
        clock_canvas.render();
    }
public:
    vclock() : base_type(),
            m_face_border_width(default_face_border_width),
            m_face_border_color(default_face_border_color),
            m_face_color(default_face_color),
            m_tick_color(default_tick_color),
            m_tick_border_width(default_tick_border_width),
            m_minute_color(default_minute_color),
            m_minute_border_color(default_minute_border_color),
            m_minute_border_width(default_minute_border_width),
            m_hour_color(default_hour_color),
            m_hour_border_color(default_hour_border_color),
            m_hour_border_width(default_hour_border_width),
            m_second_color(default_second_color),
            m_second_border_color(default_second_border_color),
            m_second_border_width(default_second_border_width),
            m_time(0),
            m_face_dirty(true),
            m_buffer_face(true)
              {

    }
    vclock(const vclock& rhs) {
        *this = rhs;
    }
    vclock(vclock&& rhs) {
        *this = rhs;
    }
    vclock& operator=(const vclock& rhs) {
        *this = rhs;
    }
    vclock& operator=(vclock&& rhs) {
        *this=rhs;
    }
    virtual ~vclock() {
        if(m_face_buffer.begin()) {
            free(m_face_buffer.begin());
        }
    }
protected:
    
    virtual void on_paint(control_surface_type& destination, const uix::srect16& clip) override {
        if(m_buffer_face && m_face_dirty) {
            typename control_surface_type::pixel_type bg;
            destination.point({0,0},&bg);
            int16_t w = this->dimensions().width;
            int16_t h = this->dimensions().height;
            if(w>h) w= h;
            fb_t bmp = gfx::create_bitmap<typename fb_t::pixel_type,typename fb_t::palette_type>(gfx::size16(w,w));
            if(bmp.begin()) {
                bmp.fill(bmp.bounds(),bg);
                gfx::canvas cvs(gfx::size16(w,w));
                if(gfx::gfx_result::success==cvs.initialize()) {
                    gfx::draw::canvas(bmp,cvs,gfx::point16::zero());
                    if(gfx::gfx_result::success==draw_clock_face(cvs)) {
                        m_face_buffer = bmp;
                        m_face_dirty = false;
                    }
                }
            }
        }
        if(m_buffer_face && !m_face_dirty) {
            // we have a valid bitmap, no need to draw the face here.
            gfx::draw::bitmap(destination,m_face_buffer.bounds(),m_face_buffer,m_face_buffer.bounds());
        }
        base_type::on_paint(destination,clip);
    }
    virtual void on_paint(gfx::canvas& destination, const uix::srect16& clip) override {
        if(!m_buffer_face || m_face_dirty) {
            draw_clock_face(destination);
        }
        draw_clock_time(destination);
    }
public:
    uint16_t face_border_width() const {
        return m_face_border_width;
    }
    void face_border_width(uint16_t value) {
        m_face_border_width = value;
        m_face_dirty = true;
        this->invalidate();
    }
    gfx::rgba_pixel<32> face_border_color() const {
        gfx::rgba_pixel<32> result;
        convert(m_face_border_color,&result);
        return result;
    }
    void face_border_color(gfx::rgba_pixel<32> value) {
        convert(value,&m_face_border_color);
        m_face_dirty = true;
        this->invalidate();
    }
    gfx::rgba_pixel<32> face_color() const {
        gfx::rgba_pixel<32> result;
        convert(m_face_color,&result);
        return result;
    }
    void face_color(gfx::rgba_pixel<32> value) {
        convert(value,&m_face_color);
        m_face_dirty = true;
        this->invalidate();
    }
    gfx::rgba_pixel<32> tick_color() const {
        gfx::rgba_pixel<32> result;
        convert(m_tick_color,&result);
        return result;
    }
    void tick_color(gfx::rgba_pixel<32> value) {
        convert(value,&m_tick_color);
        m_face_dirty = true;
        this->invalidate();
    }
    uint16_t tick_border_width() const {
        return m_tick_border_width;
    }
    void tick_border_width(uint16_t value) {
        m_tick_border_width = value;
        m_face_dirty = true;
        this->invalidate();
    }
    gfx::rgba_pixel<32> minute_color() const {
        gfx::rgba_pixel<32> result;
        convert(m_minute_color,&result);
        return result;
    }
    void minute_color(gfx::rgba_pixel<32> value) {
        convert(value,&m_minute_color);
        this->invalidate();
    }
    gfx::rgba_pixel<32> minute_border_color() const {
        gfx::rgba_pixel<32> result;
        convert(m_minute_border_color,&result);
        return result;
    }
    void minute_border_color(gfx::rgba_pixel<32> value) {
        convert(value,&m_minute_border_color);
        this->invalidate();
    }
    uint16_t minute_border_width() const {
        return m_minute_border_width;
    }
    void minute_border_width(uint16_t value) {
        m_minute_border_width = value;
        this->invalidate();
    }
    gfx::rgba_pixel<32> hour_color() const {
        gfx::rgba_pixel<32> result;
        convert(m_hour_color,&result);
        return result;
    }
    void hour_color(gfx::rgba_pixel<32> value) {
        convert(value,&m_hour_color);
        this->invalidate();
    }
    gfx::rgba_pixel<32> hour_border_color() const {
        gfx::rgba_pixel<32> result;
        convert(m_hour_border_color,&result);
        return result;
    }
    void hour_border_color(gfx::rgba_pixel<32> value) {
        convert(value,&m_hour_border_color);
        this->invalidate();
    }
    uint16_t hour_border_width() const {
        return m_hour_border_width;
    }
    void hour_border_width(uint16_t value) {
        m_hour_border_width = value;
        this->invalidate();
    }
    gfx::rgba_pixel<32> second_color() const {
        gfx::rgba_pixel<32> result;
        convert(m_second_color,&result);
        return result;
    }
    void second_color(gfx::rgba_pixel<32> value) {
        convert(value,&m_second_color);
        this->invalidate();
    }
    gfx::rgba_pixel<32> second_border_color() const {
        gfx::rgba_pixel<32> result;
        convert(m_second_border_color,&result);
        return result;
    }
    void second_border_color(gfx::rgba_pixel<32> value) {
        convert(value,&m_second_border_color);
        this->invalidate();
    }
    uint16_t second_border_width() const {
        return m_second_border_width;
    }
    void second_border_width(uint16_t value) {
        m_second_border_width = value;
        this->invalidate();
    }
    time_t time() const {
        return m_time;
    }
    void time(time_t value) {
        m_time = value;
        this->invalidate();
    }
    bool buffer_face() const {
        return m_buffer_face;
    }
    void buffer_face(bool value) {
        if(value==false) {
            if(m_buffer_face) {
                if(m_face_buffer.begin()) {
                    free(m_face_buffer.begin());
                }
            }
        }
        m_buffer_face = value;
    }
};
// the control template instantiation aliases
using vclock_t = vclock<surface_t>;
using label_t = uix::label<surface_t>;
using painter_t = uix::painter<surface_t>;

// the screen/control declarations
extern screen_t main_screen;
extern vclock_t ana_clock;
extern label_t weekday;
extern label_t dig_clock;
extern label_t timezone;
extern painter_t wifi_icon;
extern painter_t battery_icon;
