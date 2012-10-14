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

#ifndef LIBBSA_GENERICBSA_H
#define LIBBSA_GENERICBSA_H

#include <stdint.h>
#include <string>
#include <list>
#include <boost/regex.hpp>

/* This header declares the generic structures that libbsa uses to handle BSA 
   manipulation. 
   All strings are encoded in UTF-8.
*/

namespace libbsa {

	//Class for generic BSA data.
	//Files that have not yet been written have 0 hash, size and offset.
	struct BsaAsset {
		BsaAsset();

		//Asset data obtained from BSA.
		std::string path;
		uint64_t hash;
		uint32_t size;					//Files that have not yet been written to the BSA file have a size of 0.
		uint32_t offset;				//This offset is from the beginning of the file - Tes3 BSAs use from the beginning of the data section,
										//so will have to adjust them. Files that have not yet been written to the BSA have a 0 offset.
	};

	struct PendingBsaAsset {
		std::string extPath;  //Path of file in filesystem.
		std::string intPath;  //Path of file in BSA.
	};

	//Class for generic BSA data manipulation functions.
	class BSA {
	public:
		BSA(const std::string path);

		bool HasAsset(const std::string assetPath);
		BsaAsset GetAsset(const std::string assetPath);
		void GetMatchingAssets(const boost::regex regex, std::list<BsaAsset> &matchingAssets);
	
		void Extract(const std::string assetPath, const std::string destPath);
		void Extract(const std::list<libbsa::BsaAsset> &assetsToExtract, const std::string destPath);

		virtual void ExtractFromStream(std::ifstream& in, const libbsa::BsaAsset data, const std::string outPath) = 0;
	protected:
		std::string filePath;
		std::list<BsaAsset> assets;			//Files not yet written to the BSA are in this and pendingAssets.
		std::list<PendingBsaAsset> pendingAssets;  //Holds the internal->external path mapping for files not yet written to the BSA.
	};

}


#endif