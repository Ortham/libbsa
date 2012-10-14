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

#include "bsahandle.h"
#include "exception.h"
#include "libbsa.h"
#include "tes3bsa.h"
#include "tes4bsa.h"
#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <zlib.h>
#include <set>
#include <sstream>
#include <vector>

namespace fs = boost::filesystem;

using namespace std;
using namespace libbsa;

namespace libbsa {

	/////////////////////////////////////////////////
	// Comparison classes for list sorting.
	/////////////////////////////////////////////////

	//Comparison class for list::unique.
	class is_same_file {
	public:
		bool operator() (const BsaAsset first, const BsaAsset second) {
			return first.path == second.path;
		}
	};

	//Comparison class for list::sort by hash.
	bool hash_comp(const BsaAsset first, const BsaAsset second) {

		uint32_t f1 = *(uint32_t*)(&(first.hash));
		uint32_t f2 = *(uint32_t*)(&(first.hash) + sizeof(uint32_t));
		uint32_t s1 = *(uint32_t*)(&(second.hash));
		uint32_t s2 = *(uint32_t*)(&(second.hash) + sizeof(uint32_t));

		if (f1 < s1)
			return true;
		else if (f1 > s1)
			return false;
		else if (f2 < s2)
			return true;
		else if (f2 > s2)
			return false;

		return first.path < second.path;

		//return (first.hash > second.hash);
	}

	//Comparison class for list::sort by path.
	bool path_comp(const BsaAsset first, const BsaAsset second) {
		return first.path < second.path;
	}
}

