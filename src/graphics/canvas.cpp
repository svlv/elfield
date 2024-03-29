#include "canvas.hpp"

#include "constants.hpp"
#include "context_guard.hpp"
#include "graphics/arrow.hpp"
#include "graphics/chevron.hpp"
#include "graphics/curve.hpp"
#include "graphics/line.hpp"
#include "graphics/square.hpp"
#include "graphics/widgets/charge_props.hpp"
#include "point.hpp"
#include "utils.hpp"

#include <gdkmm/pixbuf.h>
#include <gtkmm.h>
#include <gtkmm/clipboard.h>
#include <iostream>

namespace elfield
{

canvas::canvas() : _field(_charges), _selected_circle(nullptr)
{
    add_events(Gdk::BUTTON_PRESS_MASK);
    add_events(Gdk::BUTTON_RELEASE_MASK);
    add_events(Gdk::POINTER_MOTION_MASK);
    add_events(Gdk::KEY_PRESS_MASK);
    add_events(Gdk::SCROLL_MASK);
    property_can_focus() = true;
}

void canvas::reinit_field() { _init_lines(); }

void canvas::_init_arrows(int width, int height)
{
    _arrows.clear();
    auto pos = point(arrow_delta / 2.0, arrow_delta / 2.0);

    while (pos.x + arrow_delta / 2.0 < width) {
        while (pos.y + arrow_delta / 2.0 < height) {
            _arrows.emplace_back(default_arrow_size, pos, 0.0);
            pos.y += arrow_delta;
        }
        pos.x += arrow_delta;
        pos.y = arrow_delta / 2.0;
    }
}

base_line_uptr canvas::_make_line(point pos, bool positive, const size& sz)
{
    const auto valid_position = [&pos, &sz]() {
        return pos.x > 0 && pos.x < sz.width && pos.y > 0 && pos.y < sz.height;
    };
    auto crv = [&]() -> base_line_uptr {
        switch (_line_type) {
        case base_line::type::line:
            return std::make_unique<line>(pos);
        case base_line::type::curve:
            return std::make_unique<curve>(pos);
        }
        return nullptr;
    }();
    double chevron_step = 0.0;
    for (size_t i = 0; i < 1000 && valid_position(); ++i) {
        auto end = positive
                       ? _charges.get_hint(pos, charge::type::negative, 10.0)
                       : _charges.get_hint(pos, charge::type::positive, 10.0);
        if (end) {
            const auto& coord = end->get_coord();
            crv->add_point(coord);
            break;
        }
        const auto delta = point(_field.get_cos(pos) * _line_delta,
                                 _field.get_sin(pos) * _line_delta);
        const auto mod = delta.module();
        if (mod < 1.0) {
            break;
        }
        if (positive) {
            pos += delta;
        } else {
            pos -= delta;
        }
        crv->add_point(pos);
        chevron_step += mod;
        if (chevron_step >= total_chevron_step) {
            chevron_step = 0.0;
            crv->add_chevron(
                chevron(default_chevron_size, pos, _field.get_angle(pos)));
        }
    }
    crv->fill();
    return crv;
}

void canvas::_init_lines(std::optional<Gtk::Allocation> allocation)
{
    if (!allocation.has_value()) {
        allocation = get_allocation();
    }
    const auto sz = size(allocation->get_width(), allocation->get_height());
    _lines.clear();
    const auto get_begin = [](const point& coord, size_t idx,
                              size_t count) -> point {
        const double angle = idx * 2 * M_PI / count;
        return point(coord.x + cos(angle) * line_delta,
                     coord.y + sin(angle) * line_delta);
    };
    for (const auto& charge : _charges.get_positive_charges()) {
        if (charge->get_value() > 0.0) {
            const size_t count =
                std::abs(charge->get_value()) * lines_per_charge;
            for (size_t idx = 0; idx < count; ++idx) {
                _lines.emplace_back(_make_line(
                    get_begin(charge->get_coord(), idx, count), true, sz));
            }
        }
    }
    for (const auto& charge : _charges.get_negative_charges()) {
        if (charge->get_value() < 0.0) {
            const size_t count =
                std::abs(charge->get_value()) * lines_per_charge;
            for (size_t idx = 0; idx < count; ++idx) {
                _lines.emplace_back(_make_line(
                    get_begin(charge->get_coord(), idx, count), false, sz));
            }
        }
    }
}

void canvas::_draw_arrows(const size& sz,
                          const Cairo::RefPtr<Cairo::Context>& cr)
{
    if (_draw_arrows_flag && !_charges.empty()) {
        const auto guard = context_guard(cr);
        Gdk::Cairo::set_source_rgba(cr, arrow_color);

        for (auto& arr : _arrows) {
            const auto& coord = arr.get_coord();
            arr.rotate(_field.get_angle(coord));

            if (arr.is_selected()) {
                const context_guard guard(cr);
                Gdk::Cairo::set_source_rgba(cr, highlight_arrow_color);
                {
                    const auto guard = context_guard(cr);
                    cr->set_line_width(line_width);
                    _make_line(coord, true, sz)->draw(cr);
                    _make_line(coord, false, sz)->draw(cr);
                }
                arr.draw(cr, fill_arrow);
                continue;
            }

            arr.draw(cr, fill_arrow);
        }
    }
}

void canvas::_draw_lines(const Cairo::RefPtr<Cairo::Context>& cr)
{
    if (_draw_lines_flag && !_lines.empty()) {
        const auto guard = context_guard(cr);
        cr->set_line_width(line_width);
        for (const auto& line : _lines) {
            line->draw(cr);
        }
    }
}

void canvas::_draw_charges(const Cairo::RefPtr<Cairo::Context>& ctx)
{
    for (const auto& circle : _circles) {
        circle.draw(ctx);
    }
}

void canvas::_draw_background(const Cairo::RefPtr<Cairo::Context>& ctx)
{
    const auto guard = context_guard(ctx);
    Gdk::Cairo::set_source_rgba(ctx, _settings.background_color);
    ctx->paint();
}

void canvas::set_background_color(Gdk::RGBA bg_color)
{
    _settings.background_color = bg_color;
    queue_draw();
}

void canvas::_draw_potential(const Cairo::RefPtr<Cairo::Context>& ctx)
{
    if (_charges.empty() || !_draw_potential_flag) {
        return;
    }
    const auto guard = context_guard(ctx);
    const double delta = 10.0;
    Gtk::Allocation allocation = get_allocation();
    const auto sz = size(allocation.get_width(), allocation.get_height());
    size_t x_count = sz.width / delta + 1;
    size_t y_count = sz.height / delta + 1;

    const auto get_color = [&](double value) {
        const double min_e = -0.01;
        const double max_e = 0.01;
        Gdk::RGBA color;
        if (value < 0.0) {
            auto portion = value / min_e;
            if (portion > 1.0)
                portion = 1.0;
            color.set_rgba(0.0, 1 - portion, portion, .7);
            return color;
        } else {
            auto portion = value / max_e;
            if (portion > 1.0)
                portion = 1.0;
            color.set_rgba(portion, 1 - portion, 0.0, .7);
        }
        return color;
    };

    point coord(0.0, 0.0);
    for (size_t x_idx = 0; x_idx < x_count; ++x_idx) {
        for (size_t y_idx = 0; y_idx < y_count; ++y_idx) {
            const auto e = _field.get_potential(coord);
            square sq(coord, get_color(e));
            sq.draw(ctx);
            coord.y += delta;
        }
        coord.x += delta;
        coord.y = 0.0;
    }
}

bool canvas::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    Gtk::Allocation allocation = get_allocation();
    const auto sz = size(allocation.get_width(), allocation.get_height());

