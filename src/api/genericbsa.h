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

#ifndef __LIBBSA_GENERICBSA_H__
#define __LIBBSA_GENERICBSA_H__

#include "helpers.h"
#include <stdint.h>
#include <string>
#include <list>
#include <regex>

#include <boost/filesystem/fstream.hpp>

/* This header declares the generic structures that libbsa uses to handle BSA
   manipulation.
   All strings are encoded in UTF-8.
*/

namespace libbsa {
    //Class for generic BSA data.
    //Files that have not yet been written have 0 hash, size and offset.
    struct BsaAsset {
        BsaAsset();

        //Asset data obtained from BSA.
        std::string path;
        uint64_t hash;
        uint32_t size;                  //Files that have not yet been written to the BSA file have a size of 0.
        uint32_t offset;                //This offset is from the beginning of the file - Tes3 BSAs use from the beginning of the data section,
                                        //so will have to adjust them. Files that have not yet been written to the BSA have a 0 offset.
    };

    struct PendingBsaAsset {
        std::string extPath;  //Path of file in filesystem.
        std::string intPath;  //Path of file in BSA.
    };
}

//Class for generic BSA data manipulation functions.
struct _bsa_handle_int {
public:
    _bsa_handle_int(const std::string& path);
    ~_bsa_handle_int();
    virtual void Save(std::string path, const uint32_t version, const uint32_t compression) = 0;

    bool HasAsset(const std::string& assetPath);
    libbsa::BsaAsset GetAsset(const std::string& assetPath);
    void GetMatchingAssets(const std::regex& regex, std::list<libbsa::BsaAsset>& matchingAssets);

    void Extract(const std::string& assetPath, uint8_t** _data, size_t* _size);
    void Extract(const std::string& assetPath, const std::string& destPath, const bool overwrite);
    void Extract(const std::list<libbsa::BsaAsset>& assetsToExtract, const std::string& destPath, const bool overwrite);

    uint32_t CalcChecksum(const std::string& assetPath);

    //External data array pointers and sizes.
    char ** extAssets;
    size_t extAssetsNum;
protected:
    //Reads the asset data into memory, at .first, with size .second. Remember to free the memory once used.
    virtual std::pair<uint8_t*, size_t> ReadData(boost::filesystem::ifstream& in, const libbsa::BsaAsset& data) = 0;

    std::string filePath;
    std::list<libbsa::BsaAsset> assets;         //Files not yet written to the BSA are in this and pendingAssets.
    std::list<libbsa::PendingBsaAsset> pendingAssets;  //Holds the internal->external path mapping for files not yet written to the BSA.
};

#endif
