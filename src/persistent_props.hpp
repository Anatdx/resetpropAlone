#pragma once

#include <map>
#include <optional>
#include <string>

namespace resetprop {

using PersistentPropertyMap = std::map<std::string, std::string>;

bool get_persistent_property(const std::string& name,
                             std::string* value,
                             bool* found,
                             std::string* error);

bool list_persistent_properties(PersistentPropertyMap* properties, std::string* error);

bool set_persistent_property(const std::string& name,
                             const std::string& value,
                             std::string* error);

bool delete_persistent_property(const std::string& name,
                                bool* deleted,
                                std::string* error);

}  // namespace resetprop