    _draw_background(cr);
    _draw_potential(cr);
    _draw_arrows(sz, cr);
    _draw_lines(cr);
    _draw_charges(cr);

    return true;
}

bool canvas::on_button_press_event(GdkEventButton* event)
{
    if (event->type == GDK_BUTTON_PRESS) {
        const auto coord = point(event->x, event->y);
        if (event->button == 1) {
            _selected_circle = get_hint_circle(coord);
            if (!_selected_circle) {
                _charges.emplace_back(charge::type::positive, coord, 1.0);
                _circles.emplace_back(_charges.get_positive_charges().back());
                _circles.back().select(true);
                for (auto& arr : _arrows) {
                    arr.select(false);
                }
                reinit_field();
                queue_draw();
            }
        } else if (event->button == 3) {
            _charges.emplace_back(charge::type::negative, coord, -1.0);
            _circles.emplace_back(_charges.get_negative_charges().back());
            _circles.back().select(true);
            for (auto& arr : _arrows) {
                arr.select(false);
            }
            reinit_field();
            queue_draw();
        }
    } else if (event->type == GDK_DOUBLE_BUTTON_PRESS) {
        _selected_circle = nullptr;
        const auto coord = point(event->x, event->y);
        Gtk::Window* parent = dynamic_cast<Gtk::Window*>(this->get_toplevel());
        const auto* circle = get_hint_circle(coord);
        if (parent && circle) {
            auto props = charge_props(*parent, circle->get_charge(), *this);
            props.run();
            _charges.validate(circle->get_charge());
            reinit_field();
            queue_draw();
        }
    }
    return false;
}

