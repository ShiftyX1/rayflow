#include "utils.hpp"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <sstream>

namespace engine::scripting::api {

std::string lua_args_to_string(sol::variadic_args va, sol::this_state ts) {
    sol::state_view lua(ts);
    std::ostringstream oss;

    bool first = true;
    for (auto v : va) {
        if (!first) oss << "\t";
        first = false;

        sol::protected_function tostring = lua["tostring"];
        if (tostring.valid()) {
            auto result = tostring(v);
            if (result.valid()) {
                oss << result.get<std::string>();
            }
        }
    }
    return oss.str();
}

} // namespace engine::scripting::api
