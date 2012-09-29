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

#include "format.h"
#include "exception.h"
#include "libbsa.h"
#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

namespace fs = boost::filesystem;

using namespace std;
using namespace libbsa;

/* File format infos:
	<http://www.uesp.net/wiki/Tes3Mod:BSA_File_Format>
   <http://www.uesp.net/wiki/Tes4Mod:BSA_File_Format>
   <http://www.uesp.net/wiki/Tes5Mod:Archive_File_Format>
   <http://falloutmods.wikia.com/wiki/BSA_file_format>
   <http://forums.bethsoft.com/topic/957536-wipz-tes4files-for-f3/>
*/

const uint32_t LIBBSA_BSA_MAGIC_TES4 = '\0ASB';  //Also for TES5, FO3 and probably FNV too.
const uint32_t LIBBSA_BSA_VERSION_TES3 = 0x100;  //Yeah, MW's BSA version is higher than later games'.
const uint32_t LIBBSA_BSA_VERSION_TES4 = 0x67;
const uint32_t LIBBSA_BSA_VERSION_TES5 = 0x68;   //Also for FO3 and probably FNV too.

struct Tes4Header {
	uint32_t field;
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

bsa_handle_int::bsa_handle_int(string path) :
	extAssets(NULL),
	extAssetsNum(0) {
	
	//Set transcoding up.
	trans.SetEncoding(1252);

	//Check if file exists.
	if (fs::exists(path)) {
		bool isTES3BSA = false;  //Morrowind BSAs have a different format.
	
		ifstream in(path.c_str(), ios::binary);
		in.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

		/* Parse it!
		   Note that all numbers are encoded as little-endian. This is good, 
		   because then we can read them directly into memory without having
		   to reverse byte order on all likely CPUs. However, in future, may
		   be worth considering doing an endian-ness check and reversing as 
		   necessary. 
		   Important stuff to look for:
		   1. Magic marker for BSAs. Fail if not found.
		   2. Version number. Fail if not valid.

		   libbsa doesn't really care about file data until extraction/saving.
		   What is important is what files there are in the BSA, and where their
		   data can be found.
		   Since paths must be unique (to calculate unique hashes), create a map
		   that holds <path, offset> data. Data inside a BSA is sorted by hash
		   value, but with all folders first followed by all files, so no point
		   keeping the map ordered, better to order it at save time.
		*/

		// Before we load header into memory struct, check which struct to use.
		uint32_t magic;
		in.read((char*)&magic, sizeof(uint32_t));
		
		if (magic == LIBBSA_BSA_VERSION_TES3)  //Magic is actually tes3 bsa version.
			isTES3BSA = true;
		else if (magic != LIBBSA_BSA_MAGIC_TES4)
			throw error(LIBBSA_ERROR_FILE_READ_FAIL, path);
		
		if (!isTES3BSA) {
			in.seekg(0, ios_base::beg);  //Set back to beginning.
			
			Tes4Header header;
			in.read((char*)&header, sizeof(Tes4Header));
			
			if ((header.version != LIBBSA_BSA_VERSION_TES4 && header.version != LIBBSA_BSA_VERSION_TES5) || header.offset != 36)
				throw error(LIBBSA_ERROR_FILE_READ_FAIL, path);

			//Now we get to the real meat of the file.
			//Folder records are followed by file records in blocks by folder name, followed by file names.
			//File records and file names have the same ordering.
			FolderRecord * folderRecords;
			uint8_t * fileRecords;
			//Three terms are the number of folder name string length bytes, the total length of folder strings and the total number of file records.
			uint32_t startOfFileRecords = sizeof(Tes4Header) + sizeof(FolderRecord) * header.folderCount;
			uint32_t fileRecordsSize = header.folderCount * sizeof(uint8_t) + header.totalFolderNameLength + sizeof(FileRecord) * header.fileCount;
			uint32_t startOfFileNames = startOfFileRecords + fileRecordsSize;
			uint8_t * fileNames;	//A list of null-terminated filenames, one after another.
			try {
				folderRecords = new FolderRecord[header.folderCount];
				in.read((char*)folderRecords, sizeof(FolderRecord) * header.folderCount);
				
				fileRecords = new uint8_t[fileRecordsSize];
				in.read((char*)fileRecords, sizeof(uint8_t) * fileRecordsSize);
				
				fileNames = new uint8_t[header.totalFileNameLength];
				in.read((char*)fileNames, sizeof(uint8_t) * header.totalFileNameLength);
			} catch (bad_alloc &e) {
				throw error(LIBBSA_ERROR_NO_MEM);
			}
			
			/* Now we have the folder records, file records and file names in memory. This gives us:
			
				- folder name hashes
				- numbers of files in each folder
				- the offsets of the file records for the files in each folder
				- the folder names
				- the file name hashes
				- the file sizes, plus compression data
				- the offsets to the raw data of each file
				- the file names
				
			We can discard the hashes, since they're calculated purely from the paths.
			After we've processed all the files in each folder, we can discard their numbers.
			After we've processed all the file records for each folder, we can discard their offsets.
			We want to keep the folder names.
			We want to keep the file sizes and compression data.
			We want to keep the offsets to the raw data of each file.
			We want to keep the file names. 
			
			We'll need a bunch of nested loops.
			First level: loop through the folder records.
			*/
			
			uint32_t fileNameListPos = 0;
			for (uint32_t i=0; i < header.folderCount; i++) {
				//For each folder record, we want the count and the offset.
				//Go to the offset to get the folder name.
				//Offset is from 0th byte of file and includes totalFileNameLength for some reason.
				uint32_t startOfFileRecordBlock = folderRecords[i].offset - startOfFileRecords - header.totalFileNameLength;
				uint8_t folderNameLength = *(fileRecords + startOfFileRecordBlock) - 1;
				string folderName = string((char*)(fileRecords + startOfFileRecordBlock + 1), folderNameLength);
				
				
				//Now loop through the file records for this folder. For each file record, look up the corresponding filename. Keep a counter outside the FolderRecord loop as the filename list is for all folders.
				uint32_t j = startOfFileRecordBlock + 1 + folderNameLength + 1 + (uint32_t)fileRecords;
				uint32_t max = j + folderRecords[i].count * sizeof(FileRecord);
				while (j < max) {
					//Fill in file data.
					FileRecordData fileData;
					fileData.size = *(uint32_t*)(j + sizeof(uint64_t));
					fileData.offset = *(uint32_t*)(j + sizeof(uint64_t) + sizeof(uint32_t));
						
					//Now we need to build the file path. First: file name.
					//Find position of null pointer.
					char * nptr = strchr((char*)(fileNames + fileNameListPos), '\0');
					if (nptr == NULL)
						throw error(LIBBSA_ERROR_FILE_READ_FAIL, path);

					string fileName = string((char*)(fileNames + fileNameListPos), nptr - (char*)(fileNames + fileNameListPos));

					string filePath;
					if (!folderName.empty())
						filePath = folderName + '\\';
					filePath += fileName;
					
					//Finally, add file path and object to unordered map.
					paths.insert(pair<string, FileRecordData>(filePath, fileData));
				
					j += sizeof(FileRecord);
					fileNameListPos += fileName.length() + 1;
				}
			}
			
			delete [] folderRecords;
			delete [] fileRecords;
			delete [] fileNames;
		}
	}
}

bsa_handle_int::~bsa_handle_int() {
	if (extAssets != NULL) {
		for (size_t i=0; i < extAssetsNum; i++)
			delete [] extAssets[i];
		delete [] extAssets;
	}
}

void bsa_handle_int::Save(std::string path, const uint32_t flags) {

}

// Calculates a mini-hash.
uint32_t bsa_handle_int::HashString(std::string str) {
	uint32_t hash = 0;
	for (size_t i=0, len=str.length(); i < len; i++) {
		hash = 0x1003F * hash + str[i];
	}
	return hash;
}

//Implemented following the Python example here:
//<http://www.uesp.net/wiki/Tes4Mod:BSA_File_Format>
uint64_t bsa_handle_int::CalcHash(std::string path) {
	uint64_t hash1 = 0;
	uint32_t hash2 = 0;
	uint32_t hash3 = 0;

	boost::to_lower(path);
	const string ext = fs::path(path).extension().string();
	const string file = fs::path(path).stem().string();
	const size_t len = file.length(); 
	
	if (!file.empty()) {
		hash1 =
			file[len - 1]
			| len << 16
			| file[0] << 24;
		
		if (len > 2) {
			hash1 |= file[len - 2] << 8;
			if (len > 3)
				hash2 = HashString(file);
		}
	}
	
	if (!ext.empty()) {
		if (ext == ".kf")
			hash1 |= 0x80;
		else if (ext == ".nif")
			hash1 |= 0x8000;
		else if (ext == ".dds")
			hash1 |= 0x8080;
		else if (ext == ".wav")
			hash1 |= 0x80000000;
	
		hash3 = HashString(file);
	}
	
	hash2 = hash2 + hash3;
	return ((uint64_t)hash2 << 32) + hash1;
}

uint8_t * bsa_handle_int::GetString(std::string str) {
	str = trans.EncToUtf8(str);
	return ToUint8_tString(str);
}