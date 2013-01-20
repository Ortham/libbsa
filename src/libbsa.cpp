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
#include "helpers.h"
#include "genericbsa.h"
#include "tes3bsa.h"
#include "tes4bsa.h"
#include "error.h"
#include <boost/filesystem/detail/utf8_codecvt_facet.hpp>
#include <boost/filesystem.hpp>
#include <locale>
#include <boost/regex.hpp>
#include <boost/unordered_set.hpp>

using namespace std;
using namespace libbsa;

/*------------------------------
   Global variables
------------------------------*/

const unsigned int LIBBSA_VERSION_MAJOR = 2;
const unsigned int LIBBSA_VERSION_MINOR = 0;
const unsigned int LIBBSA_VERSION_PATCH = 0;

const char * extErrorString = NULL;


/*------------------------------
   Constants
------------------------------*/

// Return codes
const unsigned int LIBBSA_OK                        = 0;
const unsigned int LIBBSA_ERROR_INVALID_ARGS        = 1;
const unsigned int LIBBSA_ERROR_NO_MEM              = 2;
const unsigned int LIBBSA_ERROR_FILESYSTEM_ERROR    = 3;
const unsigned int LIBBSA_ERROR_BAD_STRING          = 4;
const unsigned int LIBBSA_ERROR_ZLIB_ERROR          = 5;
const unsigned int LIBBSA_ERROR_PARSE_FAIL          = 6;
const unsigned int LIBBSA_RETURN_MAX                = LIBBSA_ERROR_PARSE_FAIL;

/* BSA save flags */
/* Use only one version flag. */
const unsigned int LIBBSA_VERSION_TES3              = 0x00000001;
const unsigned int LIBBSA_VERSION_TES4              = 0x00000002;
const unsigned int LIBBSA_VERSION_TES5              = 0x00000004;
/* Use only one compression flag. */
const unsigned int LIBBSA_COMPRESS_LEVEL_0          = 0x00000010;
const unsigned int LIBBSA_COMPRESS_LEVEL_1          = 0x00000020;
const unsigned int LIBBSA_COMPRESS_LEVEL_2          = 0x00000040;
const unsigned int LIBBSA_COMPRESS_LEVEL_3          = 0x00000080;
const unsigned int LIBBSA_COMPRESS_LEVEL_4          = 0x00000100;
const unsigned int LIBBSA_COMPRESS_LEVEL_5          = 0x00000200;
const unsigned int LIBBSA_COMPRESS_LEVEL_6          = 0x00000400;
const unsigned int LIBBSA_COMPRESS_LEVEL_7          = 0x00000800;
const unsigned int LIBBSA_COMPRESS_LEVEL_8          = 0x00001000;
const unsigned int LIBBSA_COMPRESS_LEVEL_9          = 0x00002000;
const unsigned int LIBBSA_COMPRESS_LEVEL_NOCHANGE   = 0x00004000;

unsigned int c_error(unsigned int code, const char * what) {
    extErrorString = what;
    return code;
}


/*------------------------------
   Version Functions
------------------------------*/

/* Returns whether this version of libbsa is compatible with the given
   version of libbsa. */
LIBBSA bool bsa_is_compatible (const unsigned int versionMajor, const unsigned int versionMinor, const unsigned int versionPatch) {
    if (versionMajor == 2 && versionMinor == 0 && versionPatch == 0)
        return true;
    else
        return false;
}

LIBBSA void bsa_get_version (unsigned int * const versionMajor, unsigned int * const versionMinor, unsigned int * const versionPatch) {
    *versionMajor = LIBBSA_VERSION_MAJOR;
    *versionMinor = LIBBSA_VERSION_MINOR;
    *versionPatch = LIBBSA_VERSION_PATCH;
}


/*------------------------------
   Error Handling Functions
------------------------------*/

/* Outputs a string giving the a message containing the details of the
   last error or warning encountered by a function called for the given
   game handle. */
LIBBSA unsigned int bsa_get_error_message (const char ** const details) {
    if (details == NULL)
        return c_error(LIBBSA_ERROR_INVALID_ARGS, "Null pointer passed.");

    *details = extErrorString;

    return LIBBSA_OK;
}

LIBBSA void bsa_cleanup () {
    delete [] extErrorString;
}


/*----------------------------------
   Lifecycle Management Functions
----------------------------------*/

/* Opens a BSA file at path, returning a handle.  */
LIBBSA unsigned int bsa_open (bsa_handle * const bh, const char * const path) {
    if (bh == NULL || path == NULL)  //Check for valid args.
        return c_error(LIBBSA_ERROR_INVALID_ARGS, "Null pointer passed.");

    //Set the locale to get encoding conversions working correctly.
    setlocale(LC_CTYPE, "");
    locale global_loc = locale();
    locale loc(global_loc, new boost::filesystem::detail::utf8_codecvt_facet());
    boost::filesystem::path::imbue(loc);

    //Create handle for the appropriate BSA type.
    try {
        if (tes3::IsBSA(path))
            *bh = new tes3::BSA(path);
        else if (tes4::IsBSA(path))
            *bh = new tes4::BSA(path);
        else
            *bh = new tes4::BSA(path);  //Arbitrary choice of BSA type.
    } catch (error& e) {
        return c_error(e.code(), e.what());
    } catch (ios_base::failure& e) {
        return c_error(LIBBSA_ERROR_FILESYSTEM_ERROR, e.what());
    }

    return LIBBSA_OK;
}

