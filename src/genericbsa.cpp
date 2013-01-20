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

#include "genericbsa.h"
#include "libbsa.h"
#include "error.h"
#include <fstream>

using namespace std;
using namespace libbsa;

namespace libbsa {
    //////////////////////////////////////////////
    // BsaAsset Constructor
    //////////////////////////////////////////////

    BsaAsset::BsaAsset() : hash(0), size(0), offset(0) {}
}

//////////////////////////////////////////////
// BSA Class Methods
//////////////////////////////////////////////

_bsa_handle_int::_bsa_handle_int(const std::string& path) : filePath(path), extAssets(NULL), extAssetsNum(0) {}

_bsa_handle_int::~_bsa_handle_int() {
    for (size_t i=0; i < extAssetsNum; i++)
        delete [] extAssets[i];
    delete [] extAssets;
}

bool _bsa_handle_int::HasAsset(const std::string& assetPath) {
    for (std::list<BsaAsset>::iterator it = assets.begin(), endIt = assets.end(); it != endIt; ++it) {
        if (it->path == assetPath)
            return true;
    }
    return false;
}

BsaAsset _bsa_handle_int::GetAsset(const std::string& assetPath) {
    for (std::list<BsaAsset>::iterator it = assets.begin(), endIt = assets.end(); it != endIt; ++it) {
        if (it->path == assetPath)
            return *it;
    }
    BsaAsset ba;
    return ba;
}

void _bsa_handle_int::GetMatchingAssets(const boost::regex& regex, std::list<BsaAsset>& matchingAssets) {
    matchingAssets.clear();
    for (std::list<BsaAsset>::iterator it = assets.begin(), endIt = assets.end(); it != endIt; ++it) {
        if (boost::regex_match(it->path, regex))
            matchingAssets.push_back(*it);
    }
}

void _bsa_handle_int::Extract(const std::string& assetPath, const std::string& outPath, const bool overwrite) {
    //Get asset data.
    BsaAsset data = GetAsset(assetPath);
    if (data.path.empty())
        throw error(LIBBSA_ERROR_FILESYSTEM_ERROR, "Path is empty.");

    try {
        //Open input stream.
        ifstream in(filePath.c_str(), ios::binary);
        in.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

        ExtractFromStream(in, data, outPath + '/' + data.path, overwrite);

        in.close();
    } catch (ios_base::failure& e) {
        throw error(LIBBSA_ERROR_FILESYSTEM_ERROR, e.what());
    }
}

void _bsa_handle_int::Extract(const list<BsaAsset>& assetsToExtract, const std::string& outPath, const bool overwrite) {
    try {
        //Open the source BSA.
        ifstream in(filePath.c_str(), ios::binary);
        in.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

        //Loop through the map, checking that each path doesn't already exist, creating path components if necessary, and extracting files.
        for (list<BsaAsset>::const_iterator it = assetsToExtract.begin(), endIt = assetsToExtract.end(); it != endIt; ++it) {
            ExtractFromStream(in, *it, outPath + '/' + it->path, overwrite);
        }

        in.close();
    } catch (ios_base::failure& e) {
        throw error(LIBBSA_ERROR_FILESYSTEM_ERROR, e.what());
    }
}
