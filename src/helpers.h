/*  libbsa

    A library for reading and writing BSA files.

    Copyright (C) 2012-2013    WrinklyNinja

    This file is part of libbsa.

    libbsa is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    libbsa is distributed in the hope that it will
    be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with libbsa.  If not, see
    <http://www.gnu.org/licenses/>.
*/

#ifndef __LIBBSA_HELPERS_H__
#define __LIBBSA_HELPERS_H__

#include <string>
#include <boost/unordered_map.hpp>

namespace libbsa {
    // std::string to null-terminated uint8_t string converter.
    char * ToNewCString(const std::string& str);

    //Replaces all forwardslashes with backslashes, and lowercases letters.
    std::string FixPath(const char * path);

    uint32_t GetCrc32(const std::string& filename);

    //Only ever need to convert between Windows-1252 and UTF-8.
    std::string ToUTF8(const std::string& str);
    std::string FromUTF8(const std::string& str);
}

#endif