/* Create a BSA at the specified path. The 'flags' argument consists of a set
   of bitwise OR'd constants defining the version of the BSA and the
   compression level used (and whether the compression is forced). */
LIBBSA unsigned int bsa_save (bsa_handle bh, const char * const path, const unsigned int flags) {
    if (bh == NULL || path == NULL)  //Check for valid args.
        return c_error(LIBBSA_ERROR_INVALID_ARGS, "Null pointer passed.");

    //Check that flags are valid.
    unsigned int version = 0, compression = 0;

        //First we need to see what flags are set.
    if (flags & LIBBSA_VERSION_TES3 && !(flags & LIBBSA_COMPRESS_LEVEL_0))
        return c_error(LIBBSA_ERROR_INVALID_ARGS, "Morrowind BSAs cannot be compressed.");


    //Check for version flag duplication.
    if (flags & LIBBSA_VERSION_TES3)
        version = LIBBSA_VERSION_TES3;
    if (flags & LIBBSA_VERSION_TES4) {
        if (version > 0)
            return c_error(LIBBSA_ERROR_INVALID_ARGS, "Cannot specify more than one version.");
        version = LIBBSA_VERSION_TES4;
    }
    if (flags & LIBBSA_VERSION_TES5) {
        if (version > 0)
            return c_error(LIBBSA_ERROR_INVALID_ARGS, "Cannot specify more than one version.");
        version = LIBBSA_VERSION_TES5;
    }

    //Now remove version flag from flags and check for compression flag duplication.
    compression = flags ^ version;
    if(!(compression & (compression-1)))
        return c_error(LIBBSA_ERROR_INVALID_ARGS, "Invalid compression level specified.");

    try {
        bh->Save(path, version, compression);
    } catch (error& e) {
        return c_error(e.code(), e.what());
    } catch (ios_base::failure& e) {
        return c_error(LIBBSA_ERROR_FILESYSTEM_ERROR, e.what());
    }

    return LIBBSA_OK;
}

/* Closes the BSA associated with the given handle, freeing any memory
   allocated during its use. */
LIBBSA void bsa_close (bsa_handle bh) {
    delete bh;
}


/*------------------------------
   Content Reading Functions
------------------------------*/

/* Gets an array of all the assets in the given BSA that match the contentPath
   given. contentPath is a POSIX Extended regular expression that all asset
   paths within the BSA will be compared to. */
LIBBSA unsigned int bsa_get_assets (bsa_handle bh, const char * const contentPath, char *** const assetPaths, size_t * const numAssets) {
    if (bh == NULL || contentPath == NULL || assetPaths == NULL || numAssets == NULL) //Check for valid args.
        return c_error(LIBBSA_ERROR_INVALID_ARGS, "Null pointer passed.");

    //Free memory if in use.
    if (bh->extAssets != NULL) {
        for (size_t i=0; i < bh->extAssetsNum; i++)
            delete [] bh->extAssets[i];
        delete [] bh->extAssets;
        bh->extAssets = NULL;
        bh->extAssetsNum = 0;
    }

    //Init values.
    *assetPaths = NULL;
    *numAssets = 0;

    //Build regex expression. Also check that it is valid.
    boost::regex regex;
    try {
        regex = boost::regex(contentPath, boost::regex::extended|boost::regex::icase);
    } catch (boost::regex_error& e) {
        return c_error(LIBBSA_ERROR_INVALID_ARGS, e.what());
    }

    //We don't know how many matches there will be, so put all matches into a temporary buffer first.
    list<BsaAsset> temp;
    bh->GetMatchingAssets(regex, temp);

    if (temp.empty())
        return LIBBSA_OK;

    //Fill external array.
    try {
        bh->extAssetsNum = temp.size();
        bh->extAssets = new char*[bh->extAssetsNum];
        size_t i = 0;
        for (list<BsaAsset>::iterator it = temp.begin(), endIt = temp.end(); it != endIt; ++it) {
            bh->extAssets[i] = ToNewCString(it->path);
            i++;
        }
    } catch (bad_alloc& e) {
        return c_error(LIBBSA_ERROR_NO_MEM, e.what());
    } catch (error& e) {
        return c_error(e.code(), e.what());
    }

    *assetPaths = bh->extAssets;
    *numAssets = bh->extAssetsNum;

    return LIBBSA_OK;
}

