#pragma once

#include "physics/charge.hpp"
#include <gtkmm/dialog.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/entry.h>
#include <gtkmm/frame.h>
#include <gtkmm/label.h>
#include <map>
#include <string>

namespace maxwell
{
class charge_props : public Gtk::Dialog
{
  public:
    explicit charge_props(Gtk::Window& parent, const charge_ptr& charge,
                          Gtk::DrawingArea& area);
    ~charge_props() = default;
    using widgets_t = std::map<std::string, std::unique_ptr<Gtk::Widget>>;

  protected:
    void on_response(int response_id) override;

  private:
    void _on_button_charge_up_click();
    void _on_button_charge_down_click();
    void _on_button_charge_left_click();
    void _on_button_charge_right_click();

    charge_ptr _charge;
    widgets_t _widgets;
    Gtk::Frame _frame;
    std::reference_wrapper<Gtk::DrawingArea> _drawing_area;
};
} // namespace maxwell
