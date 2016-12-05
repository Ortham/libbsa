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

#ifndef __LIBBSA_SSESTRUCTS_H__
#define __LIBBSA_SSESTRUCTS_H__

#include "genericbsa.h"
#include <stdint.h>
#include <string>
#include <boost/filesystem.hpp>

/* File format infos:
    <None currently available>

    This header file defines the constants, structures and functions specific
    to the Sse-type BSA, which is used by Skyrim: Special Edition
*/

namespace libbsa {
    namespace sse {
        //Sse-type BSA class.
        class BSA : public GenericBsa {
        public:
            static const uint32_t BSA_MAGIC = '\0ASB';  //Also for TES5, FO3 and probably FNV too.
            static const uint32_t BSA_VERSION_SSE = 0x69;

            static const uint32_t BSA_FOLDER_RECORD_OFFSET = 36;  //Folder record offset for TES4-type BSAs is constant.

            static const uint32_t BSA_COMPRESSED = 0x0004;  //If this flag is present in the archiveFlags header field, then the BSA file data is compressed.

            static const uint32_t FILE_INVERT_COMPRESSED = 0x40000000;  //Inverts the file data compression status for the specific file this flag is set for.

            BSA(const boost::filesystem::path& path);
            void Save(const boost::filesystem::path& path,
                      const uint32_t version,
                      const uint32_t compression);

            //Check if a given file is a Tes4-type BSA.
            static bool IsBSA(const boost::filesystem::path& path);
        private:
            std::pair<uint8_t*, size_t> ReadData(std::ifstream& in,
                                                 const BsaAsset& data) const;
            static std::pair<uint8_t*, size_t> uncompressData(const std::string& assetPath,
                                                              const uint8_t * data,
                                                              size_t size);

            static std::string getFolderName(const uint8_t * fileRecords,
                                             uint32_t folderOffset);
            static std::string getFileName(const uint8_t * fileNames,
                                           uint32_t offset);

            static uint32_t HashString(const std::string& str);
            static uint64_t CalcHash(const std::string& assetPath, const std::string& ext);

            uint32_t archiveFlags;
            uint32_t fileFlags;

            static bool hash_comp(const BsaAsset& first, const BsaAsset& second);
            static bool path_comp(const BsaAsset& first, const BsaAsset& second);

            struct Header {
                uint32_t fileId;
                uint32_t version;
                uint32_t offset;
                uint32_t archiveFlags;
                uint32_t folderCount;
                uint32_t fileCount;
                uint32_t totalFolderNameLength;
                uint32_t totalFileNameLength;
                uint32_t fileFlags;
            };

            struct FolderRecord {
                uint64_t nameHash;  //Hash of folder name.
                uint32_t count;     //Number of files in folder.
                uint32_t unk;	      //Unknown
                uint64_t offset;    //Offset to the fileRecords for this folder, including the folder name, from the beginning of the file.
            };

            struct FileRecord {
                uint64_t nameHash;  //Hash of the filename.
                uint32_t size;      //Size of the data. See TES4Mod wiki page for details.
                uint32_t offset;    //Offset to the raw file data, from byte 0.
            };
        };
    }
}

#endif