/* Checks if a specific asset, found within the BSA at assetPath, is in the given BSA. */
LIBBSA unsigned int bsa_contains_asset (bsa_handle bh, const char * const assetPath, bool * const result) {
    if (bh == NULL || assetPath == NULL || result == NULL) //Check for valid args.
        return c_error(LIBBSA_ERROR_INVALID_ARGS, "Null pointer passed.");

    string assetStr = FixPath(assetPath);

    *result = bh->HasAsset(assetStr);

    return LIBBSA_OK;
}


/*------------------------------
   Content Writing Functions
------------------------------*/

/* Replaces all the assets in the given BSA with the given assets. */
LIBBSA unsigned int bsa_set_assets (bsa_handle bh, const bsa_asset * const assets, const size_t numAssets) {
    if (bh == NULL || assets == NULL) //Check for valid args.
        return c_error(LIBBSA_ERROR_INVALID_ARGS, "Null pointer passed.");

    return LIBBSA_OK;
}

/* Adds a specific asset to a BSA. */
LIBBSA unsigned int bsa_add_asset (bsa_handle bh, const bsa_asset asset) {
    if (bh == NULL) //Check for valid args.
        return c_error(LIBBSA_ERROR_INVALID_ARGS, "Null pointer passed.");

    return LIBBSA_OK;
}

/* Removes a specific asset, found at assetPath, from a BSA. */
LIBBSA unsigned int bsa_remove_asset (bsa_handle bh, const char * const assetPath) {
    if (bh == NULL || assetPath == NULL) //Check for valid args.
        return c_error(LIBBSA_ERROR_INVALID_ARGS, "Null pointer passed.");

    return LIBBSA_OK;
}


/*--------------------------------
   Content Extraction Functions
--------------------------------*/

/* Extracts all the files and folders that match the contentPath given to the
   given destPath. contentPath is a path ending in a filename given as a POSIX
   Extended regular expression that all asset paths within the BSA will be
   compared to. Directory structure is preserved. */
LIBBSA unsigned int bsa_extract_assets (bsa_handle bh, const char * const contentPath, const char * destPath, char *** const assetPaths, size_t * const numAssets, const bool overwrite) {
    if (bh == NULL || contentPath == NULL || destPath == NULL || assetPaths == NULL) //Check for valid args.
        return c_error(LIBBSA_ERROR_INVALID_ARGS, "Null pointer passed.");

    //Free memory if in use.
    if (bh->extAssets != NULL) {
        for (size_t i=0; i < bh->extAssetsNum; i++)
            delete [] bh->extAssets[i];
        delete [] bh->extAssets;
        bh->extAssets = NULL;
        bh->extAssetsNum = 0;
    }

    //Init values.
    *assetPaths = NULL;
    *numAssets = 0;

    //Build regex expression. Also check that it is valid.
    boost::regex regex;
    try {
        regex = boost::regex(string(reinterpret_cast<const char*>(contentPath)), boost::regex::extended|boost::regex::icase);
    } catch (boost::regex_error& e) {
        return c_error(LIBBSA_ERROR_INVALID_ARGS, e.what());
    }

    //We don't know how many matches there will be, so put all matches into a temporary buffer first.
    list<BsaAsset> temp;
    bh->GetMatchingAssets(regex, temp);

    if (temp.empty())
        return LIBBSA_OK;

    //Extract files.
    try {
        bh->Extract(temp, string(reinterpret_cast<const char*>(destPath)), overwrite);
    } catch (error& e) {
        return c_error(e.code(), e.what());
    }

    //Now iterate through temp hashmap, outputting filenames.
    try {
        bh->extAssetsNum = temp.size();
        bh->extAssets = new char*[bh->extAssetsNum];
        size_t i = 0;
        for (list<BsaAsset>::iterator it = temp.begin(), endIt = temp.end(); it != endIt; ++it) {
            bh->extAssets[i] = ToNewCString(it->path);
            i++;
        }
    } catch (bad_alloc& e) {
        return c_error(LIBBSA_ERROR_NO_MEM, e.what());
    } catch (error& e) {
        return c_error(e.code(), e.what());
    }

    *assetPaths = bh->extAssets;
    *numAssets = bh->extAssetsNum;

    return LIBBSA_OK;
}

/* Extracts a specific asset, found at assetPath, from a given BSA, to destPath. */
LIBBSA unsigned int bsa_extract_asset (bsa_handle bh, const char * const assetPath, const char * const destPath, const bool overwrite) {
    if (bh == NULL || assetPath == NULL || destPath == NULL) //Check for valid args.
        return c_error(LIBBSA_ERROR_INVALID_ARGS, "Null pointer passed.");

    string assetStr = FixPath(assetPath);

    try {
        bh->Extract(assetStr, string(reinterpret_cast<const char*>(destPath)), overwrite);
    } catch (error& e) {
        return c_error(e.code(), e.what());
    }

    return LIBBSA_OK;
}
