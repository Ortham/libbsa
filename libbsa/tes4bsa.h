/*	libbsa
	
	A library for reading and writing BSA files.

    Copyright (C) 2012    WrinklyNinja

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

#ifndef LIBBSA_TES4STRUCTS_H
#define LIBBSA_TES4STRUCTS_H

#include "genericbsa.h"
#include <stdint.h>
#include <string>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

/* File format infos:
	<http://www.uesp.net/wiki/Tes4Mod:BSA_File_Format>
	<http://www.uesp.net/wiki/Tes5Mod:Archive_File_Format>
	<http://falloutmods.wikia.com/wiki/BSA_file_format>
	<http://forums.bethsoft.com/topic/957536-wipz-tes4files-for-f3/>

	This header file defines the constants, structures and functions specific 
	to the Tes4-type BSA, which is used by Oblivion, Fallout 3, 
	Fallout: New Vegas and Skyrim.
*/

namespace libbsa { namespace tes4 {

	const uint32_t BSA_MAGIC = '\0ASB';  //Also for TES5, FO3 and probably FNV too.
	const uint32_t BSA_VERSION_TES4 = 0x67;
	const uint32_t BSA_VERSION_TES5 = 0x68;   //Also for FO3 and probably FNV too.

	const uint32_t BSA_FOLDER_RECORD_OFFSET = 36;  //Folder record offset for TES4-type BSAs is constant.

	const uint32_t BSA_COMPRESSED = 0x0004;  //If this flag is present in the archiveFlags header field, then the BSA file data is compressed.

	const uint32_t FILE_INVERT_COMPRESSED = 0x40000000;  //Inverts the file data compression status for the specific file this flag is set for.

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
		uint64_t nameHash;	//Hash of folder name.
		uint32_t count;		//Number of files in folder.
		uint32_t offset;	//Offset to the fileRecords for this folder, including the folder name, from the beginning of the file.
	};

	struct FileRecord {
		uint64_t nameHash;	//Hash of the filename.
		uint32_t size;		//Size of the data. See TES4Mod wiki page for details.
		uint32_t offset;	//Offset to the raw file data, from byte 0.
	};

	// Calculates a mini-hash.
	inline uint32_t HashString(std::string str) {
		uint32_t hash = 0;
		for (size_t i=0, len=str.length(); i < len; i++) {
			hash = 0x1003F * hash + (uint8_t)str[i];
		}
		return hash;
	}

	//Implemented following the Python example here:
	//<http://www.uesp.net/wiki/Tes4Mod:BSA_File_Format>
	inline uint64_t CalcHash(std::string path, std::string ext) {
		uint64_t hash1 = 0;
		uint32_t hash2 = 0;
		uint32_t hash3 = 0;
		const size_t len = path.length(); 
	
		if (!path.empty()) {
			hash1 = (uint64_t)(
					((uint8_t)path[len - 1])
					+ (len << 16)
					+ ((uint8_t)path[0] << 24)
				);
		
			if (len > 2) {
				hash1 += ((uint8_t)path[len - 2] << 8);
				if (len > 3)
					hash2 = HashString(path.substr(1, len - 3));
			}
		}
	
		if (!ext.empty()) {
			if (ext == ".kf")
				hash1 += 0x80;
			else if (ext == ".nif")
				hash1 += 0x8000;
			else if (ext == ".dds")
				hash1 += 0x8080;
			else if (ext == ".wav")
				hash1 += 0x80000000;

			hash3 = HashString(ext);
		}
	
		hash2 = hash2 + hash3;
		return ((uint64_t)hash2 << 32) + hash1;
	}

	//Comparison function for list::sort by hash.
	inline bool hash_comp(const BsaAsset first, const BsaAsset second) {
		return first.hash < second.hash;
	}

	//Comparison class for list::unique.
	class is_same_file {
	public:
		bool operator() (const BsaAsset first, const BsaAsset second) {
			return first.path == second.path;
		}
	} same_file_comp;
} }

#endif