bool canvas::on_scroll_event(GdkEventScroll* scroll_event)
{
    const auto coord = point(scroll_event->x, scroll_event->y);
    auto* hint_circle = get_hint_circle(coord);
    auto chrg = hint_circle ? hint_circle->get_charge() : charge_ptr();
    if (chrg) {
        if (scroll_event->direction == GdkScrollDirection::GDK_SCROLL_UP) {
            chrg->get_value() += 1.0;
            _charges.validate(chrg);
            reinit_field();
            queue_draw();
        } else if (scroll_event->direction ==
                   GdkScrollDirection::GDK_SCROLL_DOWN) {
            chrg->get_value() -= 1.0;
            _charges.validate(chrg);
            reinit_field();
            queue_draw();
        }
    }
    return false;
}

bool canvas::on_button_release_event(GdkEventButton* event)
{
    _selected_circle = nullptr;
    return false;
}

bool canvas::on_key_press_event(GdkEventKey* event)
{
    if (event->keyval == GDK_KEY_l) {
        _draw_lines_flag = !_draw_lines_flag;
        queue_draw();
    } else if (event->keyval == GDK_KEY_a) {
        _draw_arrows_flag = !_draw_arrows_flag;
        queue_draw();
    } else if (event->keyval == GDK_KEY_t) {
        _line_type = _line_type == base_line::type::line
                         ? base_line::type::curve
                         : base_line::type::line;
        _init_lines();
        queue_draw();
        std::cout << "Line type has been changed to " << (int)_line_type
                  << std::endl;
    } else if (event->keyval == GDK_KEY_j && _line_delta > 2.0) {
        _line_delta -= 2.;
        _init_lines();
        queue_draw();
        std::cout << "Delta line has been changed to " << _line_delta
                  << std::endl;
    } else if (event->keyval == GDK_KEY_k && _line_delta <= 20.0) {
        _line_delta += 2.;
        _init_lines();
        queue_draw();
        std::cout << "Delta line has been changed to " << _line_delta
                  << std::endl;
    } else if (event->keyval == GDK_KEY_x) {
        const auto it = std::find_if(
            _circles.cbegin(), _circles.cend(),
            [](const auto& circle) { return circle.is_selected(); });
        if (it != _circles.cend()) {
            _charges.erase(it->get_charge());
            _circles.erase(it);
            _selected_circle = nullptr;
            reinit_field();
            queue_draw();
        }
    } else if (event->keyval == GDK_KEY_p) {
        _draw_potential_flag = !_draw_potential_flag;
        queue_draw();
    } else if (event->keyval == GDK_KEY_s &&
               (event->state & GDK_CONTROL_MASK)) {
        save_to_png();
    } else if (event->keyval == GDK_KEY_c &&
               (event->state & GDK_CONTROL_MASK)) {
        copy();
    } else if (event->keyval == GDK_KEY_c) {
        clear();
        queue_draw();
    }
    return false;
}

void canvas::copy()
{
    if (const auto window = this->get_window()) {
        if (auto clipboard = Gtk::Clipboard::get()) {
            if (auto shot = Gdk::Pixbuf::create(
                    window, 0, 0, window->get_width(), window->get_height())) {
                clipboard->set_image(shot);
            }
        }
    }
}

void canvas::clear()
{
    _charges.clear();
    _circles.clear();
    _lines.clear();
    _selected_circle = nullptr;
}

void canvas::save_to_png()
{
    if (const auto window = this->get_window()) {
        if (const auto context = window->create_cairo_context()) {
            if (const auto target = context->get_target()) {
                std::time_t t = std::time(0);
                if (std::tm* now = std::localtime(&t)) {
                    char str[30];
                    sprintf(str, "Elfield_%i%02i%02i_%02i%02i%02i.png",
                            now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,
                            now->tm_hour, now->tm_min, now->tm_sec);
                    target->write_to_png(str);
                }
            }
        }
    }
}

bool canvas::on_motion_notify_event(GdkEventMotion* event)
{
    const auto coord = point(event->x, event->y);
    if (_selected_circle) {
        _selected_circle->move(coord);
        reinit_field();
    } else {
        bool flg = false;
        for (auto& circle : _circles) {
            bool is_hint = circle.is_hint(coord);
            flg = flg || is_hint;
            circle.select(is_hint);
        }
        for (auto& arr : _arrows) {
            arr.select(!flg && arr.is_hint(coord));
        }
    }
    queue_draw();
    return false;
}

void canvas::on_size_allocate(Gtk::Allocation& allocation)
{
    _init_arrows(allocation.get_width(), allocation.get_height());
    _init_lines(allocation);
    Gtk::DrawingArea::on_size_allocate(allocation);
}

circle* canvas::get_hint_circle(const point& coord)
{
    auto it = std::find_if(
        _circles.begin(), _circles.end(),
        [&coord](const auto& circle) { return circle.is_hint(coord); });
    if (it != _circles.end()) {
        return &(*it);
    }
    return nullptr;
}

} // namespace elfield
