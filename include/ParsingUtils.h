#ifndef CLAYG_PARSINGUTILS_H
#define CLAYG_PARSINGUTILS_H

#include <string>
#include <utility>
#include <vector>

namespace ParsingUtils
{
std::vector<std::pair<std::string, std::string>> parse_key_value_list(
    const std::string& input,
    char pair_separator = ',',
    char key_value_separator = '=');
}

#endif //CLAYG_PARSINGUTILS_H
