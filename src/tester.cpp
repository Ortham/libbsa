/*  libbsa

    A library for reading and writing BSA files.

    Copyright (C) 2012-2013    WrinklyNinja

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
#include "streams.h"

#include <stdint.h>

#include <boost/filesystem.hpp>

using std::endl;

int main() {
    /* List of official BSAs for testing.
    R = Reads OK, E = Extracts OK, W = Writes OK.
    !R = Doesn't read OK, !E and !W similar.

    Morrowind:
        Morrowind.bsa                           R   E   !W
        Bloodmoon.bsa                           R   E   !W
        Tribunal.bsa                            R   E   W

    Oblivion:
        Oblivion - Meshes.bsa                   R   E   !W
        Oblivion - Misc.bsa                     R   E   W
        Oblivion - Sounds.bsa                   R   E   W
        Oblivion - Textures - Compressed.bsa    R   E   W
        Oblivion - Voices1.bsa                  R   E   W
        Oblivion - Voices2.bsa                  R   E   W
        Knights.bsa                             R   E   !W
        DLCShiveringIsles - Meshes.bsa          R   E   !W
        DLCShiveringIsles - Sounds.bsa          R   E   W
        DLCShiveringIsles - Textures.bsa        R   E   !W
        DLCShiveringIsles - Voices.bsa          R   E   W

    Skyrim:
        Skyrim - Animations.bsa                 R   E   !W
        Skyrim - Interface.bsa                  R   E   W
        Skyrim - Meshes.bsa                     R   E   !W
        Skyrim - Misc.bsa                       R   E   !W
        Skyrim - Shaders.bsa                    R   E   !W
        Skyrim - Sounds.bsa                     R   E   W
        Skyrim - Textures.bsa                   R   E   !W
        Skyrim - Voices.bsa                     R   E   W
        Skyrim - VoicesExtra.bsa                R   E   W
        Update.bsa                              R   E   W
    */

    const char * path = "/media/oliver/6CF05918F058EA3A/Program Files (x86)/Steam/steamapps/common/skyrim/Data/Skyrim - Misc.bsa";
    const char * outPath = "/media/oliver/6CF05918F058EA3A/Program Files (x86)/Steam/steamapps/common/skyrim/Data/Skyrim - Misc.bsa.new";
//  const char * asset = "meshes/m/probe_journeyman_01.nif";
//  const char * extPath = "C:\\Users\\Oliver\\Downloads\\probe_journeyman_01.nif.extract";
    const char * destPath = "/home/oliver/Testing/libbsa/Skyrim - Misc/";
    bsa_handle bh;
    uint32_t ret;
    size_t numAssets;
    const char * contentPath = ".+";
    char * err;
    char ** assetPaths;
    bool result;

    libbsa::ofstream out(boost::filesystem::path("libbsa-tester.txt"));
    if (!out.good()){
        std::cout << "File could not be opened for reading.";
        return 1;
    }

    out << "Using path: " << path << endl;

    out << "TESTING bsa_open(...)" << endl;
    ret = bsa_open(&bh, path);
    if (ret != LIBBSA_OK)
        out << '\t' << "bsa_open(...) failed! Return code: " << ret << endl;
    else {
        out << '\t' << "bsa_open(...) successful!" << endl;

        out << "TESTING bsa_get_assets(...)" << endl;
        ret = bsa_get_assets(bh, contentPath, &assetPaths, &numAssets);
        if (ret != LIBBSA_OK)
            out << '\t' << "bsa_get_assets(...) failed! Return code: " << ret << endl;
        else {
            out << '\t' << "bsa_get_assets(...) successful! Number of paths: " << numAssets << endl;
            for (size_t i=0; i < numAssets; i++) {
                out << '\t' << assetPaths[i] << endl;
            }
        }
/*
        out << "TESTING bsa_contains_asset(...)" << endl;
        ret = bsa_contains_asset(bh, asset, &result);
        if (ret != LIBBSA_OK)
            out << '\t' << "bsa_contains_asset(...) failed! Return code: " << ret << endl;
        else
            out << '\t' << "bsa_contains_asset(...) successful! Is \"" << asset << "\" in BSA: " << result << endl;

        out << "TESTING bsa_extract_asset(...)" << endl;
        ret = bsa_extract_asset(bh, asset, extPath, true);
        if (ret != LIBBSA_OK)
            out << '\t' << "bsa_extract_asset(...) failed! Return code: " << ret << endl;
        else
            out << '\t' << "bsa_extract_asset(...) successful!" << endl;
*/
        out << "TESTING bsa_extract_assets(...)" << endl;
        ret = bsa_extract_assets(bh, contentPath, destPath, &assetPaths, &numAssets, true);
        if (ret != LIBBSA_OK)
            out << '\t' << "bsa_extract_assets(...) failed! Return code: " << ret << endl;
        else {
            out << '\t' << "bsa_extract_assets(...) successful! Number of paths: " << numAssets << endl;
            for (size_t i=0; i < numAssets; i++) {
                out << '\t' << assetPaths[i] << endl;
            }
        }
/*
        out << "TESTING bsa_save(...)" << endl;
        ret = bsa_save(bh, outPath, LIBBSA_VERSION_TES4 | LIBBSA_COMPRESS_LEVEL_NOCHANGE);
        if (ret != LIBBSA_OK)
            out << '\t' << "bsa_save(...) failed! Return code: " << ret << endl;
        else
            out << '\t' << "bsa_save(...) successful!" << endl;
*/
        out << "TESTING bsa_close(...)" << endl;
        bsa_close(bh);
    }

    out.close();
    return 0;
}
