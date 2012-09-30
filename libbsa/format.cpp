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
#include <zlib.h>

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

const uint32_t LIBBSA_BSA_COMPRESSED = 0x0004;

const uint32_t LIBBSA_FILE_COMPRESSED = 0x40000000;  //Reverses the existence of LIBBSA_BSA_COMPRESSED for the file if set.

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

struct Tes3Header {
	uint32_t version;
	uint32_t hashOffset;
	uint32_t fileCount;
};

struct Tes4FolderRecord {
	uint64_t nameHash;	//Hash of folder name.
	uint32_t count;		//Number of files in folder.
	uint32_t offset;	//Offset to the fileRecords for this folder, including the folder name, from the beginning of the file.
};

struct Tes4FileRecord {
	uint64_t nameHash;	//Hash of the filename.
	uint32_t size;		//Size of the data. See TES4Mod wiki page for details.
	uint32_t offset;	//Offset to the raw file data, from byte 0.
};

bsa_handle_int::bsa_handle_int(string path) :
	extAssets(NULL),
	extAssetsNum(0),
	isTes3BSA(false),
	filePath(path) {
	
	//Set transcoding up.
	trans.SetEncoding(1252);

	//Check if file exists.
	if (fs::exists(path)) {
	
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
			isTes3BSA = true;
		else if (magic != LIBBSA_BSA_MAGIC_TES4)
			throw error(LIBBSA_ERROR_FILE_READ_FAIL, path);

		if (isTes3BSA) {
			in.seekg(0, ios_base::beg);  //Set back to beginning.
			Tes3Header header;
			in.read((char*)&header, sizeof(Tes3Header));

			hashOffset = header.hashOffset;
			fileCount = header.fileCount;

			/* We want:
			- file names
			- file sizes
			- raw data offsets

			Load the FileRecordData (size,offset), filename offsets and filename records into memory, then work on them there.
			*/
			FileRecordData * fileRecordData;
			uint32_t * filenameOffsets;
			uint8_t * filenameRecords;
			uint32_t filenameRecordsSize = header.hashOffset - sizeof(FileRecordData) * header.fileCount - sizeof(uint32_t) * header.fileCount;
			try {
				fileRecordData = new FileRecordData[header.fileCount];
				in.read((char*)fileRecordData, sizeof(FileRecordData) * header.fileCount);
				
				filenameOffsets = new uint32_t[header.fileCount];
				in.read((char*)filenameOffsets, sizeof(uint32_t) * header.fileCount);
				
				filenameRecords = new uint8_t[filenameRecordsSize];
				in.read((char*)filenameRecords, sizeof(uint8_t) * filenameRecordsSize);
			} catch (bad_alloc &e) {
				throw error(LIBBSA_ERROR_NO_MEM);
			}

			in.close(); //No longer need the file open.

			//All three arrays have the same ordering, so we just need to loop through one and look at the corresponding position in the other.
			for (uint32_t i=0; i < header.fileCount; i++) {
				FileRecordData fileData;
				fileData.size = fileRecordData[i].size;
				fileData.offset = fileRecordData[i].offset;

				//Now we need to build the file path. First: file name.
				//Find position of null pointer.
				char * nptr = strchr((char*)(filenameRecords + filenameOffsets[i]), '\0');
				if (nptr == NULL)
					throw error(LIBBSA_ERROR_FILE_READ_FAIL, path);

				string fileName = string((char*)(filenameRecords + filenameOffsets[i]), nptr - (char*)(filenameRecords + filenameOffsets[i]));

				//Finally, add file path and object to unordered map.
				paths.insert(pair<string, FileRecordData>(fileName, fileData));
			}

			delete [] fileRecordData;
			delete [] filenameOffsets;
			delete [] filenameRecords;

		} else {
			in.seekg(0, ios_base::beg);  //Set back to beginning.
			
			Tes4Header header;
			in.read((char*)&header, sizeof(Tes4Header));
			
			if ((header.version != LIBBSA_BSA_VERSION_TES4 && header.version != LIBBSA_BSA_VERSION_TES5) || header.offset != 36)
				throw error(LIBBSA_ERROR_FILE_READ_FAIL, path);

			//Record the file and archive flags.
			fileFlags = header.fileFlags;
			archiveFlags = header.archiveFlags;

			//Now we get to the real meat of the file.
			//Folder records are followed by file records in blocks by folder name, followed by file names.
			//File records and file names have the same ordering.
			Tes4FolderRecord * folderRecords;
			uint8_t * fileRecords;
			//Three terms are the number of folder name string length bytes, the total length of folder strings and the total number of file records.
			uint32_t startOfFileRecords = sizeof(Tes4Header) + sizeof(Tes4FolderRecord) * header.folderCount;
			uint32_t fileRecordsSize = header.folderCount * sizeof(uint8_t) + header.totalFolderNameLength + sizeof(Tes4FileRecord) * header.fileCount;
			uint32_t startOfFileNames = startOfFileRecords + fileRecordsSize;
			uint8_t * fileNames;	//A list of null-terminated filenames, one after another.
			try {
				folderRecords = new Tes4FolderRecord[header.folderCount];
				in.read((char*)folderRecords, sizeof(Tes4FolderRecord) * header.folderCount);
				
				fileRecords = new uint8_t[fileRecordsSize];
				in.read((char*)fileRecords, sizeof(uint8_t) * fileRecordsSize);
				
				fileNames = new uint8_t[header.totalFileNameLength];
				in.read((char*)fileNames, sizeof(uint8_t) * header.totalFileNameLength);
			} catch (bad_alloc &e) {
				throw error(LIBBSA_ERROR_NO_MEM);
			}

			in.close(); //No longer need the file open.
			
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
				
				
				//Now loop through the file records for this folder. For each file record, look up the corresponding filename. Keep a counter outside the Tes4FolderRecord loop as the filename list is for all folders.
				uint32_t j = startOfFileRecordBlock + 1 + folderNameLength + 1 + (uint32_t)fileRecords;
				uint32_t max = j + folderRecords[i].count * sizeof(Tes4FileRecord);
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
				
					j += sizeof(Tes4FileRecord);
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

void bsa_handle_int::Extract(FileRecordData data, std::string outPath) {
	//Check if outPath exists, and if its parent exists.
	if (fs::exists(outPath) || !fs::exists(fs::path(outPath).parent_path()))
		throw error(LIBBSA_ERROR_FILE_WRITE_FAIL, outPath);

	uint32_t size = data.size;
	uint32_t offset = data.offset;

	if (isTes3BSA)
		offset += sizeof(Tes3Header) + hashOffset + fileCount * sizeof(uint64_t);

	//Check if given file is compressed or not. If not, can ofstream straight to path, otherwise need to involve zlib.
/*	if (isTes3BSA || (archiveFlags & LIBBSA_BSA_COMPRESSED && size & LIBBSA_FILE_COMPRESSED) || archiveFlags & ~LIBBSA_BSA_COMPRESSED) {
		//Just need to use size and offset to write to binary file stream.
		uint8_t * buffer;

		//We probably need to get rid of the compression flag from size though, it can't purely be byte size with it there.
		if (!isTes3BSA && size & LIBBSA_FILE_COMPRESSED)
			size ^= LIBBSA_FILE_COMPRESSED;

		try {
			buffer = new uint8_t[size];
		} catch (bad_alloc &e) {
			throw error(LIBBSA_ERROR_NO_MEM);
		}

		ifstream in(filePath.c_str(), ios::binary);
		in.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

		in.seekg(offset, ios_base::beg);
		in.read((char*)buffer, size);

		in.close();

		ofstream out(outPath.c_str(), ios::binary | ios::trunc);
		out.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

		out.write((char*)buffer, size);

		out.close();

		delete [] buffer;
	} else {
*/		//Use zlib.
		size -= sizeof(uint32_t);  //First uint32_t of data is the size of the uncompressed data.
		uint32_t uncompressedSize;

		//Open input and output streams.
		ifstream in(filePath.c_str(), ios::binary);
		in.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

		//Get the uncompressed size.
		in.seekg(offset, ios_base::beg);
		in.read((char*)&uncompressedSize, sizeof(uint32_t));

		//in and out are now at their starting locations for reading and writing, and we have the compressed and uncompressed size.
		//Allocate memory for the compressed file and the uncompressed file.
		uint8_t * compressedFile;
		uint8_t * uncompressedFile;
		try {
			compressedFile = new uint8_t[size];
			uncompressedFile = new uint8_t[uncompressedSize];
		} catch (bad_alloc &e) {
			throw error(LIBBSA_ERROR_NO_MEM);
		}

		in.read((char*)compressedFile, size);
		in.close();

		//We can use a pre-made utility function instead of having to mess around with zlib proper.
		int ret = uncompress(uncompressedFile, (uLongf*)&uncompressedSize, compressedFile, size);
		if (ret != Z_OK)
			throw error(LIBBSA_ERROR_ZLIB_ERROR);

		//Now output to file.
		ofstream out(outPath.c_str(), ios::binary | ios::trunc);
		out.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

		out.write((char*)uncompressedFile, uncompressedSize);
		out.close();

		//Free memory.
		delete [] compressedFile;
		delete [] uncompressedFile;
//	}
}

void bsa_handle_int::Extract(boost::unordered_map<std::string, FileRecordData>& data, std::string outPath) {
	//Check if outPath exists and that it is a directory.
	if (!fs::exists(outPath) || !fs::is_directory(outPath))
		throw error(LIBBSA_ERROR_FILE_WRITE_FAIL, outPath);

	//Open source BSA.
	ifstream in(filePath.c_str(), ios::binary);
	in.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

	//Loop through the map, checking that each path doesn't already exist, creating path components if necessary, and extracting files.
	for (boost::unordered_map<string, FileRecordData>::iterator it = data.begin(), endIt = data.end(); it != endIt; ++it) {
		//Create parent directories.
		fs::path fullPath = fs::path(outPath) / it->first;
		fs::create_directories(fullPath.parent_path());  //This creates any directories in the path that don't already exist.

		if (fs::exists(fullPath))
			throw error(LIBBSA_ERROR_FILE_WRITE_FAIL, fullPath.string());

		uint32_t size = it->second.size;
		uint32_t offset = it->second.offset;

		if (isTes3BSA)
			offset += sizeof(Tes3Header) + hashOffset + fileCount * sizeof(uint64_t);

		//Check if given file is compressed or not. If not, can ofstream straight to path, otherwise need to involve zlib.
		if (isTes3BSA || (archiveFlags & LIBBSA_BSA_COMPRESSED && size & LIBBSA_FILE_COMPRESSED) || archiveFlags & ~LIBBSA_BSA_COMPRESSED) {
			//Just need to use size and offset to write to binary file stream.
			uint8_t * buffer;

			//We probably need to get rid of the compression flag from size though, it can't purely be byte size with it there.
			if (!isTes3BSA && size & LIBBSA_FILE_COMPRESSED)
				size ^= LIBBSA_FILE_COMPRESSED;

			try {
				buffer = new uint8_t[size];
			} catch (bad_alloc &e) {
				throw error(LIBBSA_ERROR_NO_MEM);
			}

			in.seekg(offset, ios_base::beg);
			in.read((char*)buffer, size);

			ofstream out(fullPath.c_str(), ios::binary | ios::trunc);
			out.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

			out.write((char*)buffer, size);

			out.close();

			delete [] buffer;
		} else {
			//Use zlib.
		}
	}

	in.close();
}

// Calculates a mini-hash.
uint32_t bsa_handle_int::Tes4HashString(std::string str) {
	uint32_t hash = 0;
	for (size_t i=0, len=str.length(); i < len; i++) {
		hash = 0x1003F * hash + str[i];
	}
	return hash;
}

//Implemented following the Python example here:
//<http://www.uesp.net/wiki/Tes4Mod:BSA_File_Format>
uint64_t bsa_handle_int::CalcTes4Hash(std::string path) {
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
				hash2 = Tes4HashString(file);
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
	
		hash3 = Tes4HashString(file);
	}
	
	hash2 = hash2 + hash3;
	return ((uint64_t)hash2 << 32) + hash1;
}

uint8_t * bsa_handle_int::GetString(std::string str) {
	str = trans.EncToUtf8(str);
	return ToUint8_tString(str);
}