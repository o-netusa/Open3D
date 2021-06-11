// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2021 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#include "open3d/visualization/gui/ListView.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

#include "open3d/visualization/gui/Theme.h"
#include "open3d/visualization/gui/Util.h"

namespace open3d {
namespace visualization {
namespace gui {

namespace {
static const int NO_SELECTION = -1;
static int g_next_list_box_id = 1;

/// The definition should match that of FileDialog.h
enum SelectionMode_ {
    DISABLE       = 0,             // view-only, no item can be selected
    SINGLE_SELECT = 1 << 0,        // only one item can be selected
    MULTI_SELECT  = 1 << 1,        // multiple items can be selected
    DESELECTABLE  = 1 << 2,        // selected item can be de-selected
};

}  // namespace

struct ListView::Impl {
    std::string imgui_id_;
    std::vector<std::string> items_;
    SelectionMode_ selection_mode_ = SelectionMode_::SINGLE_SELECT;  // default
    std::vector<int> selected_indices_;
    int last_selected_index_ = NO_SELECTION; // for shift multi-selection
    bool auto_scroll_y{false};
    std::function<void(std::vector<const char *>, bool)> on_value_changed_;
};

ListView::ListView() : impl_(new ListView::Impl()) {
    impl_->imgui_id_ = "##listview_" + std::to_string(g_next_list_box_id++);
}

ListView::~ListView() {}

void ListView::SetItems(const std::vector<std::string> &items) {
    impl_->items_ = items;
    impl_->selected_indices_.clear();
}

void ListView::SetSelectionMode(int mode) {
    impl_->selection_mode_ = static_cast<SelectionMode_>(mode);
}

std::vector<int> ListView::GetSelectedIndices() const {
    return impl_->selected_indices_;
}

std::vector<const char *> ListView::GetSelectedValues() const {
    std::vector<const char*> results;
    for (int idx : impl_->selected_indices_) {
        results.push_back(impl_->items_[idx].c_str());
    }
    return results;
}

void ListView::SetSelectedIndex(int index) {
    impl_->selected_indices_.clear();
    if (index >= 0) {
        impl_->selected_indices_.push_back(std::min(int(impl_->items_.size() - 1), index));
    }
}

void ListView::AddSelectedIndex(int index) {
    int idx = std::max(0, std::min(int(impl_->items_.size() - 1), index));
    auto itor = std::find(impl_->selected_indices_.begin(), impl_->selected_indices_.end(), idx);
    if (itor == impl_->selected_indices_.end()) {
        impl_->selected_indices_.push_back(idx);
        std::sort(impl_->selected_indices_.begin(), impl_->selected_indices_.end());
    }
}

void ListView::SetOnValueChanged(
        std::function<void(std::vector<const char *>, bool)> on_value_changed) {
    impl_->on_value_changed_ = on_value_changed;
}

Size ListView::CalcPreferredSize(const LayoutContext &context,
                                 const Constraints &constraints) const {
    auto padding = ImGui::GetStyle().FramePadding;
    auto *font = ImGui::GetFont();
    ImVec2 size(0, 0);

    for (auto &item : impl_->items_) {
        auto item_size = font->CalcTextSizeA(float(context.theme.font_size),
                                             float(constraints.width), 0.0,
                                             item.c_str());
        size.x = std::max(size.x, item_size.x);
        size.y += ImGui::GetFrameHeight();
    }
    return Size(int(std::ceil(size.x + 2.0f * padding.x)), Widget::DIM_GROW);
}

Size ListView::CalcMinimumSize(const LayoutContext &context) const {
    return Size(0, 3 * context.theme.font_size);
}

Widget::DrawResult ListView::Draw(const DrawContext &context) {
    auto &frame = GetFrame();
    ImGui::SetCursorScreenPos(
            ImVec2(float(frame.x), float(frame.y) + ImGui::GetScrollY()));
    ImGui::PushItemWidth(float(frame.width));

    ImGui::PushStyleColor(ImGuiCol_FrameBg,
                          colorToImgui(context.theme.list_background_color));
    ImGui::PushStyleColor(ImGuiCol_Header,  // selection color
                          colorToImgui(context.theme.list_selected_color));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,  // hover color
                          colorToImgui(Color(0, 0, 0, 0)));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,  // click-hold color
                          colorToImgui(context.theme.list_selected_color));

