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
#include <map>
#include <sstream>
#include <vector>

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

struct FileData {
	string name;
	string parent;
	uint32_t size;
	uint32_t offset;
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

void bsa_handle_int::Save(std::string path, const uint32_t version, const uint32_t compression) {
	//Version and compression have been validated.
	if (version == LIBBSA_VERSION_TES3) {
		//Build file header.
		Tes3Header header;
		header.version = LIBBSA_BSA_VERSION_TES3;
		header.fileCount = paths.size();
		//Can't set hashOffset until the size of the names array is known.

		//All records apart from actual data are sorted by hash, so we need to calculate the hashes of all records, and put that into a map with their name strings.
		//File data is stored alphabetically by filename, so make the unordered map ordered.
		std::map<uint64_t, string> hashmap;
		std::map<string, FileRecordData> fileList;
		for (boost::unordered_map<string, FileRecordData>::iterator it = paths.begin(), endIt = paths.end(); it != endIt; ++it) {
			uint64_t hash = CalcTes3Hash(it->first);
			hashmap.insert(pair<uint64_t, string>(hash, it->first));
			fileList.insert(*it);
		}
		//fileList will be the same as paths, but with updated offsets.
		//hashmap will be used to get fileList data in hash order.

		//Allocate memory for info blocks.
		FileRecordData * fileRecordData;
		uint32_t * filenameOffsets;
		uint64_t * hashes;
		//Can't allocate memory for filenames, because we don't know how long they are to be. Use a string buffer instead.
		string filenameRecords;
		try {
			fileRecordData = new FileRecordData[header.fileCount];
			filenameOffsets = new uint32_t[header.fileCount];
			hashes = new uint64_t[header.fileCount];
		} catch (bad_alloc &e) {
			throw error(LIBBSA_ERROR_NO_MEM);
		}

		//Update the file data offsets by looping through the map and incrementing it with file sizes.
		uint32_t fileDataOffset = 0;
		for (map<string, FileRecordData>::iterator it = fileList.begin(), endIt = fileList.end(); it != endIt; ++it) {
			it->second.offset = fileDataOffset;  //This results in the wrong offsets for 1185 files in Morrowind.bsa.
			//All the other files are OK, only thing I can think of is that hashes are wrong.
			fileDataOffset += it->second.size;
		}

		//file data, names and hashes are all done in hash order.
		uint32_t filenameOffset = 0;
		uint32_t i = 0;
		for (map<uint64_t, string>::iterator it = hashmap.begin(), endIt = hashmap.end(); it != endIt; ++it) {
			
			//Find file record data in map.
			map<string, FileRecordData>::iterator itr = fileList.find(it->second);
			if (itr == fileList.end())
				throw error(LIBBSA_ERROR_FILE_WRITE_FAIL);

			//Set size and offset.
			fileRecordData[i].size = itr->second.size;
			fileRecordData[i].offset = itr->second.offset;
			
			//Set filename offset, and store filename.
			filenameOffsets[i] = filenameOffset;
			filenameRecords += it->second + '\0';
			filenameOffset += it->second.length() + 1;
			
			hashes[i] = it->first;
			i++;
		}

		//fileRecordData, filenameOffsets, hashes and filenameRecords are now complete.
		//We can now calculate the header's hashOffset to complete it.
		header.hashOffset = (sizeof(FileRecordData) + sizeof(uint32_t)) * header.fileCount + filenameRecords.length();

		//Now open up the input and output streams and start reading/writing file data.
		//Open out stream.
		if (path == filePath)
			path += ".new";  //Avoid read/write collisions.
		ofstream out(path.c_str(), ios::binary | ios::trunc);
		out.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

		ifstream in(filePath.c_str(), ios::binary);
		in.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

		//Only thing left to do is to write out the completed BSA sections, then
		//read the file data from the input and write it to the output.

		//Write out completed BSA sections.
		out.write((char*)&header, sizeof(Tes3Header));
		out.write((char*)fileRecordData, sizeof(FileRecordData) * header.fileCount);
		out.write((char*)filenameOffsets, sizeof(uint32_t) * header.fileCount);
		out.write((char*)filenameRecords.data(), filenameRecords.length());
		out.write((char*)hashes, sizeof(uint64_t) * header.fileCount);

		//Now write out raw file data in alphabetical filename order.
		for (map<string, FileRecordData>::iterator it = fileList.begin(), endIt = fileList.end(); it != endIt; ++it) {
			//it->second.offset is the offset for the data in the new file. We don't need it though, because we're doing writes in sequence.
			//We want the offset for the data in the old file.
			boost::unordered_map<string, FileRecordData>::iterator itr = paths.find(it->first);

			if (itr == paths.end())
				throw error(LIBBSA_ERROR_FILE_WRITE_FAIL);

			//Allocate memory for this file's data, read it in, write it out, then free memory.
			uint8_t * fileData;
			try {
				fileData = new uint8_t[it->second.size];  //Doesn't matter where we get size from.
			} catch (bad_alloc &e) {
				throw error(LIBBSA_ERROR_NO_MEM);
			}

			//Read data in.
			in.seekg(itr->second.offset + sizeof(Tes3Header) + header.hashOffset + header.fileCount * sizeof(uint64_t), ios_base::beg);
			in.read((char*)fileData, it->second.size);

			//Write data out.
			out.write((char*)fileData, it->second.size);

			//Free memory.
			delete [] fileData;

			//As Save() could be called at any point in the BSA handle's use, we need to ensure that the data is
			//valid going foward.
			itr->second.offset = it->second.offset;
		}

		out.close();
		in.close();

		//Now rename the output file.
		if (fs::path(path).extension().string() == ".new") {
			try {
				fs::rename(path, fs::path(path).stem());
			} catch (fs::filesystem_error& e) {
				throw LIBBSA_ERROR_FILE_WRITE_FAIL;
			}
		}

		//Update member vars.
		isTes3BSA = true;
		filePath = path;
		hashOffset = header.hashOffset;
		fileCount = header.fileCount;
	} else {

		///////////////////////////////
		// Set header up
		///////////////////////////////

		Tes4Header header;

		header.fileId = LIBBSA_BSA_MAGIC_TES4;

		if (version == LIBBSA_VERSION_TES4)
			header.version = LIBBSA_BSA_VERSION_TES4;
		else if (version == LIBBSA_VERSION_TES5)
			header.version = LIBBSA_BSA_VERSION_TES5;

		header.offset = 36;

		header.archiveFlags = archiveFlags;
		if (compression == LIBBSA_COMPRESS_LEVEL_0 && header.archiveFlags & LIBBSA_BSA_COMPRESSED)
			header.archiveFlags ^= LIBBSA_BSA_COMPRESSED;
		else if (compression != LIBBSA_COMPRESS_LEVEL_0 && !(header.archiveFlags & LIBBSA_BSA_COMPRESSED))
			header.archiveFlags |= LIBBSA_BSA_COMPRESSED;

		//Need to sort folder and file names separately into hash-sorted maps before header.folderCount and name lengths can be set.
		std::map<uint64_t, string> folderHashmap;
		std::map<uint64_t, FileData> fileHashmap;
		for (boost::unordered_map<string, FileRecordData>::iterator it = paths.begin(), endIt = paths.end(); it != endIt; ++it) {
			string folder = fs::path(it->first).parent_path().string();
			string file = fs::path(it->first).filename().string();

			uint64_t folderHash = CalcTes4Hash(folder);
			uint64_t fileHash = CalcTes4Hash(file);

			FileData fd;
			fd.name = file;
			fd.parent = folder;
			fd.size = it->second.size;
			fd.offset = it->second.offset;

			folderHashmap.insert(pair<uint64_t, string>(folderHash, folder));
			fileHashmap.insert(pair<uint64_t, FileData>(fileHash, fd));
		}
		header.folderCount = folderHashmap.size();

		header.fileCount = paths.size();

		header.totalFolderNameLength = 0;
		for (map<uint64_t, string>::iterator it = folderHashmap.begin(), endIt=folderHashmap.end(); it != endIt; ++it) {
			header.totalFolderNameLength += it->second.length() + 1;
		}

		string fileNameBlock;
		header.totalFileNameLength = 0;
		for (map<uint64_t, FileData>::iterator it = fileHashmap.begin(), endIt=fileHashmap.end(); it != endIt; ++it) {
			fileNameBlock += it->second.name + '\0';
		}
		header.totalFileNameLength = fileNameBlock.length();
		
		header.fileFlags = fileFlags;

		/////////////////////////////
		// Set folder record array
		/////////////////////////////

		/* Iterate through the folder hashmap.
		   For each folder, scan the file hashmap for files with matching parent paths.
		   For any such files, write out their nameHash, size and the offset at which their data can be found (calculated from the sum of previous sizes).
		   Also prepend the length of the folder name and the folder name to this file data list.
		   Once all matching files have been found, add their count and offset to the folder record stream.
		*/

		ostringstream folderRecords;
		ostringstream fileRecordBlocks;
		uint32_t fileRecordBlockOffset = header.totalFileNameLength;
		uint32_t fileDataOffset = sizeof(Tes3Header) + sizeof(Tes4FolderRecord) * header.folderCount + header.totalFolderNameLength + header.folderCount + sizeof(Tes4FileRecord) * header.fileCount + header.totalFileNameLength;
		vector<FileData> dataVec;
		for (map<uint64_t, string>::iterator it = folderHashmap.begin(), endIt=folderHashmap.end(); it != endIt; ++it) {
			size_t fileCount = 0;
			uint8_t nameLength = it->second.length() + 1;

			//Write folder name length, folder name to fileRecordBlocks stream.
			fileRecordBlocks.write((char*)&nameLength, 1);
			fileRecordBlocks.write((it->second + '\0').data(), nameLength);

			for (map<uint64_t, FileData>::iterator itr = fileHashmap.begin(), endItr=fileHashmap.end(); itr != endItr; ++itr) {
				if (itr->second.parent == it->second) {
					//Write file hash, size and offset to fileRecordBlocks stream.
					fileRecordBlocks.write((char*)&(itr->first), sizeof(uint64_t));
					uint32_t size = itr->second.size;
					itr->second.offset = fileDataOffset;
					fileRecordBlocks.write((char*)&size, sizeof(uint32_t));
					fileRecordBlocks.write((char*)&fileDataOffset, sizeof(uint32_t));
					//Increment count and data offset.
					fileCount++;
					fileDataOffset += size;
					//Add record data to vector for later ordered extraction.
					dataVec.push_back(itr->second);
				}
			}

			//Now write folder record.
			folderRecords.write((char*)&(it->first), sizeof(uint64_t));
			folderRecords.write((char*)&fileCount, sizeof(uint32_t));
			folderRecords.write((char*)&fileRecordBlockOffset, sizeof(uint32_t));

			//Increment fileRecordBlockOffset.
			fileRecordBlockOffset = fileRecordBlocks.str().length();
		}

		////////////////////////
		// Write out
		////////////////////////

		if (path == filePath)
			path += ".new";  //Avoid read/write collisions.
		ofstream out(path.c_str(), ios::binary | ios::trunc);
		out.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

		out.write((char*)&header, sizeof(Tes4Header));
		out.write(folderRecords.str().data(), sizeof(Tes4FolderRecord) * header.folderCount);
		out.write(fileRecordBlocks.str().data(), fileRecordBlocks.str().length());
		out.write(fileNameBlock.data(), header.totalFileNameLength);

		ifstream in(filePath.c_str(), ios::binary);
		in.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

		//Now write out raw file data in the same order it was listed in the FileRecordBlocks.
		for (vector<FileData>::iterator it = dataVec.begin(), endIt = dataVec.end(); it != endIt; ++it) {
			//Allocate memory for this file's data, read it in, write it out, then free memory.
			uint8_t * fileData;
			try {
				fileData = new uint8_t[it->size];  //Doesn't matter where we get size from.
			} catch (bad_alloc &e) {
				throw error(LIBBSA_ERROR_NO_MEM);
			}

			//Read data in.
			in.seekg(it->offset, ios_base::beg);
			in.read((char*)fileData, it->size);

			//Write data out.
			out.write((char*)fileData, it->size);

			//Free memory.
			delete [] fileData;

			//Update offset for this file in the paths map.
			boost::unordered_map<string, FileRecordData>::iterator itr = paths.find(it->parent + '\\' + it->name);

			if (itr == paths.end())
				throw error(LIBBSA_ERROR_FILE_NOT_FOUND, it->parent + '\\' + it->name);

			itr->second.offset = it->offset;
		}

		out.close();
		in.close();
	}
}

void bsa_handle_int::Extract(FileRecordData data, std::string outPath) {
	//Check if outPath exists, and if its parent exists.
	if (fs::exists(outPath) || !fs::exists(fs::path(outPath).parent_path()))
		throw error(LIBBSA_ERROR_FILE_WRITE_FAIL, outPath);

	//Open input stream.
	ifstream in(filePath.c_str(), ios::binary);
	in.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

	ExtractFromStream(in, data, outPath);

	in.close();
}

void bsa_handle_int::Extract(boost::unordered_map<std::string, FileRecordData>& data, std::string outPath) {
	//Check if outPath exists and that it is a directory.
	if (!fs::exists(outPath) || !fs::is_directory(outPath))
		throw error(LIBBSA_ERROR_FILE_WRITE_FAIL, outPath);

	//Open the source BSA.
	ifstream in(filePath.c_str(), ios::binary);
	in.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

	//Loop through the map, checking that each path doesn't already exist, creating path components if necessary, and extracting files.
	for (boost::unordered_map<string, FileRecordData>::iterator it = data.begin(), endIt = data.end(); it != endIt; ++it) {
		//Create parent directories.
		fs::path fullPath = fs::path(outPath) / it->first;
		fs::create_directories(fullPath.parent_path());  //This creates any directories in the path that don't already exist.

		if (fs::exists(fullPath))
			throw error(LIBBSA_ERROR_FILE_WRITE_FAIL, fullPath.string());

		ExtractFromStream(in, it->second, outPath);
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
	string file;
	if (ext.empty())
		file = fs::path(path).string();
	else
		file = fs::path(path).stem().string();
	const size_t len = file.length(); 
	
	if (!file.empty()) {
		hash1 =
			(uint64_t)(file[len - 1])
			+ ((uint64_t)(len) << 16)
			+ ((uint64_t)(file[0]) << 24);
		
		if (len > 2) {
			hash1 += ((uint64_t)(file[len - 2]) << 8);
			if (len > 3)
				hash2 = Tes4HashString(file);
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

		hash3 = Tes4HashString(file);
	}
	
	hash2 = hash2 + hash3;
	return ((uint64_t)hash2 << 32) + hash1;
}

//Taken from: <http://www.uesp.net/wiki/Tes3Mod:BSA_File_Format#Hash_calculation>
uint64_t bsa_handle_int::CalcTes3Hash(std::string path) {
	size_t len = path.length();
	uint32_t hash1 = 0;
	uint32_t hash2 = 0;
	unsigned l = path.length() >> 1;
	unsigned sum, off, temp, i, n;

	for (sum = off = i = 0; i < l; i++) {
		sum ^= (((unsigned)(path[i])) << (off & 0x1F));
		off += 8;
	}
	hash1 = sum;

	for (sum = off = 0; i < len; i++) {
		temp = (((unsigned)(path[i])) << (off & 0x1F));
		sum ^= temp;
		n = temp & 0x1F;
		sum = (sum << (32 - n)) | (sum >> n);
		off += 8;
	}
	hash2 = sum;

	return ((uint64_t)hash1 << 32) + (uint64_t)hash2;
}

uint8_t * bsa_handle_int::GetString(std::string str) {
	str = trans.EncToUtf8(str);
	return ToUint8_tString(str);
}

void bsa_handle_int::ExtractFromStream(ifstream& in, FileRecordData data, std::string outPath) {
	//Check if given file is compressed or not. If not, can ofstream straight to path, otherwise need to involve zlib.
	if (isTes3BSA || (archiveFlags & LIBBSA_BSA_COMPRESSED && data.size & LIBBSA_FILE_COMPRESSED) || !(archiveFlags & LIBBSA_BSA_COMPRESSED)) {
		//Just need to use size and offset to write to binary file stream.
		uint8_t * buffer;

		if (isTes3BSA)
			data.offset += sizeof(Tes3Header) + hashOffset + fileCount * sizeof(uint64_t);
		else if (data.size & LIBBSA_FILE_COMPRESSED)  //Remove compression flag from size to get actual size.
			data.size ^= LIBBSA_FILE_COMPRESSED;

		try {
			buffer = new uint8_t[data.size];
		} catch (bad_alloc &e) {
			throw error(LIBBSA_ERROR_NO_MEM);
		}

		in.seekg(data.offset, ios_base::beg);
		in.read((char*)buffer, data.size);

		ofstream out(outPath.c_str(), ios::binary | ios::trunc);
		out.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

		out.write((char*)buffer, data.size);

		out.close();

		delete [] buffer;
	} else {
		//Use zlib.
		//Get the uncompressed size.
		uint32_t uncompressedSize;
		in.seekg(data.offset, ios_base::beg);
		in.read((char*)&uncompressedSize, sizeof(uint32_t));

		//in and out are now at their starting locations for reading and writing, and we have the compressed and uncompressed size.
		//Allocate memory for the compressed file and the uncompressed file.
		uint8_t * compressedFile;
		uint8_t * uncompressedFile;
		data.size -= sizeof(uint32_t);  //First uint32_t of data is the size of the uncompressed data.
		try {
			compressedFile = new uint8_t[data.size];
			uncompressedFile = new uint8_t[uncompressedSize];
		} catch (bad_alloc &e) {
			throw error(LIBBSA_ERROR_NO_MEM);
		}

		in.read((char*)compressedFile, data.size);

		//We can use a pre-made utility function instead of having to mess around with zlib proper.
		int ret = uncompress(uncompressedFile, (uLongf*)&uncompressedSize, compressedFile, data.size);
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
	}
}