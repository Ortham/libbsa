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

#include "helpers.h"
#include "libbsa.h"
#include "error.h"
#include "streams.h"
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <boost/crc.hpp>
#include <boost/locale.hpp>

using namespace std;

namespace libbsa {

    // std::string to null-terminated char string converter.
    char * ToNewCString(const std::string& str) {
        char * p = new char[str.length() + 1];
        return strcpy(p, str.c_str());
    }

    //Replaces all forwardslashes with backslashes, and lowercases letters.
    string FixPath(const char * path) {
        string out = string(reinterpret_cast<const char*>(path));
        boost::to_lower(out);

        boost::replace_all(out, "/", "\\");

        if (out[0] == '\\')
            out = out.substr(1);

        return out;
    }

    //Calculate the CRC of the given file for comparison purposes.
    uint32_t GetCrc32(const string& filename) {
        uint32_t chksum = 0;
        static const size_t buffer_size = 8192;
        char buffer[buffer_size];
        libbsa::ifstream ifile(filename.c_str(), ios::binary);
        boost::crc_32_type result;
        if (ifile.good()) {
            do {
                ifile.read(buffer, buffer_size);
                result.process_bytes(buffer, ifile.gcount());
            } while (ifile);
            chksum = result.checksum();
        }
        return chksum;
    }

    std::string ToUTF8(const std::string& str) {
        try {
            return boost::locale::conv::to_utf<char>(str, "Windows-1252", boost::locale::conv::stop);
        } catch (boost::locale::conv::conversion_error& e) {
            throw error(LIBBSA_ERROR_BAD_STRING, "\"" + str + "\" cannot be encoded in Windows-1252.");
        }
    }

    std::string FromUTF8(const std::string& str) {
        try {
            return boost::locale::conv::from_utf<char>(str, "Windows-1252", boost::locale::conv::stop);
        } catch (boost::locale::conv::conversion_error& e) {
            throw error(LIBBSA_ERROR_BAD_STRING, "\"" + str + "\" cannot be encoded in Windows-1252.");
        }
    }
}
