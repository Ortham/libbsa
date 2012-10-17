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

#include "tes4bsa.h"
#include "exception.h"
#include "libbsa.h"
#include <fstream>
#include <vector>
#include <cstring>
#include <boost/filesystem.hpp>
#include <zlib.h>

namespace fs = boost::filesystem;

using namespace std;

namespace libbsa { namespace tes4 {

	BSA::BSA(const std::string path) 
		: bsa_handle_int(path),
		archiveFlags(0),
		fileFlags(0) {

		//Set transcoding up.
		trans.SetEncoding(1252);

		//Check if file exists.
		if (fs::exists(path)) {
	
			ifstream in(path.c_str(), ios::binary);
			in.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

			Header header;
			in.seekg(0, ios_base::beg);
			in.read((char*)&header, sizeof(Header));
			
			if ((header.version != BSA_VERSION_TES4 && header.version != BSA_VERSION_TES5) || header.offset != BSA_FOLDER_RECORD_OFFSET)
				throw error(LIBBSA_ERROR_FILE_READ_FAIL, path);

			//Now we get to the real meat of the file.
			//Folder records are followed by file records in blocks by folder name, followed by file names.
			//File records and file names have the same ordering.
			FolderRecord * folderRecords;
			uint8_t * fileRecords;
			uint8_t * fileNames;	//A list of null-terminated filenames, one after another.
			uint32_t fileRecordsSize = 
				header.folderCount + //Folder name string length (in 1 byte).
				header.totalFolderNameLength + //Total length of folder name strings.
				sizeof(FileRecord) * header.fileCount;  //Total size of all file records.
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

			in.close(); //No longer need the file open.

			/* Loop through the folder records, for each folder looking up the file records associated with it,
			and the filenames associated with those records. */
			uint32_t fileNameListPos = 0;
			uint32_t startOfFileRecords = sizeof(Header) + sizeof(FolderRecord) * header.folderCount;
			for (uint32_t i=0; i < header.folderCount; i++) {
				/* folderRecords[i].count gives the number of file records associated with this folder.
					folderRecords[i].offset gives the offset to the file records associated with this folder,
					from the beginning of the file, plus the total filenames length.
					folderRecords[i].hash can be discarded. */

				folderRecords[i].offset -= header.totalFileNameLength + startOfFileRecords;  //Get rid of this first.

				//Need to get folder name to add before file name in internal data store.
				uint8_t folderNameLength = *(fileRecords + folderRecords[i].offset) - 1;
				string folderName = trans.EncToUtf8(string((char*)(fileRecords + folderRecords[i].offset + 1), folderNameLength));

				//Now loop through file records for this folder record.
				uint32_t startOfFolderFileRecords = (uint32_t)fileRecords + folderRecords[i].offset + folderNameLength + 2;
				for (uint32_t j=0; j < folderRecords[i].count; j++) {
					BsaAsset fileData;
					FileRecord fr = *(FileRecord*)(startOfFolderFileRecords + j * sizeof(FileRecord));
					fileData.hash = fr.nameHash;
					fileData.size = fr.size;
					fileData.offset = fr.offset;

					//Now we need to build the file path. First: file name.
					char * filenameStart = (char*)(fileNames + fileNameListPos);
					//Find position of null pointer.
					char * nptr = strchr(filenameStart, '\0');
					if (nptr == NULL)
						throw error(LIBBSA_ERROR_FILE_READ_FAIL, path);

					fileData.path += trans.EncToUtf8(string(filenameStart, nptr - filenameStart));
					fileNameListPos += fileData.path.length() + 1;

					if (!folderName.empty())
						fileData.path = folderName + '\\' + fileData.path;

					//Finally, add file path and object to list.
					assets.push_back(fileData);
				}
			}

			//Record the file and archive flags.
			fileFlags = header.fileFlags;
			archiveFlags = header.archiveFlags;
			
			delete [] folderRecords;
			delete [] fileRecords;
			delete [] fileNames;
		}
	}

