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

#ifndef LIBBSA_BSAHANDLE_H
#define LIBBSA_BSAHANDLE_H

#include "helpers.h"
#include "genericbsa.h"
#include <stdint.h>
#include <string>
#include <fstream>
#include <list>
#include <boost/regex.hpp>

/* This handle combines the generic BSA handling (in genericbsa.h) with the 
   handling for specific BSA types (what is given in tes3bsa.h, tes4bsa.h).
   It also provides the memory management for the library's frontend.
   
   As in genericbsa.h, all strings are encoded in UTF-8: transcoding happens
   on load/save.

   The following functions contain BSA-type-specific code:
   bsa_handle_int(const std::string path)
   Save(std::string path, const uint32_t version, const uint32_t compression)
   ExtractFromStream(std::ifstream& in, const libbsa::BsaAsset data, const std::string outPath)

   The private member variables are also BSA-type-specific.
*/

struct bsa_handle_int : public libbsa::BSA {
public:
	bsa_handle_int(const std::string path);
	~bsa_handle_int();
	
	void Save(std::string path, const uint32_t version, const uint32_t compression);

	//External data array pointers and sizes.
	uint8_t ** extAssets;
	size_t extAssetsNum;
private:
	void ExtractFromStream(std::ifstream& in, const libbsa::BsaAsset data, const std::string outPath);

	bool isTes3BSA;

	//Some TES4-type-specific BSA data.
	uint32_t archiveFlags;
	uint32_t fileFlags;

	//Some TES3-type-specific BSA data.
	uint32_t hashOffset;

	//Utility functions for string type/encoding conversion.
	libbsa::Transcoder trans;  //For Windows-1252 <-> UTF-8 encoding conversion.
};
#endif