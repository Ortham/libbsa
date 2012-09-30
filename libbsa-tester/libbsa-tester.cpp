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

#include <iostream>
#include <stdint.h>
#include <fstream>
#include <clocale>
#include <boost/filesystem/detail/utf8_codecvt_facet.hpp>
#include <boost/filesystem.hpp>

#include "libbsa.h"

using std::endl;

int main() {
	//Set the locale to get encoding conversions working correctly.
	setlocale(LC_CTYPE, "");
	std::locale global_loc = std::locale();
	std::locale loc(global_loc, new boost::filesystem::detail::utf8_codecvt_facet());
	boost::filesystem::path::imbue(loc);

	uint8_t * path = reinterpret_cast<uint8_t *>("C:\\Users\\Oliver\\Downloads\\Morrowind.bsa");
	uint8_t * asset = reinterpret_cast<uint8_t *>("meshes/clothes/monk/monkhood_f_0.nif");
	bsa_handle bh;
	uint32_t ret;
	size_t numAssets;
	uint8_t * contentPath = reinterpret_cast<uint8_t *>(".+");
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
	else
		out << '\t' << "OpenBSA(...) successful!" << endl;

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

	out << "TESTING CloseBSA(...)" << endl;
	CloseBSA(bh);

	out.close();
	return 0;
}