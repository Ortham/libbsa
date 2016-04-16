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

#include "bsa_asset.h"
#include <stdint.h>
#include <string>
#include <list>
#include <regex>

#include <boost/filesystem/fstream.hpp>

/* This header declares the generic structures that libbsa uses to handle BSA
   manipulation.
   All strings are encoded in UTF-8.
*/

//Class for generic BSA data manipulation functions.
struct GenericBsa {
public:
    GenericBsa(const std::string& path);

    virtual void Save(std::string path, const uint32_t version, const uint32_t compression) = 0;

    bool HasAsset(const std::string& assetPath);
    libbsa::BsaAsset GetAsset(const std::string& assetPath);
    void GetMatchingAssets(const std::regex& regex, std::list<libbsa::BsaAsset>& matchingAssets);

    void Extract(const std::string& assetPath, uint8_t** _data, size_t* _size);
    void Extract(const std::string& assetPath, const std::string& destPath, const bool overwrite);
    void Extract(const std::list<libbsa::BsaAsset>& assetsToExtract, const std::string& destPath, const bool overwrite);

    uint32_t CalcChecksum(const std::string& assetPath);
protected:
    //Reads the asset data into memory, at .first, with size .second. Remember to free the memory once used.
    virtual std::pair<uint8_t*, size_t> ReadData(boost::filesystem::ifstream& in, const libbsa::BsaAsset& data) = 0;

    std::string filePath;
    std::list<libbsa::BsaAsset> assets;         //Files not yet written to the BSA are in this and pendingAssets.
};

#endif