bsa_handle_int::bsa_handle_int(const string path) :
	BSA(path),
	archiveFlags(0),
	fileFlags(0),
	hashOffset(0),
	extAssets(NULL),
	extAssetsNum(0),
	isTes3BSA(false) {
	
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
		*/

		// Before we load header into memory struct, check which struct to use.
		uint32_t magic;
		in.read((char*)&magic, sizeof(uint32_t));
		
		if (magic == tes3::BSA_VERSION_TES3)  //Magic is actually tes3 bsa version.
			isTes3BSA = true;
		else if (magic != tes4::BSA_MAGIC)
			throw error(LIBBSA_ERROR_FILE_READ_FAIL, path);

		if (isTes3BSA) {
			tes3::Header header;
			in.seekg(0, ios_base::beg);
			in.read((char*)&header, sizeof(tes3::Header));

			hashOffset = header.hashOffset;

			/* We want:
			- file names
			- file sizes
			- raw data offsets
			- file hashes

			Load the FileRecordData (size,offset), filename offsets, filename records and hashes into memory, then work on them there.
			*/
			tes3::FileRecord * fileRecords;
			uint32_t * filenameOffsets;
			uint8_t * filenameRecords;
			uint64_t * hashRecords;
			uint32_t filenameRecordsSize = header.hashOffset - sizeof(tes3::FileRecord) * header.fileCount - sizeof(uint32_t) * header.fileCount;
			try {
				fileRecords = new tes3::FileRecord[header.fileCount];
				in.read((char*)fileRecords, sizeof(tes3::FileRecord) * header.fileCount);
				
				filenameOffsets = new uint32_t[header.fileCount];
				in.read((char*)filenameOffsets, sizeof(uint32_t) * header.fileCount);
				
				filenameRecords = new uint8_t[filenameRecordsSize];
				in.read((char*)filenameRecords, sizeof(uint8_t) * filenameRecordsSize);

				hashRecords = new uint64_t[header.fileCount];
				in.read((char*)hashRecords, sizeof(uint64_t) * header.fileCount);
			} catch (bad_alloc &e) {
				throw error(LIBBSA_ERROR_NO_MEM);
			}

			in.close(); //No longer need the file open.

	ofstream debug("debug.txt");
	debug << "Original Hash" << '\t' << "Calc'ed Hash" << endl;

			//All three arrays have the same ordering, so we just need to loop through one and look at the corresponding position in the other.
			uint32_t startOfData = sizeof(tes3::Header) + header.hashOffset + header.fileCount * sizeof(uint64_t);
			for (uint32_t i=0; i < header.fileCount; i++) {
				BsaAsset fileData;
				fileData.size = fileRecords[i].size;
				fileData.offset = startOfData + fileRecords[i].offset;  //Internally, offsets are adjusted so that they're from file beginning.
				fileData.hash = hashRecords[i];

				//Now we need to build the file path. First: file name.
				//Find position of null pointer.
				char * nptr = strchr((char*)(filenameRecords + filenameOffsets[i]), '\0');
				if (nptr == NULL)
					throw error(LIBBSA_ERROR_FILE_READ_FAIL, path);

				fileData.path = trans.EncToUtf8(string((char*)(filenameRecords + filenameOffsets[i]), nptr - (char*)(filenameRecords + filenameOffsets[i])));
				
	//Test libbsa's hash function.
	uint64_t hash = tes3::CalcHash(fileData.path);
	if (fileData.hash != hash)
		debug << fileData.hash << '\t' << hash << endl;

//	debug << fileData.path << '\t' << fileData.hash << '\t' << fileData.size << '\t' << fileData.offset << endl;

				//Finally, add fileData to list.
				assets.push_back(fileData);
			}

	debug.close();

			delete [] fileRecords;
			delete [] filenameOffsets;
			delete [] filenameRecords;
			delete [] hashRecords;

		} else {
			tes4::Header header;
			in.read((char*)&header, sizeof(tes4::Header));
			
			if ((header.version != tes4::BSA_VERSION_TES4 && header.version != tes4::BSA_VERSION_TES5) || header.offset != tes4::BSA_FOLDER_RECORD_OFFSET)
				throw error(LIBBSA_ERROR_FILE_READ_FAIL, path);

			//Record the file and archive flags.
			fileFlags = header.fileFlags;
			archiveFlags = header.archiveFlags;

			//Now we get to the real meat of the file.
			//Folder records are followed by file records in blocks by folder name, followed by file names.
			//File records and file names have the same ordering.
			tes4::FolderRecord * folderRecords;
			uint8_t * fileRecords;
			uint8_t * fileNames;	//A list of null-terminated filenames, one after another.
			//Three terms are the number of folder name string length bytes, the total length of folder strings and the total number of file records.
			uint32_t startOfFileRecords = sizeof(tes4::Header) + sizeof(tes4::FolderRecord) * header.folderCount;
			uint32_t fileRecordsSize = header.folderCount * sizeof(uint8_t) + header.totalFolderNameLength + sizeof(tes4::FileRecord) * header.fileCount;
			uint32_t startOfFileNames = startOfFileRecords + fileRecordsSize;
			try {
				folderRecords = new tes4::FolderRecord[header.folderCount];
				in.read((char*)folderRecords, sizeof(tes4::FolderRecord) * header.folderCount);
				
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
				
			After we've processed all the files in each folder, we can discard their numbers.
			After we've processed all the file records for each folder, we can discard their offsets.
			We want to keep everything else.
			
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
				string folderName = trans.EncToUtf8(string((char*)(fileRecords + startOfFileRecordBlock + 1), folderNameLength));
				
				//Now loop through the file records for this folder. For each file record, look up the corresponding filename. Keep a counter outside the Tes4FolderRecord loop as the filename list is for all folders.
				uint32_t j = startOfFileRecordBlock + 1 + folderNameLength + 1 + (uint32_t)fileRecords;
				uint32_t max = j + folderRecords[i].count * sizeof(tes4::FileRecord);
				while (j < max) {
					//Fill in file data.
					BsaAsset fileData;
					fileData.hash = *(uint64_t*)j;
					fileData.size = *(uint32_t*)(j + sizeof(uint64_t));
					fileData.offset = *(uint32_t*)(j + sizeof(uint64_t) + sizeof(uint32_t));
						
					//Now we need to build the file path. First: file name.
					//Find position of null pointer.
					char * nptr = strchr((char*)(fileNames + fileNameListPos), '\0');
					if (nptr == NULL)
						throw error(LIBBSA_ERROR_FILE_READ_FAIL, path);

					fileData.path += trans.EncToUtf8(string((char*)(fileNames + fileNameListPos), nptr - (char*)(fileNames + fileNameListPos)));

		//Do hash test.
		if (fileData.hash != tes4::CalcHash(fileData.path))
			throw exception("hash mismatch!");

					if (!folderName.empty())
						fileData.path = folderName + '\\' + fileData.path;

					//Finally, add file path and object to list.
					assets.push_back(fileData);
				
					j += sizeof(tes4::FileRecord);
					fileNameListPos += fileData.path.length() + 1;
				}
			}
			
			delete [] folderRecords;
			delete [] fileRecords;
			delete [] fileNames;
		}
	}

	extAssets = NULL;
	extAssetsNum = 0;
}

