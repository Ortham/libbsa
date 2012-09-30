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

struct FileRecordData {  //Who cares about the hash, anyway?
	uint32_t size;
	uint32_t offset;
};

struct bsa_handle_int {
	bsa_handle_int(const std::string path);
	~bsa_handle_int();

	void Save(std::string path, const uint32_t flags);

	uint8_t * GetString(std::string str);

	void Extract(uint32_t size, uint32_t offset, std::string outPath);
	
	//File data.
	uint32_t archiveFlags;
	uint32_t fileFlags;
	boost::unordered_map<std::string, FileRecordData> paths;

	//Outputted strings/arrays.
	uint8_t ** extAssets;
	size_t extAssetsNum;

	//For Windows-1252 <-> UTF-8 encoding conversion.
	libbsa::Transcoder trans;
	
	private:
		// Calculates a mini-hash.
		uint32_t HashString(std::string str);
		
		//Implemented following the Python example here:
		//<http://www.uesp.net/wiki/Tes4Mod:BSA_File_Format>
		uint64_t CalcHash(std::string path);
};
#endif