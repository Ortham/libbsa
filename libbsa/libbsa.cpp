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
#include "helpers.h"
#include "format.h"
#include "exception.h"
#include <boost/filesystem/detail/utf8_codecvt_facet.hpp>
#include <locale>
#include <boost/regex.hpp>
#include <boost/unordered_set.hpp>

using namespace std;
using namespace libbsa;

/*------------------------------
   Global variables
------------------------------*/

const uint32_t LIBBSA_VERSION_MAJOR = 1;
const uint32_t LIBBSA_VERSION_MINOR = 0;
const uint32_t LIBBSA_VERSION_PATCH = 0;

uint8_t * extErrorString = NULL;


/*------------------------------
   Constants
------------------------------*/

/* Return codes */
LIBBSA const uint32_t LIBBSA_OK							= 0;
LIBBSA const uint32_t LIBBSA_ERROR_INVALID_ARGS			= 1;
LIBBSA const uint32_t LIBBSA_ERROR_NO_MEM				= 2;
LIBBSA const uint32_t LIBBSA_ERROR_FILE_NOT_FOUND		= 3;
LIBBSA const uint32_t LIBBSA_ERROR_FILE_WRITE_FAIL		= 4;
LIBBSA const uint32_t LIBBSA_ERROR_FILE_READ_FAIL		= 5;
LIBBSA const uint32_t LIBBSA_ERROR_BAD_STRING			= 6;
LIBBSA const uint32_t LIBBSA_RETURN_MAX					= LIBBSA_ERROR_BAD_STRING;
/* No doubt there will be more... */

/* BSA save flags */
/* Use only one version flag. */
LIBBSA const uint32_t LIBBSA_VERSION_TES3				= 0x00000001;
LIBBSA const uint32_t LIBBSA_VERSION_TES4				= 0x00000002;
LIBBSA const uint32_t LIBBSA_VERSION_TES5				= 0x00000004;
LIBBSA const uint32_t LIBBSA_VERSION_FO3				= 0x00000008;
LIBBSA const uint32_t LIBBSA_VERSION_FNV				= 0x00000010;
/* Use only one compression flag. */
LIBBSA const uint32_t LIBBSA_COMPRESS_LEVEL_0			= 0x00000020;
LIBBSA const uint32_t LIBBSA_COMPRESS_LEVEL_1			= 0x00000040;
LIBBSA const uint32_t LIBBSA_COMPRESS_LEVEL_2			= 0x00000080;
LIBBSA const uint32_t LIBBSA_COMPRESS_LEVEL_3			= 0x00000100;
LIBBSA const uint32_t LIBBSA_COMPRESS_LEVEL_4			= 0x00000200;
LIBBSA const uint32_t LIBBSA_COMPRESS_LEVEL_5			= 0x00000400;
LIBBSA const uint32_t LIBBSA_COMPRESS_LEVEL_6			= 0x00000800;
LIBBSA const uint32_t LIBBSA_COMPRESS_LEVEL_7			= 0x00001000;
LIBBSA const uint32_t LIBBSA_COMPRESS_LEVEL_8			= 0x00002000;
LIBBSA const uint32_t LIBBSA_COMPRESS_LEVEL_9			= 0x00004000;
LIBBSA const uint32_t LIBBSA_COMPRESS_LEVEL_MAX_QUICK	= 0x00008000;
LIBBSA const uint32_t LIBBSA_COMPRESS_LEVEL_MAX_MAX		= 0x00010000;
/* Use only one other flag. */
LIBBSA const uint32_t LIBBSA_OTHER_FORCE_COMPRESSION	= 0x00020000;


/*------------------------------
   Version Functions
------------------------------*/

/* Returns whether this version of libbsa is compatible with the given
   version of libbsa. */
LIBBSA bool IsCompatibleVersion(const uint32_t versionMajor, const uint32_t versionMinor, const uint32_t versionPatch) {
	if (versionMajor == 1 && versionMinor == 0 && versionPatch == 0)
		return true;
	else
		return false;
}