bsa_handle_int::~bsa_handle_int() {
	for (size_t i=0; i < extAssetsNum; i++)
		delete [] extAssets[i];
	delete [] extAssets;
}

void bsa_handle_int::Save(std::string path, const uint32_t version, const uint32_t compression) {
	//Version and compression have been validated.

	if (path == filePath)
		path += ".new";  //Avoid read/write collisions.

	ifstream in(filePath.c_str(), ios::binary);
	in.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.
		
	ofstream out(path.c_str(), ios::binary | ios::trunc);
	out.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

	if (version == LIBBSA_VERSION_TES3) {
		//Build file header.
		tes3::Header header;
		header.version = tes3::BSA_VERSION_TES3;
		header.fileCount = assets.size();
		//Can't set hashOffset until the size of the names array is known.

		//Allocate memory for info blocks.
		tes3::FileRecord * fileRecords;
		uint32_t * filenameOffsets;
		uint64_t * hashes;
		//Can't allocate memory for filenames, because we don't know how long they are to be. Use a string buffer instead.
		string filenameRecords;
		try {
			fileRecords = new tes3::FileRecord[header.fileCount];
			filenameOffsets = new uint32_t[header.fileCount];
			hashes = new uint64_t[header.fileCount];
		} catch (bad_alloc &e) {
			throw error(LIBBSA_ERROR_NO_MEM);
		}

		//Need to update the file data offsets before populating the records. This requires the list to be sorted by path.
		//We still want to keep the old offsets for writing the raw file data though.
		assets.sort(path_comp);
		uint32_t fileDataOffset = 0;
		vector<uint32_t> oldOffsets;
		for (list<BsaAsset>::iterator it = assets.begin(), endIt = assets.end(); it != endIt; ++it) {
			oldOffsets.push_back(it->offset);
			it->offset = fileDataOffset;  //This results in the wrong offsets for 1185 files in Morrowind.bsa.
			fileDataOffset += it->size;
		}

		//file data, names and hashes are all done in hash order, so sort list by hash.
		assets.sort(hash_comp);
		uint32_t filenameOffset = 0;
		uint32_t i = 0;
		for (list<BsaAsset>::iterator it = assets.begin(), endIt = assets.end(); it != endIt; ++it) {
			
			//Set size and offset.
			fileRecords[i].size = it->size;
			fileRecords[i].offset = it->offset;
			
			//Set filename offset, and store filename.
			filenameOffsets[i] = filenameOffset;

			//Transcode.
			string filename = trans.Utf8ToEnc(it->path) + '\0';
			filenameRecords += filename;
			filenameOffset += filename.length();
			
			hashes[i] = it->hash;
			i++;
		}

		//We can now calculate the header's hashOffset to complete it.
		header.hashOffset = (sizeof(tes3::FileRecord) + sizeof(uint32_t)) * header.fileCount + filenameRecords.length();

		//Only thing left to do is to write out the completed BSA sections, then
		//read the file data from the input and write it to the output.

		//Write out completed BSA sections.
		out.write((char*)&header, sizeof(tes3::Header));   //ok
		out.write((char*)fileRecords, sizeof(tes3::FileRecord) * header.fileCount);  //offsets are wrong
		out.write((char*)filenameOffsets, sizeof(uint32_t) * header.fileCount);  //ok
		out.write(filenameRecords.data(), filenameRecords.length());  //OK.
		out.write((char*)hashes, sizeof(uint64_t) * header.fileCount);       //wrong

		//Now write out raw file data in alphabetical filename order.
		assets.sort(path_comp);
		i = 0;
		for (list<BsaAsset>::iterator it = assets.begin(), endIt = assets.end(); it != endIt; ++it) {
			//it->second.offset is the offset for the data in the new file. We don't need it though, because we're doing writes in sequence.
			//We want the offset for the data in the old file.

			//Allocate memory for this file's data, read it in, write it out, then free memory.
			uint8_t * fileData;
			try {
				fileData = new uint8_t[it->size];  //Doesn't matter where we get size from.
			} catch (bad_alloc &e) {
				throw error(LIBBSA_ERROR_NO_MEM);
			}

			//Read data in.
			in.seekg(oldOffsets[i], ios_base::beg);
			in.read((char*)fileData, it->size);

			//Write data out.
			out.write((char*)fileData, it->size);

			//Free memory.
			delete [] fileData;

			i++;
		}

		//Update member vars.
		isTes3BSA = true;
		filePath = path;
		hashOffset = header.hashOffset;
	} else {

		///////////////////////////////
		// Set header up
		///////////////////////////////

		tes4::Header header;

		header.fileId = tes4::BSA_MAGIC;

		if (version == LIBBSA_VERSION_TES4)
			header.version = tes4::BSA_VERSION_TES4;
		else if (version == LIBBSA_VERSION_TES5)
			header.version = tes4::BSA_VERSION_TES5;

		header.offset = 36;

		header.archiveFlags = archiveFlags;
		if (compression == LIBBSA_COMPRESS_LEVEL_0 && header.archiveFlags & tes4::BSA_COMPRESSED)
			header.archiveFlags ^= tes4::BSA_COMPRESSED;
		else if (compression != LIBBSA_COMPRESS_LEVEL_0 && !(header.archiveFlags & tes4::BSA_COMPRESSED))
			header.archiveFlags |= tes4::BSA_COMPRESSED;

		//Need to sort folder and file names separately into hash-sorted sets before header.folderCount and name lengths can be set.
		bool(*hash_comp_p)(BsaAsset,BsaAsset) = hash_comp;
		set<BsaAsset, bool(*)(BsaAsset,BsaAsset)> folderHashset(hash_comp_p);
		set<BsaAsset, bool(*)(BsaAsset,BsaAsset)> fileHashset(hash_comp_p);
		for (list<BsaAsset>::iterator it = assets.begin(), endIt = assets.end(); it != endIt; ++it) {
			BsaAsset folderAsset;
			BsaAsset fileAsset;

			//Transcode paths.
			folderAsset.path = trans.Utf8ToEnc(fs::path(it->path).parent_path().string());
			fileAsset.path = trans.Utf8ToEnc(it->path); /*fs::path(it->path).filename().string();*/

			folderAsset.hash = tes4::CalcHash(folderAsset.path);
			fileAsset.hash = it->hash;

			fileAsset.size = it->size;
			fileAsset.offset = it->offset;

			folderHashset.insert(folderAsset);  //Size and offset are zero for now.
			fileHashset.insert(fileAsset);
		}
		header.folderCount = folderHashset.size();

		header.fileCount = assets.size();

		header.totalFolderNameLength = 0;
		for (set<BsaAsset, bool(*)(BsaAsset,BsaAsset)>::iterator it = folderHashset.begin(), endIt=folderHashset.end(); it != endIt; ++it) {
			header.totalFolderNameLength += it->path.length() + 1;
		}

		header.totalFileNameLength = 0;
		for (set<BsaAsset, bool(*)(BsaAsset,BsaAsset)>::iterator it = fileHashset.begin(), endIt=fileHashset.end(); it != endIt; ++it) {
			header.totalFileNameLength += fs::path(it->path).filename().string().length() + 1;
		}

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
		string fileNameBlock;
		uint32_t fileRecordBlockOffset = header.totalFileNameLength;
		uint32_t fileDataOffset = sizeof(tes4::Header) + sizeof(tes4::FolderRecord) * header.folderCount + header.totalFolderNameLength + header.folderCount + sizeof(tes4::FileRecord) * header.fileCount + header.totalFileNameLength;
		list<BsaAsset> orderedAssets;
		for (set<BsaAsset, bool(*)(BsaAsset,BsaAsset)>::iterator it = folderHashset.begin(), endIt=folderHashset.end(); it != endIt; ++it) {
			size_t fileCount = 0;
			uint8_t nameLength = it->path.length() + 1;

			//Write folder name length, folder name to fileRecordBlocks stream.
			fileRecordBlocks.write((char*)&nameLength, 1);
			fileRecordBlocks.write((it->path + '\0').data(), nameLength);

			for (set<BsaAsset, bool(*)(BsaAsset,BsaAsset)>::iterator itr = fileHashset.begin(), endItr=fileHashset.end(); itr != endItr; ++itr) {
				if (fs::path(itr->path).parent_path().string() == it->path) {
					//Write file hash, size and offset to fileRecordBlocks stream.
					fileRecordBlocks.write((char*)&(itr->hash), sizeof(uint64_t));
					uint32_t size = itr->size;
					fileRecordBlocks.write((char*)&size, sizeof(uint32_t));
					fileRecordBlocks.write((char*)&fileDataOffset, sizeof(uint32_t));
					//Increment count and data offset.
					fileCount++;
					fileDataOffset += size;
					//Add record data to vector for later ordered extraction.
					orderedAssets.push_back(*itr);
					orderedAssets.back().offset = fileDataOffset;  //Can't update the offset in the set.
					//Also write out filename to fileNameBlock.
					fileNameBlock += fs::path(itr->path).filename().string() + '\0';
				}
			}

			//Now write folder record.
			folderRecords.write((char*)&(it->hash), sizeof(uint64_t));
			folderRecords.write((char*)&fileCount, sizeof(uint32_t));
			folderRecords.write((char*)&fileRecordBlockOffset, sizeof(uint32_t));

			//Increment fileRecordBlockOffset.
			fileRecordBlockOffset = fileRecordBlocks.str().length();
		}

		////////////////////////
		// Write out
		////////////////////////

		out.write((char*)&header, sizeof(tes4::Header));
		out.write(folderRecords.str().data(), sizeof(tes4::FolderRecord) * header.folderCount);
		out.write(fileRecordBlocks.str().data(), fileRecordBlocks.str().length());
		out.write(fileNameBlock.data(), header.totalFileNameLength);

		//Now write out raw file data in the same order it was listed in the FileRecordBlocks.
		for (list<BsaAsset>::iterator it = orderedAssets.begin(), endIt = orderedAssets.end(); it != endIt; ++it) {
			//Allocate memory for this file's data, read it in, write it out, then free memory.
			uint8_t * fileData;
			try {
				fileData = new uint8_t[it->size];  //Doesn't matter where we get size from.
			} catch (bad_alloc &e) {
				throw error(LIBBSA_ERROR_NO_MEM);
			}

			//Get the old BSA's file data offset.
			list<BsaAsset>::iterator itr, endItr;
			for (itr = assets.begin(), endItr = assets.end(); itr != endItr; ++itr) {
				if (itr->path == it->path)
					break;
			}

			if (itr == endItr)
				throw error(LIBBSA_ERROR_FILE_NOT_FOUND, it->path);

			//Read data in.
			in.seekg(itr->offset, ios_base::beg);  //This is the offset in the old BSA.
			in.read((char*)fileData, it->size);

			//Write data out.
			out.write((char*)fileData, it->size);

			//Free memory.
			delete [] fileData;

			//Update the stored offset.
			itr->offset = it->offset;
		}

		//Update member vars.
		isTes3BSA = false;
		filePath = path;
		archiveFlags = header.archiveFlags;
		fileFlags = header.fileFlags;
	}
	
	in.close();
	out.close();

	//Now rename the output file.
	if (fs::path(path).extension().string() == ".new") {
		try {
			fs::rename(path, fs::path(path).stem());
		} catch (fs::filesystem_error& e) {
			throw LIBBSA_ERROR_FILE_WRITE_FAIL;
		}
	}
}

