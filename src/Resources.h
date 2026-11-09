#pragma once

#include <string_view>

namespace resources {

// The text of a file built into the executable by Nardy3D.rc.
std::string_view Text(const wchar_t* name);

} // namespace resources
