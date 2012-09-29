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

#ifndef LIBBSA_H
#define LIBBSA_H

#include <stdint.h>
#include <stddef.h>

#if defined(_MSC_VER)
//MSVC doesn't support C99, so do the stdbool.h definitions ourselves.
//START OF stdbool.h DEFINITIONS. 
#	ifndef __cplusplus
#		define bool	_Bool
#		define true	1
#		define false   0
#	endif
#	define __bool_true_false_are_defined   1
//END OF stdbool.h DEFINITIONS.
#else
#	include <stdbool.h>
#endif

// set up dll import/export decorators
// when compiling the dll on windows, ensure LIBBSA_EXPORT is defined.  clients
// that use this header do not need to define anything to import the symbols
// properly.
#if defined(_WIN32) || defined(_WIN64)
#	ifdef LIBBSA_STATIC
#		define LIBBSA
#   elif defined LIBBSA_EXPORT
#       define LIBBSA __declspec(dllexport)
#   else
#       define LIBBSA __declspec(dllimport)
#   endif
#else
#   define LIBBSA
#endif

#ifdef __cplusplus
extern "C"
{
#endif


/*------------------------------
   Types
------------------------------*/

/* Abstraction of BSA info structure while providing type safety. */
typedef struct bsa_handle_int * bsa_handle;

/* Holds the source and destination paths for an asset to be added to a BSA. 
   These paths must be valid until the BSA is saved, as they are not actually
   written until then. */
typedef struct {
	uint8_t * sourcePath;  //The path of the asset in the external filesystem.
	uint8_t * destPath;    //The path of the asset when it is in the BSA.
} bsa_asset;

/* Return codes */
LIBBSA extern const uint32_t LIBBSA_OK;
LIBBSA extern const uint32_t LIBBSA_ERROR_INVALID_ARGS;
LIBBSA extern const uint32_t LIBBSA_ERROR_NO_MEM;
LIBBSA extern const uint32_t LIBBSA_ERROR_FILE_NOT_FOUND;
LIBBSA extern const uint32_t LIBBSA_ERROR_FILE_WRITE_FAIL;
LIBBSA extern const uint32_t LIBBSA_ERROR_FILE_READ_FAIL;
LIBBSA extern const uint32_t LIBBSA_ERROR_BAD_STRING;
LIBBSA extern const uint32_t LIBBSA_RETURN_MAX;
/* No doubt there will be more... */

/* BSA save flags */
/* Use only one version flag. */
LIBBSA extern const uint32_t LIBBSA_VERSION_TES3;
LIBBSA extern const uint32_t LIBBSA_VERSION_TES4;
LIBBSA extern const uint32_t LIBBSA_VERSION_TES5;
LIBBSA extern const uint32_t LIBBSA_VERSION_FO3;
LIBBSA extern const uint32_t LIBBSA_VERSION_FNV;
/* Use only one compression flag. */
LIBBSA extern const uint32_t LIBBSA_COMPRESS_LEVEL_0;
LIBBSA extern const uint32_t LIBBSA_COMPRESS_LEVEL_1;
LIBBSA extern const uint32_t LIBBSA_COMPRESS_LEVEL_2;
LIBBSA extern const uint32_t LIBBSA_COMPRESS_LEVEL_3;
LIBBSA extern const uint32_t LIBBSA_COMPRESS_LEVEL_4;
LIBBSA extern const uint32_t LIBBSA_COMPRESS_LEVEL_5;
LIBBSA extern const uint32_t LIBBSA_COMPRESS_LEVEL_6;
LIBBSA extern const uint32_t LIBBSA_COMPRESS_LEVEL_7;
LIBBSA extern const uint32_t LIBBSA_COMPRESS_LEVEL_8;
LIBBSA extern const uint32_t LIBBSA_COMPRESS_LEVEL_9;
LIBBSA extern const uint32_t LIBBSA_COMPRESS_LEVEL_MAX_QUICK;
LIBBSA extern const uint32_t LIBBSA_COMPRESS_LEVEL_MAX_MAX;
/* Use only one other flag. */
LIBBSA extern const uint32_t LIBBSA_OTHER_FORCE_COMPRESSION;


/*------------------------------
   Version Functions
------------------------------*/

/* Returns whether this version of libbsa is compatible with the given
   version of libbsa. */
LIBBSA bool IsCompatibleVersion (const uint32_t versionMajor, const uint32_t versionMinor, const uint32_t versionPatch);

/* Gets the version numbers for the libary. */
LIBBSA void GetVersionNums(uint32_t * versionMajor, uint32_t * versionMinor, uint32_t * versionPatch);


/*------------------------------
   Error Handling Functions
------------------------------*/

/* Gets a string with details about the last error returned. */
LIBBSA uint32_t GetLastErrorDetails(uint8_t ** details);

/* Frees the memory allocated to the last error details string. */
LIBBSA void CleanUpErrorDetails();


/*----------------------------------
   Lifecycle Management Functions
----------------------------------*/

/* Opens a BSA at path, returning a handle bh. If the BSA doesn't exist 
   then the function will create a handle for a new file. */
LIBBSA uint32_t OpenBSA(bsa_handle * bh, const uint8_t * path);

/* Create a BSA at the specified path. The 'flags' argument consists of a set
   of bitwise OR'd constants defining the version of the BSA and the
   compression level used (and whether the compression is forced). */
LIBBSA uint32_t SaveBSA(bsa_handle bh, const uint8_t * path, const uint32_t flags);

/* Closes the BSA associated with the given handle, freeing any memory 
   allocated during its use. */
LIBBSA void CloseBSA(bsa_handle bh);


/*------------------------------
   Content Reading Functions
------------------------------*/

/* Gets an array of all the assets in the given BSA that match the contentPath 
   given. contentPath is a POSIX Extended regular expression that all asset 
   paths within the BSA will be compared to. */
LIBBSA uint32_t GetAssets(bsa_handle bh, const uint8_t * contentPath, uint8_t *** assetPaths, size_t * numAssets);

/* Checks if a specific asset, found within the BSA at assetPath, is in the given BSA. */
LIBBSA uint32_t IsAssetInBSA(bsa_handle bh, const uint8_t * assetPath, bool * result);


/*------------------------------
   Content Writing Functions
------------------------------*/

/* Replaces all the assets in the given BSA with the given assets. */
LIBBSA uint32_t SetAssets(bsa_handle bh, const bsa_asset * assets, const size_t numAssets);

/* Adds a specific asset to a BSA. */
LIBBSA uint32_t AddAsset(bsa_handle bh, const bsa_asset asset);

/* Removes a specific asset, found at assetPath, from a BSA. */
LIBBSA uint32_t RemoveAsset(bsa_handle bh, const uint8_t * assetPath);


/*--------------------------------
   Content Extraction Functions
--------------------------------*/

/* Extracts all the files and folders that match the contentPath given to the 
   given destPath. contentPath is a path ending in a filename given as a POSIX
   Extended regular expression that all asset paths within the BSA will be
   compared to. Directory structure is preserved. */
LIBBSA uint32_t ExtractAssets(bsa_handle bh, const uint8_t * contentPath, const uint8_t * destPath, uint8_t *** assetPaths, size_t * numAssets);

/* Extracts a specific asset, found at assetPath, from a given BSA, to destPath. */
LIBBSA uint32_t ExtractAsset(bsa_handle bh, const uint8_t * assetPath, const uint8_t * destPath);

#ifdef __cplusplus
}
#endif


#endif