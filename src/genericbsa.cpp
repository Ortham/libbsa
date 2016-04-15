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
#include <boost/filesystem.hpp>
#include <boost/crc.hpp>

namespace fs = boost::filesystem;

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
    for (size_t i = 0; i < extAssetsNum; i++)
        delete[] extAssets[i];
    delete[] extAssets;
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

void _bsa_handle_int::GetMatchingAssets(const regex& regex, std::list<BsaAsset>& matchingAssets) {
    matchingAssets.clear();
    for (std::list<BsaAsset>::iterator it = assets.begin(), endIt = assets.end(); it != endIt; ++it) {
        if (regex_match(it->path, regex))
            matchingAssets.push_back(*it);
    }
}

void _bsa_handle_int::Extract(const std::string& assetPath, uint8_t** _data, size_t* _size) {
    //Get asset data.
    BsaAsset data = GetAsset(assetPath);
    if (data.path.empty())
        throw error(LIBBSA_ERROR_FILESYSTEM_ERROR, "Path is empty.");

    std::pair<uint8_t*, size_t> dataPair;
    try {
        //Read file data.
        boost::filesystem::ifstream in(fs::path(filePath), ios::binary);
        in.exceptions(ios::failbit | ios::badbit | ios::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

        dataPair = ReadData(in, data);

        *_data = dataPair.first;
        *_size = dataPair.second;

        in.close();
    }
    catch (ios_base::failure& e) {
        delete[] dataPair.first;
        throw error(LIBBSA_ERROR_FILESYSTEM_ERROR, e.what());
    }
}

void _bsa_handle_int::Extract(const std::string& assetPath, const std::string& outPath, const bool overwrite) {
    //Get asset data.
    BsaAsset data = GetAsset(assetPath);
    if (data.path.empty())
        throw error(LIBBSA_ERROR_FILESYSTEM_ERROR, "Path is empty.");

    std::pair<uint8_t*, size_t> dataPair;
    std::string outFilePath = outPath + '/' + data.path;
    try {
        //Create parent directories.
        fs::create_directories(fs::path(outFilePath).parent_path());  //This creates any directories in the path that don't already exist.

        if (!overwrite && fs::exists(outFilePath))
            throw error(LIBBSA_ERROR_FILESYSTEM_ERROR, "The file \"" + outFilePath + "\" already exists.");

        //Read file data.
        boost::filesystem::ifstream in(fs::path(filePath), ios::binary);
        in.exceptions(ios::failbit | ios::badbit | ios::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

        dataPair = ReadData(in, data);

        in.close();

        //Write new file.
        boost::filesystem::ofstream out(fs::path(outFilePath), ios::binary | ios::trunc);
        out.exceptions(ios::failbit | ios::badbit | ios::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

        out.write((char*)dataPair.first, dataPair.second);

        out.close();

        //Free data in memory.
        delete[] dataPair.first;
    }
    catch (ios_base::failure& e) {
        delete[] dataPair.first;
        throw error(LIBBSA_ERROR_FILESYSTEM_ERROR, e.what());
    }
}

void _bsa_handle_int::Extract(const list<BsaAsset>& assetsToExtract, const std::string& outPath, const bool overwrite) {
    std::pair<uint8_t*, size_t> dataPair;
    try {
        //Open the source BSA.
        boost::filesystem::ifstream in(fs::path(filePath), ios::binary);
        in.exceptions(ios::failbit | ios::badbit | ios::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

        //Loop through the map, checking that each path doesn't already exist, creating path components if necessary, and extracting files.
        for (list<BsaAsset>::const_iterator it = assetsToExtract.begin(), endIt = assetsToExtract.end(); it != endIt; ++it) {
            std::string outFilePath = outPath + '/' + it->path;

            //Create parent directories.
            fs::create_directories(fs::path(outPath).parent_path());  //This creates any directories in the path that don't already exist.

            if (!overwrite && fs::exists(outPath))
                throw error(LIBBSA_ERROR_FILESYSTEM_ERROR, "The file \"" + outPath + "\" already exists.");

            //Get file data.
            dataPair = ReadData(in, *it);

            //Write new file.
            boost::filesystem::ofstream out(fs::path(outPath) / it->path, ios::binary | ios::trunc);
            out.exceptions(ios::failbit | ios::badbit | ios::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

            out.write((char*)dataPair.first, dataPair.second);

            out.close();

            //Free data in memory.
            delete[] dataPair.first;
        }

        in.close();
    }
    catch (ios_base::failure& e) {
        delete[] dataPair.first;
        throw error(LIBBSA_ERROR_FILESYSTEM_ERROR, e.what());
    }
}

uint32_t _bsa_handle_int::CalcChecksum(const std::string& assetPath) {
    //Get asset data.
    BsaAsset data = GetAsset(assetPath);
    if (data.path.empty())
        throw error(LIBBSA_ERROR_FILESYSTEM_ERROR, "Path is empty.");

    std::pair<uint8_t*, size_t> dataPair;
    try {
        //Open input stream.
        boost::filesystem::ifstream in(fs::path(filePath), ios::binary);
        in.exceptions(ios::failbit | ios::badbit | ios::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

        dataPair = ReadData(in, data);

        in.close();

        //Calculate the checksum now.
        boost::crc_32_type result;
        result.process_bytes(dataPair.first, dataPair.second);

        delete[] dataPair.first;

        return result.checksum();
    }
    catch (ios_base::failure& e) {
        delete[] dataPair.first;
        throw error(LIBBSA_ERROR_FILESYSTEM_ERROR, e.what());
    }
}
