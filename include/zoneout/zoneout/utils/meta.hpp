#pragma once

#include "uuid.hpp"
#include <string>

namespace zoneout {

    struct Meta {
        UUID id;
        std::string name;
        std::string type;
        std::string subtype;

        Meta() : id(generateUUID()), name(""), type(""), subtype("default") {}

        Meta(const std::string &name_, const std::string &type_, const std::string &subtype_ = "default")
            : id(generateUUID()), name(name_), type(type_), subtype(subtype_) {}

        Meta(const UUID &id_, const std::string &name_, const std::string &type_,
             const std::string &subtype_ = "default")
            : id(id_), name(name_), type(type_), subtype(subtype_) {}
    };

} // namespace zoneout
