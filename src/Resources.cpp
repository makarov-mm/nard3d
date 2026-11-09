#include "Resources.h"

#include "Win32.h"

#include <stdexcept>

namespace resources {

std::string_view Text(const wchar_t* name)
{
    HRSRC info = FindResourceW(nullptr, name, MAKEINTRESOURCEW(10)); // RT_RCDATA
    HGLOBAL handle = info ? LoadResource(nullptr, info) : nullptr;
    const void* data = handle ? LockResource(handle) : nullptr;
    if (!data)
        throw std::runtime_error("A file is missing from the program's resources: " + win32::Narrow(name));
    return {static_cast<const char*>(data), SizeofResource(nullptr, info)};
}

} // namespace resources
