#pragma once

#include <string>

namespace resetprop {

enum class PropertyContextType {
    Serialized,
    Split,
    PreSplit,
};

const char* property_context_type_name(PropertyContextType type);

bool detect_property_context_type(const std::string& props_root,
                                  PropertyContextType* type,
                                  std::string* error);

bool resolve_property_context(const std::string& name,
                              std::string* context,
                              std::string* error);

bool resolve_property_context_from(const std::string& props_root,
                                   const std::string& name,
                                   std::string* context,
                                   std::string* error);

bool resolve_property_area_path(const std::string& props_root,
                                const std::string& name,
                                std::string* path,
                                std::string* context,
                                std::string* error);

bool resolve_property_serial_path(const std::string& props_root,
                                  std::string* path,
                                  std::string* error);

}  // namespace resetprop