LIBBSA void GetVersionNums(uint32_t * versionMajor, uint32_t * versionMinor, uint32_t * versionPatch) {
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
LIBBSA uint32_t GetLastErrorDetails(uint8_t ** details) {
	if (details == NULL)
		return error(LIBBSA_ERROR_INVALID_ARGS, "Null pointer passed.").code();

	//Free memory if in use.
	delete [] extErrorString;
	extErrorString = NULL;

	try {
		extErrorString = ToUint8_tString(lastException.what());
	} catch (bad_alloc /*&e*/) {
		return error(LIBBSA_ERROR_NO_MEM).code();
	}

	*details = extErrorString;
	return LIBBSA_OK;
}

LIBBSA void CleanUpErrorDetails() {
	delete [] extErrorString;
	extErrorString = NULL;
}


/*----------------------------------
   Lifecycle Management Functions
----------------------------------*/

/* Opens a STRINGS, ILSTRINGS or DLSTRINGS file at path, returning a handle 
   sh. If the strings file doesn't exist then it will be created. The file 
   extension is used to determine the string data format used. */
LIBBSA uint32_t OpenBSA(bsa_handle * bh, const uint8_t * path) {
	if (bh == NULL || path == NULL) //Check for valid args.
		return error(LIBBSA_ERROR_INVALID_ARGS, "Null pointer passed.").code();

	//Set the locale to get encoding conversions working correctly.
	setlocale(LC_CTYPE, "");
	locale global_loc = locale();
	locale loc(global_loc, new boost::filesystem::detail::utf8_codecvt_facet());
	boost::filesystem::path::imbue(loc);

	//Create handle.
	try {
		*bh = new bsa_handle_int(string(reinterpret_cast<const char *>(path)));
	} catch (error& e) {
		return e.code();
	}

	return LIBBSA_OK;
}

/* Create a BSA at the specified path. The 'flags' argument consists of a set
   of bitwise OR'd constants defining the version of the BSA and the
   compression level used (and whether the compression is forced). */
LIBBSA uint32_t SaveBSA(bsa_handle bh, const uint8_t * path, const uint32_t flags) {
	if (bh == NULL || path == NULL) //Check for valid args.
		return error(LIBBSA_ERROR_INVALID_ARGS, "Null pointer passed.").code();
	else if (flags == 0 /*|| ...*/)
		return error(LIBBSA_ERROR_INVALID_ARGS, "Invalid flags combination passed.").code();

	return LIBBSA_OK;
}

/* Closes the BSA associated with the given handle, freeing any memory 
   allocated during its use. */
LIBBSA void CloseBSA(bsa_handle bh) {
	delete bh;
}


/*------------------------------
   Content Reading Functions
------------------------------*/

/* Gets an array of all the assets in the given BSA that match the contentPath 
   given. contentPath is a POSIX Extended regular expression that all asset 
   paths within the BSA will be compared to. */
LIBBSA uint32_t GetAssets(bsa_handle bh, const uint8_t * contentPath, uint8_t *** assetPaths, size_t * numAssets) {
	if (bh == NULL || contentPath == NULL || assetPaths == NULL || numAssets == NULL) //Check for valid args.
		return error(LIBBSA_ERROR_INVALID_ARGS, "Null pointer passed.").code();

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

	if (bh->paths.empty())
		return LIBBSA_OK;

	//Build regex expression. Also check that it is valid.
	boost::regex regex;
	try {
		regex = boost::regex(string(reinterpret_cast<const char*>(contentPath)), boost::regex::extended|boost::regex::icase);
	} catch (boost::regex_error e) {
		return error(LIBBSA_ERROR_INVALID_ARGS, "Invalid regular expression passed.").code();
	}

	//We don't know how many matches there will be, so put all matches into a temporary buffer first.
	boost::unordered_set<string> temp;
	for (boost::unordered_map<string, FileRecordData>::iterator it = bh->paths.begin(), endIt = bh->paths.end(); it != endIt; ++it) {
		if (boost::regex_match(it->first, regex))
			temp.insert(it->first);
	}

	if (temp.empty())
		return LIBBSA_OK;
	
	//Fill external array.
	try {
		bh->extAssetsNum = temp.size();
		bh->extAssets = new uint8_t*[bh->extAssetsNum];
		size_t i = 0;
		for (boost::unordered_set<string>::iterator it = temp.begin(), endIt = temp.end(); it != endIt; ++it) {
			bh->extAssets[i] = bh->GetString(*it);
			i++;
		}
	} catch (bad_alloc& /*e*/) {
		return error(LIBBSA_ERROR_NO_MEM).code();
	} catch (error& e) {
		return e.code();
	}

	*assetPaths = bh->extAssets;
	*numAssets = bh->extAssetsNum;

	return LIBBSA_OK;
}

/* Checks if a specific asset, found within the BSA at assetPath, is in the given BSA. */
LIBBSA uint32_t IsAssetInBSA(bsa_handle bh, const uint8_t * assetPath, bool * result) {
	if (bh == NULL || assetPath == NULL || result == NULL) //Check for valid args.
		return error(LIBBSA_ERROR_INVALID_ARGS, "Null pointer passed.").code();

	string assetStr = FixPath(assetPath);

	if (bh->paths.find(assetStr) != bh->paths.end())
		*result = true;
	else
		*result = false;

	return LIBBSA_OK;
}


/*------------------------------
   Content Writing Functions
------------------------------*/

/* Replaces all the assets in the given BSA with the given assets. */
LIBBSA uint32_t SetAssets(bsa_handle bh, const bsa_asset * assets) {
	if (bh == NULL || assets == NULL) //Check for valid args.
		return error(LIBBSA_ERROR_INVALID_ARGS, "Null pointer passed.").code();

	return LIBBSA_OK;
}

/* Adds a specific asset to a BSA. */
LIBBSA uint32_t AddAsset(bsa_handle bh, const bsa_asset asset) {
	if (bh == NULL) //Check for valid args.
		return error(LIBBSA_ERROR_INVALID_ARGS, "Null pointer passed.").code();

	return LIBBSA_OK;
}

/* Removes a specific asset, found at assetPath, from a BSA. */
LIBBSA uint32_t RemoveAsset(bsa_handle bh, const uint8_t * assetPath) {
	if (bh == NULL || assetPath == NULL) //Check for valid args.
		return error(LIBBSA_ERROR_INVALID_ARGS, "Null pointer passed.").code();

	return LIBBSA_OK;
}


/*--------------------------------
   Content Extraction Functions
--------------------------------*/

/* Extracts all the files and folders that match the contentPath given to the 
   given destPath. contentPath is a path ending in a filename given as a POSIX
   Extended regular expression that all asset paths within the BSA will be
   compared to. Directory structure is preserved. */
LIBBSA uint32_t ExtractAssets(bsa_handle bh, const uint8_t * contentPath, const uint8_t * destPath, uint8_t *** content) {
	if (bh == NULL || contentPath == NULL || destPath == NULL || content == NULL) //Check for valid args.
		return error(LIBBSA_ERROR_INVALID_ARGS, "Null pointer passed.").code();

	return LIBBSA_OK;
}

/* Extracts a specific asset, found at assetPath, from a given BSA, to destPath. */
LIBBSA uint32_t ExtractAsset(bsa_handle bh, const uint8_t * assetPath, const uint8_t * destPath) {
	if (bh == NULL || assetPath == NULL || destPath == NULL) //Check for valid args.
		return error(LIBBSA_ERROR_INVALID_ARGS, "Null pointer passed.").code();

	return LIBBSA_OK;
}