	void BSA::Save(std::string path, const uint32_t version, const uint32_t compression) {
				//Version and compression have been validated.

		if (path == filePath)
			path += ".new";  //Avoid read/write collisions.

		ifstream in(filePath.c_str(), ios::binary);
		in.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.
		
		ofstream out(path.c_str(), ios::binary | ios::trunc);
		out.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.
	
		///////////////////////////////
		// Set header up
		///////////////////////////////

		Header header;

		header.fileId = BSA_MAGIC;

		if (version == LIBBSA_VERSION_TES4)
			header.version = BSA_VERSION_TES4;
		else if (version == LIBBSA_VERSION_TES5)
			header.version = BSA_VERSION_TES5;

		header.offset = 36;

		header.archiveFlags = archiveFlags;
		if (compression != LIBBSA_COMPRESS_LEVEL_NOCHANGE) {
			if (compression == LIBBSA_COMPRESS_LEVEL_0 && header.archiveFlags & BSA_COMPRESSED)
				header.archiveFlags ^= BSA_COMPRESSED;
			else if (compression != LIBBSA_COMPRESS_LEVEL_0 && !(header.archiveFlags & BSA_COMPRESSED))
				header.archiveFlags |= BSA_COMPRESSED;
		}

		//Need to sort folder and file names separately into hash-sorted sets before header.folderCount and name lengths can be set.
		list<BsaAsset> folderHashset;
		list<BsaAsset> fileHashset;
		for (list<BsaAsset>::iterator it = assets.begin(), endIt = assets.end(); it != endIt; ++it) {
			BsaAsset folderAsset;
			BsaAsset fileAsset;

			//Transcode paths.
			folderAsset.path = trans.Utf8ToEnc(fs::path(it->path).parent_path().string());
			fileAsset.path = trans.Utf8ToEnc(it->path); /*fs::path(it->path).filename().string();*/

			folderAsset.hash = CalcHash(folderAsset.path, "");
			fileAsset.hash = it->hash;

			fileAsset.size = it->size;
			fileAsset.offset = it->offset;

			folderHashset.push_back(folderAsset);  //Size and offset are zero for now.
			fileHashset.push_back(fileAsset);
		}
		path_comp is_same_file;
		folderHashset.unique(is_same_file);
		fileHashset.unique(is_same_file);
		header.folderCount = folderHashset.size();

		header.fileCount = assets.size();

		header.totalFolderNameLength = 0;
		for (list<BsaAsset>::iterator it = folderHashset.begin(), endIt=folderHashset.end(); it != endIt; ++it) {
			header.totalFolderNameLength += it->path.length() + 1;
		}

		header.totalFileNameLength = 0;
		for (list<BsaAsset>::iterator it = fileHashset.begin(), endIt=fileHashset.end(); it != endIt; ++it) {
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

		FolderRecord * folderRecords;
		uint8_t * fileRecordBlocks;
		uint8_t * fileNames;
		uint32_t fileRecordBlocksSize = header.folderCount + header.totalFolderNameLength + header.fileCount * sizeof(FileRecord);
		try {
			folderRecords = new FolderRecord[header.folderCount];
			fileRecordBlocks = new uint8_t[fileRecordBlocksSize];
			fileNames = new uint8_t[header.totalFileNameLength];
		} catch (bad_alloc &e) {
			throw error(LIBBSA_ERROR_NO_MEM);
		}

		uint32_t startOfFileRecordBlock = sizeof(Header) + header.folderCount * sizeof(FolderRecord) + header.totalFileNameLength;  //For some reason offsets include this.
		uint32_t fileDataOffset = startOfFileRecordBlock + fileRecordBlocksSize;
		list<BsaAsset> orderedAssets;
		uint32_t i = 0;
		uint32_t currFileRecordBlockPos = 0;
		uint32_t currFileNamePos = 0;
		folderHashset.sort(hash_comp);
		fileHashset.sort(hash_comp);
		for (list<BsaAsset>::iterator it = folderHashset.begin(), endIt=folderHashset.end(); it != endIt; ++it) {
			//Write folder hash and offset, write count later.
			folderRecords[i].nameHash = it->hash;
			folderRecords[i].offset = startOfFileRecordBlock + currFileRecordBlockPos;

			//Write folder name length, folder name to fileRecordBlocks buffer.
			size_t fileCount = 0;
			uint8_t nameLength = it->path.length() + 1;
			fileRecordBlocks[currFileRecordBlockPos] = nameLength;
			currFileRecordBlockPos++;
			strcpy((char*)fileRecordBlocks + currFileRecordBlockPos, (it->path + '\0').data());
			currFileRecordBlockPos += nameLength;

			uint32_t j = 0;
			for (list<BsaAsset>::iterator itr = fileHashset.begin(), endItr=fileHashset.end(); itr != endItr; ++itr) {
				if (fs::path(itr->path).parent_path().string() == it->path) {
					//Write file hash, size and offset to fileRecordBlocks stream.
					memcpy(fileRecordBlocks + currFileRecordBlockPos, &(itr->hash), sizeof(uint64_t));
					currFileRecordBlockPos += sizeof(uint64_t);
					memcpy(fileRecordBlocks + currFileRecordBlockPos, &(itr->size), sizeof(uint32_t));
					currFileRecordBlockPos += sizeof(uint32_t);
					memcpy(fileRecordBlocks + currFileRecordBlockPos, &fileDataOffset, sizeof(uint32_t));
					currFileRecordBlockPos += sizeof(uint32_t);
					//Increment count and data offset.
					fileCount++;
					fileDataOffset += itr->size;
					//Add record data to list for later ordered extraction.
					orderedAssets.push_back(*itr);
					orderedAssets.back().offset = fileDataOffset;  //Can't update the offset in the set.
					//Also write out filename to fileNameBlock.
					string filename = fs::path(itr->path).filename().string() + '\0';
					strcpy((char*)fileNames + currFileNamePos, filename.data());
					currFileNamePos += filename.length();
				}
			}

			folderRecords[i].count = fileCount;

			i++;
		}

		////////////////////////
		// Write out
		////////////////////////

		out.write((char*)&header, sizeof(Header));
		out.write((char*)folderRecords, sizeof(FolderRecord) * header.folderCount);
		out.write((char*)fileRecordBlocks, fileRecordBlocksSize);
		out.write((char*)fileNames, header.totalFileNameLength);

		delete [] folderRecords;
		delete [] fileRecordBlocks;
		delete [] fileNames;

		//Now write out raw file data in the same order it was listed in the FileRecordBlocks.
		for (list<BsaAsset>::iterator it = orderedAssets.begin(), endIt = orderedAssets.end(); it != endIt; ++it) {
			//Allocate memory for this file's data, read it in, write it out, then free memory.
			//This doesn't yet support compression level changing or assets that have been added to the BSA.

			uint32_t size = it->size;
			if (size & FILE_INVERT_COMPRESSED)  //Remove compression flag from size to get actual size.
				size ^= FILE_INVERT_COMPRESSED;

			uint8_t * fileData;
			try {
				fileData = new uint8_t[size];
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
			in.read((char*)fileData, size);

			//Write data out.
			out.write((char*)fileData, size);

			//Free memory.
			delete [] fileData;

			//Update the stored offset.
			itr->offset = it->offset;
		}

		//Update member vars.
		filePath = path;
		archiveFlags = header.archiveFlags;
		fileFlags = header.fileFlags;

		in.close();
		out.close();

		//Now rename the output file.
	/*	if (fs::path(path).extension().string() == ".new") {
			try {
				fs::rename(path, fs::path(path).stem());
			} catch (fs::filesystem_error& e) {
				throw error(LIBBSA_ERROR_FILE_WRITE_FAIL, fs::path(path).stem().string());
			}
		}*/
	}

	void BSA::ExtractFromStream(std::ifstream& in, const libbsa::BsaAsset data, const std::string outPath) {
		//Create parent directories.
		fs::create_directories(fs::path(outPath).parent_path());  //This creates any directories in the path that don't already exist.

		if (fs::exists(outPath))
			throw error(LIBBSA_ERROR_FILE_WRITE_FAIL, outPath);

		uint32_t size = data.size;
		//Check if given file is compressed or not. If not, can ofstream straight to path, otherwise need to involve zlib.
		/* BSA-TYPE-SPECIFIC CHECK */
		if ((archiveFlags & BSA_COMPRESSED && size & FILE_INVERT_COMPRESSED) || (!(archiveFlags & BSA_COMPRESSED) && !(size & FILE_INVERT_COMPRESSED))) {
			//Just need to use size and offset to write to binary file stream.
			uint8_t * buffer;

			/* BSA-TYPE-SPECIFIC CHECK */
			if (size & FILE_INVERT_COMPRESSED)  //Remove compression flag from size to get actual size.
				size ^= FILE_INVERT_COMPRESSED;

			try {
				buffer = new uint8_t[size];
			} catch (bad_alloc &e) {
				throw error(LIBBSA_ERROR_NO_MEM);
			}

			in.seekg(data.offset, ios_base::beg);
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
			if (size & FILE_INVERT_COMPRESSED)  //Remove compression flag from size to get actual compressed size.
				size ^= FILE_INVERT_COMPRESSED;

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

	uint32_t BSA::HashString(std::string str) {
		uint32_t hash = 0;
		for (size_t i=0, len=str.length(); i < len; i++) {
			hash = 0x1003F * hash + (uint8_t)str[i];
		}
		return hash;
	}

	uint64_t BSA::CalcHash(std::string path, std::string ext) {
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

	bool hash_comp(const BsaAsset first, const BsaAsset second) {
		return first.hash < second.hash;
	}

	//Check if a given file is a Tes4-type BSA.
	bool IsBSA(std::string path) {
		//Check if file exists.
		if (!fs::exists(path))
			return false;
		else {
			uint32_t magic;
			ifstream in(path.c_str(), ios::binary);
			in.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

			in.read((char*)&magic, sizeof(uint32_t));
			in.close();
		
			return magic == BSA_MAGIC;  //Magic is actually tes3 bsa version.
		}
	}
}}