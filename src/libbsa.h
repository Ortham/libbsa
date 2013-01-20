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

#ifndef __LIBBSA_H__
#define __LIBBSA_H__

#include <stddef.h>

#if defined(_MSC_VER)
//MSVC doesn't support C99, so do the stdbool.h definitions ourselves.
//START OF stdbool.h DEFINITIONS.
#       ifndef __cplusplus
#               define bool     _Bool
#               define true     1
#               define false   0
#       endif
#       define __bool_true_false_are_defined   1
//END OF stdbool.h DEFINITIONS.
#else
#       include <stdbool.h>
#endif

// set up dll import/export decorators
// when compiling the dll on windows, ensure LIBBSA_EXPORT is defined.  clients
// that use this header do not need to define anything to import the symbols
// properly.
#if defined(_WIN32) || defined(_WIN64)
#       ifdef LIBBSA_STATIC
#               define LIBBSA
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
typedef struct _bsa_handle_int * bsa_handle;

/* Holds the source and destination paths for an asset to be added to a BSA.
   These paths must be valid until the BSA is saved, as they are not actually
   written until then. */
typedef struct {
        char * sourcePath;  //The path of the asset in the external filesystem.
        char * destPath;    //The path of the asset when it is in the BSA.
} bsa_asset;

/* Return codes */
LIBBSA extern const unsigned int LIBBSA_OK;
LIBBSA extern const unsigned int LIBBSA_ERROR_INVALID_ARGS;
LIBBSA extern const unsigned int LIBBSA_ERROR_NO_MEM;
LIBBSA extern const unsigned int LIBBSA_ERROR_FILESYSTEM_ERROR;
LIBBSA extern const unsigned int LIBBSA_ERROR_BAD_STRING;
LIBBSA extern const unsigned int LIBBSA_ERROR_ZLIB_ERROR;
LIBBSA extern const unsigned int LIBBSA_ERROR_PARSE_FAIL;
LIBBSA extern const unsigned int LIBBSA_RETURN_MAX;
/* No doubt there will be more... */

/* BSA save flags */
/* Use only one version flag. */
LIBBSA extern const unsigned int LIBBSA_VERSION_TES3;
LIBBSA extern const unsigned int LIBBSA_VERSION_TES4;
LIBBSA extern const unsigned int LIBBSA_VERSION_TES5;  //Use for Fallout 3 and Fallout: New Vegas too.
/* Use only one compression flag. */
LIBBSA extern const unsigned int LIBBSA_COMPRESS_LEVEL_0;  //No compression.
LIBBSA extern const unsigned int LIBBSA_COMPRESS_LEVEL_1;  //Least compression.
LIBBSA extern const unsigned int LIBBSA_COMPRESS_LEVEL_2;
LIBBSA extern const unsigned int LIBBSA_COMPRESS_LEVEL_3;
LIBBSA extern const unsigned int LIBBSA_COMPRESS_LEVEL_4;
LIBBSA extern const unsigned int LIBBSA_COMPRESS_LEVEL_5;
LIBBSA extern const unsigned int LIBBSA_COMPRESS_LEVEL_6;
LIBBSA extern const unsigned int LIBBSA_COMPRESS_LEVEL_7;
LIBBSA extern const unsigned int LIBBSA_COMPRESS_LEVEL_8;
LIBBSA extern const unsigned int LIBBSA_COMPRESS_LEVEL_9;  //Most compression.
LIBBSA extern const unsigned int LIBBSA_COMPRESS_LEVEL_NOCHANGE;  //Use compression used in the opened BSA.


/*------------------------------
   Version Functions
------------------------------*/

/* Returns whether this version of libbsa is compatible with the given
   version of libbsa. */
LIBBSA bool bsa_is_compatible (const unsigned int versionMajor, const unsigned int versionMinor, const unsigned int versionPatch);

/* Gets the version numbers for the libary. */
LIBBSA void bsa_get_version (unsigned int * const versionMajor, unsigned int * const versionMinor, unsigned int * const versionPatch);


/*------------------------------
   Error Handling Functions
------------------------------*/

/* Gets a string with details about the last error returned. */
LIBBSA unsigned int bsa_get_error_message (const char ** const details);

/* Frees the memory allocated to the last error details string. */
LIBBSA void bsa_cleanup ();


/*----------------------------------
   Lifecycle Management Functions
----------------------------------*/

/* Opens a BSA at path, returning a handle bh. If the BSA doesn't exist
   then the function will create a handle for a new file. */
LIBBSA unsigned int bsa_open (bsa_handle * const bh, const char * const path);

/* Create a BSA at the specified path. The 'flags' argument consists of a set
   of bitwise OR'd constants defining the version of the BSA and the
   compression level used (and whether the compression is forced). */
LIBBSA unsigned int bsa_save (bsa_handle bh, const char * const path, const unsigned int flags);

/* Closes the BSA associated with the given handle, freeing any memory
   allocated during its use. */
LIBBSA void bsa_close (bsa_handle bh);


/*------------------------------
   Content Reading Functions
------------------------------*/

/* Gets an array of all the assets in the given BSA that match the contentPath
   given. contentPath is a POSIX Extended regular expression that all asset
   paths within the BSA will be compared to. */
LIBBSA unsigned int bsa_get_assets (bsa_handle bh, const char * const contentPath, char *** const assetPaths, size_t * const numAssets);

/* Checks if a specific asset, found within the BSA at assetPath, is in the given BSA. */
LIBBSA unsigned int bsa_contains_asset (bsa_handle bh, const char * const assetPath, bool * const result);


/*------------------------------
   Content Writing Functions
------------------------------*/

/* Replaces all the assets in the given BSA with the given assets. */
LIBBSA unsigned int bsa_set_assets (bsa_handle bh, const bsa_asset * const assets, const size_t numAssets);

/* Adds a specific asset to a BSA. */
LIBBSA unsigned int bsa_add_asset (bsa_handle bh, const bsa_asset asset);

/* Removes a specific asset, found at assetPath, from a BSA. */
LIBBSA unsigned int bsa_remove_asset (bsa_handle bh, const char * const assetPath);


/*--------------------------------
   Content Extraction Functions
--------------------------------*/

/* Extracts all the files and folders that match the contentPath given to the
   given destPath. contentPath is a path ending in a filename given as a POSIX
   Extended regular expression that all asset paths within the BSA will be
   compared to. Directory structure is preserved. */
LIBBSA unsigned int bsa_extract_assets (bsa_handle bh, const char * const contentPath, const char * const destPath, char *** const assetPaths, size_t * const numAssets, const bool overwrite);

/* Extracts a specific asset, found at assetPath, from a given BSA, to destPath. */
LIBBSA unsigned int bsa_extract_asset (bsa_handle bh, const char * const assetPath, const char * const destPath, const bool overwrite);

#ifdef __cplusplus
}
#endif


#endif