    int height_in_items =
            int(std::floor(frame.height / ImGui::GetFrameHeight()));

    auto result = Widget::DrawResult::NONE;
    std::vector<int> selected_indices = impl_->selected_indices_;
    bool is_double_click = false;
    DrawImGuiPushEnabledState();
    if (ImGui::ListBoxHeader(impl_->imgui_id_.c_str(),
                             int(impl_->items_.size()), height_in_items)) {
        for (size_t i = 0; i < impl_->items_.size(); ++i) {
	    auto itor = std::find(impl_->selected_indices_.begin(),
                                  impl_->selected_indices_.end(), i);
            bool is_selected = (itor != impl_->selected_indices_.end());
            // ImGUI's list wants to hover over items, which is not done by
            // any major OS, is pretty unnecessary (you can see the cursor
            // right over the row), and acts really weird. Worse, the hover
            // is drawn instead of the selection color. So to get rid of it
            // we need hover to be the selected color iff this item is
            // selected, otherwise we want it to be transparent.
            if (is_selected) {
                ImGui::PushStyleColor(
                        ImGuiCol_HeaderHovered,
                        colorToImgui(context.theme.list_selected_color));
            } else {
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                                      colorToImgui(Color(0, 0, 0, 0)));
            }
            ImGuiSelectableFlags flag = (impl_->selection_mode_ == SelectionMode_::DISABLE) ?
                ImGuiSelectableFlags_Disabled : ImGuiSelectableFlags_AllowDoubleClick;
            if (ImGui::Selectable(impl_->items_[i].c_str(), &is_selected, flag)) {
                // Dear ImGUI seems to have a bug where it registers a
                // double-click as long as you haven't moved the mouse,
                // no matter how long the time between clicks was.
                if (ImGui::IsMouseDoubleClicked(0)) {
                    is_double_click = true;
                }
                if (ImGui::GetIO().KeyCtrl && (impl_->selection_mode_ & SelectionMode_::MULTI_SELECT)) {
                    auto it = std::find(selected_indices.begin(), selected_indices.end(), i);
                    if (is_selected && it == selected_indices.end()) {
                        selected_indices.push_back(i);
                        std::sort(selected_indices.begin(), selected_indices.end());
                    }
                    else if (!is_selected && it != selected_indices.end()) {
                        selected_indices.erase(it);
                    }
                }
                else if (ImGui::GetIO().KeyShift && (impl_->selection_mode_ & SelectionMode_::MULTI_SELECT)) {
                    selected_indices.clear();
                    if (impl_->last_selected_index_ != NO_SELECTION) {
                        int beg = std::min(int(i), impl_->last_selected_index_);
                        int end = std::max(int(i), impl_->last_selected_index_);
                        for (int n = beg; n <= end; ++n) {
                            selected_indices.push_back(n);
                        }
                    }
                    else {
                        impl_->last_selected_index_ = int(i);
                    }
                }
                else {
                    selected_indices.clear();
                    if (is_selected || is_double_click)
                    {
                        selected_indices.push_back(int(i));
                        impl_->last_selected_index_ = int(i);
                    }
                }
            }
            ImGui::PopStyleColor();
        }
        ImGui::ListBoxFooter();

	if (is_double_click ||
            selected_indices.size() != impl_->selected_indices_.size() ||
            !std::equal(selected_indices.begin(),
                        selected_indices.end(),
                        impl_->selected_indices_.begin())) {
            impl_->selected_indices_ = selected_indices;
            if (impl_->on_value_changed_) {
                impl_->on_value_changed_(GetSelectedValues(), is_double_click);
            }
            result = Widget::DrawResult::REDRAW;
        }
    }
    DrawImGuiPopEnabledState();

    ImGui::PopStyleColor(4);

    ImGui::PopItemWidth();
    return result;
}

}  // namespace gui
}  // namespace visualization
}  // namespace open3d
