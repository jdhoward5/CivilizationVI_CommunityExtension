#pragma once
#include "HavokScript.h"
#include <unordered_map>
#include <string>
#include <variant>

namespace Data {
    struct LuaVariant : public std::variant<std::string, double, int> {
        using std::variant<std::string, double, int>::variant;
        void push(hks::lua_State* L) const;
    };

    struct LuaVariantMap : public std::unordered_map<std::string, LuaVariant> {
        LuaVariantMap() = default;
        void rebuild(hks::lua_State* L);
    };
}