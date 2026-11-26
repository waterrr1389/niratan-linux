// SPDX-License-Identifier: GPL-3.0-or-later

#include "reader_view_model_state.h"

#include "services/fonts/font_store.h"

namespace niratan::viewmodels {

QString ReaderViewModel::font_face_css() const {
    return QString::fromStdString(
        fonts::FontStore(state_->storage.app_directory()).font_face_css());
}

std::optional<niratan::reader::SessionResource>
ReaderViewModel::font_resource(const std::string& file_name) {
    const auto resource =
        fonts::FontStore(state_->storage.app_directory()).font_resource(file_name);
    if (!resource.has_value()) {
        return std::nullopt;
    }
    return niratan::reader::SessionResource{resource->bytes, resource->mime_type};
}

}  // namespace niratan::viewmodels
