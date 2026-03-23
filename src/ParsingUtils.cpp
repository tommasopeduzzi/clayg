#include "ParsingUtils.h"

#include <sstream>
#include <stdexcept>

namespace
{
void trim_in_place(std::string& str)
{
    const size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos)
    {
        str.clear();
        return;
    }
    const size_t last = str.find_last_not_of(" \t\n\r");
    str = str.substr(first, last - first + 1);
}
}

std::vector<std::pair<std::string, std::string>> ParsingUtils::parse_key_value_list(
    const std::string& input,
    const char pair_separator,
    const char key_value_separator)
{
    std::vector<std::pair<std::string, std::string>> pairs;
    std::stringstream ss(input);
    std::string token;

    while (getline(ss, token, pair_separator))
    {
        trim_in_place(token);
        if (token.empty())
            continue;

        const size_t eq = token.find(key_value_separator);
        if (eq == std::string::npos)
        {
            throw std::invalid_argument("Invalid token (expected KEY=VALUE): " + token);
        }

        std::string key = token.substr(0, eq);
        std::string value = token.substr(eq + 1);
        trim_in_place(key);
        trim_in_place(value);
        pairs.emplace_back(std::move(key), std::move(value));
    }

    return pairs;
}
