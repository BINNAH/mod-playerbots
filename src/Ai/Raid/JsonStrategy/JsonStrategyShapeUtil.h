/*
 * JSON-driven raid strategy — tiny shared parsing helpers for shape qualifiers.
 * Header-only (only <string>/<vector>) so it adds no translation unit.
 */
#ifndef _PLAYERBOT_JSONSTRATEGYSHAPEUTIL_H
#define _PLAYERBOT_JSONSTRATEGYSHAPEUTIL_H

#include <string>
#include <vector>

// Split `s` on `delim`, trimming nothing. Empty tokens are skipped.
inline std::vector<std::string> JsonSplit(std::string const& s, char delim)
{
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= s.size())
    {
        size_t pos = s.find(delim, start);
        if (pos == std::string::npos)
        {
            if (start < s.size())
                out.push_back(s.substr(start));
            break;
        }
        if (pos > start)
            out.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return out;
}

// Look up "key=value" in a '|'-separated qualifier string. Returns `def` if the
// key is absent. Value may contain spaces / apostrophes (boss & aura names do).
inline std::string JsonKv(std::string const& qualifier, std::string const& key, std::string const& def = "")
{
    std::string needle = key + "=";
    for (std::string const& field : JsonSplit(qualifier, '|'))
    {
        if (field.rfind(needle, 0) == 0)  // field starts with "key="
            return field.substr(needle.size());
    }
    return def;
}

#endif
