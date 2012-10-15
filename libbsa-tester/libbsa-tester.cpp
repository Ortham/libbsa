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

#include "libbsa.h"
#include <iostream>
#include <stdint.h>
#include <fstream>

using std::endl;

int main() {
	/* List of official BSAs for testing. 
	R = Reads OK, E = Extracts OK, W = Writes OK.
	!R = Doesn't read OK, !E and !W similar.
	If a bsa is missing a R/!R, E/!E, W/!W, it means that hasn't been tested yet.
	Morrowind:
		Morrowind.bsa                           R	E	!W
		Bloodmoon.bsa                           R	E	!W
		Tribunal.bsa                            R	E	W

	Oblivion:
		Oblivion - Meshes.bsa                   R	E
		Oblivion - Misc.bsa                     R	E
		Oblivion - Sounds.bsa                   R	E
		Oblivion - Textures - Compressed.bsa    R	E
		Oblivion - Voices1.bsa                  R	E
		Oblivion - Voices2.bsa                  R	E
		Knights.bsa                             R	E	!W
		DLCShiveringIsles - Meshes.bsa          R	E
		DLCShiveringIsles - Sounds.bsa          R	E
		DLCShiveringIsles - Textures.bsa        R	E
		DLCShiveringIsles - Voices.bsa          R	E

	Skyrim:
		Skyrim - Animations.bsa                 R	E
		Skyrim - Interface.bsa                  R	E
		Skyrim - Meshes.bsa                     R	E
		Skyrim - Misc.bsa                       R	E
		Skyrim - Shaders.bsa                    R	E
		Skyrim - Sounds.bsa                     R	E
		Skyrim - Textures.bsa                   R	E
		Skyrim - Voices.bsa                     R	E
		Skyrim - VoicesExtra.bsa                R	E
		Update.bsa                              R	E
	*/

	uint8_t * path = reinterpret_cast<uint8_t *>("C:\\Users\\Oliver\\Downloads\\Libbsa Testing\\BSAs\\Skyrim - Misc.bsa");
	uint8_t * outPath = reinterpret_cast<uint8_t *>("C:\\Users\\Oliver\\Downloads\\Libbsa Testing\\BSAs\\Skyrim - Misc.bsa.new");
//	uint8_t * asset = reinterpret_cast<uint8_t *>("meshes/m/probe_journeyman_01.nif");
//	uint8_t * extPath = reinterpret_cast<uint8_t *>("C:\\Users\\Oliver\\Downloads\\probe_journeyman_01.nif.extract");
	uint8_t * destPath = reinterpret_cast<uint8_t *>("C:\\Users\\Oliver\\Downloads\\Libbsa Testing\\BSAs\\Skyrim - Misc");
	bsa_handle bh;
	uint32_t ret;
	size_t numAssets;
	uint8_t * contentPath = reinterpret_cast<uint8_t *>(".+");
	uint8_t * err;
	uint8_t ** assetPaths;
	bool result;

	std::ofstream out("libbsa-tester.txt");
	if (!out.good()){
		std::cout << "File could not be opened for reading.";
		exit(1);
	}

	out << "Using path: " << path << endl;

	out << "TESTING OpenBSA(...)" << endl;
	ret = OpenBSA(&bh, path);
	if (ret != LIBBSA_OK)
		out << '\t' << "OpenBSA(...) failed! Return code: " << ret << endl;
	else {
		out << '\t' << "OpenBSA(...) successful!" << endl;
/*
		out << "TESTING GetAssets(...)" << endl;
		ret = GetAssets(bh, contentPath, &assetPaths, &numAssets);
		if (ret != LIBBSA_OK)
			out << '\t' << "GetAssets(...) failed! Return code: " << ret << endl;
		else {
			out << '\t' << "GetAssets(...) successful! Number of paths: " << numAssets << endl;
			for (size_t i=0; i < numAssets; i++) {
				out << '\t' << assetPaths[i] << endl;
			}
		}

		out << "TESTING IsAssetInBSA(...)" << endl;
		ret = IsAssetInBSA(bh, asset, &result);
		if (ret != LIBBSA_OK)
			out << '\t' << "IsAssetInBSA(...) failed! Return code: " << ret << endl;
		else
			out << '\t' << "IsAssetInBSA(...) successful! Is \"" << asset << "\" in BSA: " << result << endl;

		out << "TESTING ExtractAsset(...)" << endl;
		ret = ExtractAsset(bh, asset, extPath);
		if (ret != LIBBSA_OK)
			out << '\t' << "ExtractAsset(...) failed! Return code: " << ret << endl;
		else
			out << '\t' << "ExtractAsset(...) successful!" << endl;

		out << "TESTING ExtractAssets(...)" << endl;
		ret = ExtractAssets(bh, contentPath, destPath, &assetPaths, &numAssets);
		if (ret != LIBBSA_OK)
			out << '\t' << "ExtractAssets(...) failed! Return code: " << ret << endl;
		else {
			out << '\t' << "ExtractAssets(...) successful! Number of paths: " << numAssets << endl;
			for (size_t i=0; i < numAssets; i++) {
				out << '\t' << assetPaths[i] << endl;
			}
		}
*/	
		out << "TESTING SaveBSA(...)" << endl;
		ret = SaveBSA(bh, outPath, LIBBSA_VERSION_TES3 | LIBBSA_COMPRESS_LEVEL_0);
		if (ret != LIBBSA_OK)
			out << '\t' << "SaveBSA(...) failed! Return code: " << ret << endl;
		else
			out << '\t' << "SaveBSA(...) successful!" << endl;

		out << "TESTING CloseBSA(...)" << endl;
		CloseBSA(bh);
	}

	out.close();
	return 0;
}