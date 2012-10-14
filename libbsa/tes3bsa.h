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

#ifndef LIBBSA_TES3STRUCTS_H
#define LIBBSA_TES3STRUCTS_H

#include <stdint.h>
#include <string>

/* File format infos:
	<http://www.uesp.net/wiki/Tes3Mod:BSA_File_Format>

	This header file defines the constants, structures and functions specific 
	to the Tes3-type BSA, which is used by Morrowind.
*/

namespace libbsa { namespace tes3 {

	const uint32_t BSA_VERSION_TES3 = 0x100;

	struct Header {
		uint32_t version;
		uint32_t hashOffset;
		uint32_t fileCount;
	};

	struct FileRecord {
		uint32_t size;
		uint32_t offset;
	};

	inline uint64_t CalcHash(const std::string path) {
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

		return ((uint64_t)hash1) + ((uint64_t)hash2 << 32);
	}
} }

#endif