void bsa_handle_int::ExtractFromStream(ifstream& in, const BsaAsset data, const std::string outPath) {
	//Create parent directories.
	fs::create_directories(fs::path(outPath).parent_path());  //This creates any directories in the path that don't already exist.

	if (fs::exists(outPath))
		throw error(LIBBSA_ERROR_FILE_WRITE_FAIL, outPath);

	uint32_t size = data.size;
	//Check if given file is compressed or not. If not, can ofstream straight to path, otherwise need to involve zlib.
	/* BSA-TYPE-SPECIFIC CHECK */
	if (isTes3BSA || (archiveFlags & tes4::BSA_COMPRESSED && data.size & tes4::FILE_INVERT_COMPRESSED) || !(archiveFlags & tes4::BSA_COMPRESSED)) {
		//Just need to use size and offset to write to binary file stream.
		uint8_t * buffer;
		uint32_t offset = data.offset;

		/* BSA-TYPE-SPECIFIC CHECK */
		if (!isTes3BSA && size & tes4::FILE_INVERT_COMPRESSED)  //Remove compression flag from size to get actual size.
			size ^= tes4::FILE_INVERT_COMPRESSED;

		try {
			buffer = new uint8_t[size];
		} catch (bad_alloc &e) {
			throw error(LIBBSA_ERROR_NO_MEM);
		}

		in.seekg(offset, ios_base::beg);
		in.read((char*)buffer, size);

		try {
			ofstream out(outPath.c_str(), ios::binary | ios::trunc);
			out.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

			out.write((char*)buffer, size);

			out.close();
		} catch (ios_base::failure& e) {
			throw error(LIBBSA_ERROR_FILE_WRITE_FAIL, outPath);
		}

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
		size -= sizeof(uint32_t);  //First uint32_t of data is the size of the uncompressed data.
		try {
			compressedFile = new uint8_t[size];
			uncompressedFile = new uint8_t[uncompressedSize];
		} catch (bad_alloc &e) {
			throw error(LIBBSA_ERROR_NO_MEM);
		}

		in.read((char*)compressedFile, size);

		//We can use a pre-made utility function instead of having to mess around with zlib proper.
		int ret = uncompress(uncompressedFile, (uLongf*)&uncompressedSize, compressedFile, size);
		if (ret != Z_OK)
			throw error(LIBBSA_ERROR_ZLIB_ERROR);

		try {
			//Now output to file.
			ofstream out(outPath.c_str(), ios::binary | ios::trunc);
			out.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

			out.write((char*)uncompressedFile, uncompressedSize);
			out.close();
		} catch (ios_base::failure& e) {
			throw error(LIBBSA_ERROR_FILE_WRITE_FAIL, outPath);
		}

		//Free memory.
		delete [] compressedFile;
		delete [] uncompressedFile;
	}
}