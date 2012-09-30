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

#ifndef LIBBSA_FORMAT_H
#define LIBBSA_FORMAT_H

#include "helpers.h"
#include <stdint.h>
#include <string>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

struct FileRecordData {  //Who cares about the hash, anyway?
	uint32_t size;
	uint32_t offset;
};

//File data is written on load and save. All strings are Windows-1252 encoded.
struct bsa_handle_int {
	bsa_handle_int(const std::string path);
	~bsa_handle_int();

	void Save(std::string path, const uint32_t flags);

	uint8_t * GetString(std::string str);

	void Extract(FileRecordData data, std::string outPath);

	void Extract(boost::unordered_map<std::string, FileRecordData>& data, std::string outPath);
	
	//Generic File data.
	bool isTes3BSA;
	std::string filePath;
	boost::unordered_map<std::string, FileRecordData> paths;

	//Tes4 file data.
	uint32_t archiveFlags;
	uint32_t fileFlags;

	//Tes3 file data.
	uint32_t hashOffset;
	uint32_t fileCount;

	//Outputted strings/arrays.
	uint8_t ** extAssets;
	size_t extAssetsNum;

	//For Windows-1252 <-> UTF-8 encoding conversion.
	libbsa::Transcoder trans;
	
	private:
		// Calculates a mini-hash.
		uint32_t Tes4HashString(std::string str);
		
		//Implemented following the Python example here:
		//<http://www.uesp.net/wiki/Tes4Mod:BSA_File_Format>
		uint64_t CalcTes4Hash(std::string path);
};
#endif