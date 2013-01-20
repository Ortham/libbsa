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

#include "tes3bsa.h"
#include "error.h"
#include "libbsa.h"
#include <fstream>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

using namespace std;

namespace libbsa { namespace tes3 {

    BSA::BSA(const std::string& path)
        : _bsa_handle_int(path),
        hashOffset(0) {

        //Set transcoding up.
        trans.SetEncoding(1252);

        //Check if file exists.
        if (fs::exists(path)) {

            ifstream in(path.c_str(), ios::binary);
            in.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

            Header header;
            in.seekg(0, ios_base::beg);
            in.read((char*)&header, sizeof(Header));

            /* We want:
            - file names
            - file sizes
            - raw data offsets
            - file hashes

            Load the FileRecordData (size,offset), filename offsets, filename records and hashes into memory, then work on them there.
            */
            FileRecord * fileRecords;
            uint32_t * filenameOffsets;
            uint8_t * filenameRecords;
            uint64_t * hashRecords;
            uint32_t filenameRecordsSize = header.hashOffset - sizeof(FileRecord) * header.fileCount - sizeof(uint32_t) * header.fileCount;
            try {
                fileRecords = new FileRecord[header.fileCount];
                in.read((char*)fileRecords, sizeof(FileRecord) * header.fileCount);

                filenameOffsets = new uint32_t[header.fileCount];
                in.read((char*)filenameOffsets, sizeof(uint32_t) * header.fileCount);

                filenameRecords = new uint8_t[filenameRecordsSize];
                in.read((char*)filenameRecords, sizeof(uint8_t) * filenameRecordsSize);

                hashRecords = new uint64_t[header.fileCount];
                in.read((char*)hashRecords, sizeof(uint64_t) * header.fileCount);
            } catch (bad_alloc& e) {
                throw error(LIBBSA_ERROR_NO_MEM, e.what());
            }

            in.close(); //No longer need the file open.

            //All three arrays have the same ordering, so we just need to loop through one and look at the corresponding position in the other.
            uint32_t startOfData = sizeof(Header) + header.hashOffset + header.fileCount * sizeof(uint64_t);
            for (uint32_t i=0; i < header.fileCount; i++) {
                BsaAsset fileData;
                fileData.size = fileRecords[i].size;
                fileData.offset = startOfData + fileRecords[i].offset;  //Internally, offsets are adjusted so that they're from file beginning.
                fileData.hash = hashRecords[i];

                //Now we need to build the file path. First: file name.
                //Find position of null pointer.
                char * nptr = strchr((char*)(filenameRecords + filenameOffsets[i]), '\0');
                if (nptr == NULL)
                    throw error(LIBBSA_ERROR_PARSE_FAIL, "Structure of \"" + path + "\" is invalid.");

                fileData.path = trans.EncToUtf8(string((char*)(filenameRecords + filenameOffsets[i]), nptr - (char*)(filenameRecords + filenameOffsets[i])));

                //Finally, add fileData to list.
                assets.push_back(fileData);
            }

            hashOffset = header.hashOffset;

            delete [] fileRecords;
            delete [] filenameOffsets;
            delete [] filenameRecords;
            delete [] hashRecords;
        }
    }

    void BSA::Save(std::string path, const uint32_t version, const uint32_t compression) {
        //Version and compression have been validated.

        if (path == filePath)
            path += ".new";  //Avoid read/write collisions.

        ifstream in(filePath.c_str(), ios::binary);
        in.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

        ofstream out(path.c_str(), ios::binary | ios::trunc);
        out.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

        //Build file header.
        Header header;
        header.version = tes3::BSA_VERSION_TES3;
        header.fileCount = assets.size();
        //Can't set hashOffset until the size of the names array is known.

        //Allocate memory for info blocks.
        tes3::FileRecord * fileRecords;
        uint32_t * filenameOffsets;
        uint64_t * hashes;
        //Can't allocate memory for filenames, because we don't know how long they are to be. Use a string buffer instead.
        string filenameRecords;
        try {
            fileRecords = new tes3::FileRecord[header.fileCount];
            filenameOffsets = new uint32_t[header.fileCount];
            hashes = new uint64_t[header.fileCount];
        } catch (bad_alloc& e) {
            throw error(LIBBSA_ERROR_NO_MEM, e.what());
        }

        //Need to update the file data offsets before populating the records. This requires the list to be sorted by path.
        //We still want to keep the old offsets for writing the raw file data though.
        assets.sort(path_comp);
        uint32_t fileDataOffset = 0;
        vector<uint32_t> oldOffsets;
ofstream debug("debug.txt");
        for (list<BsaAsset>::iterator it = assets.begin(), endIt = assets.end(); it != endIt; ++it) {



uint32_t offset = it->offset - (hashOffset + sizeof(Header) + header.fileCount * sizeof(uint64_t));
if (offset != fileDataOffset)
    debug << it->path << '\t' << offset << '\t' << fileDataOffset << '\t' << int(offset - fileDataOffset) << endl;


            oldOffsets.push_back(it->offset);
            it->offset = fileDataOffset;  //This results in the wrong offsets for some files - see README for details.
//it->offset -= hashOffset + sizeof(Header) + header.fileCount * sizeof(uint64_t);
            fileDataOffset += it->size;
        }
debug.close();

        //file data, names and hashes are all done in hash order, so sort list by hash.
        assets.sort(hash_comp);
        uint32_t filenameOffset = 0;
        uint32_t i = 0;
        for (list<BsaAsset>::const_iterator it = assets.begin(), endIt = assets.end(); it != endIt; ++it) {
            //Set size and offset.
            fileRecords[i].size = it->size;
            fileRecords[i].offset = it->offset;

            //Set filename offset, and store filename.
            filenameOffsets[i] = filenameOffset;

            //Transcode.
            string filename = trans.Utf8ToEnc(it->path) + '\0';
            filenameRecords += filename;
            filenameOffset += filename.length();

            hashes[i] = it->hash;
            i++;
        }

        //We can now calculate the header's hashOffset to complete it.
        header.hashOffset = (sizeof(FileRecord) + sizeof(uint32_t)) * header.fileCount + filenameRecords.length();

        //Only thing left to do is to write out the completed BSA sections, then
        //read the file data from the input and write it to the output.

        //Write out completed BSA sections.
        out.write((char*)&header, sizeof(Header));   //ok
        out.write((char*)fileRecords, sizeof(FileRecord) * header.fileCount);  //some offsets are wrong
        out.write((char*)filenameOffsets, sizeof(uint32_t) * header.fileCount);  //ok
        out.write(filenameRecords.data(), filenameRecords.length());  //OK.
        out.write((char*)hashes, sizeof(uint64_t) * header.fileCount);       //OK

        delete [] fileRecords;
        delete [] filenameOffsets;
        delete [] hashes;

        //Now write out raw file data in alphabetical filename order.
        assets.sort(path_comp);
        i = 0;
        for (list<BsaAsset>::const_iterator it = assets.begin(), endIt = assets.end(); it != endIt; ++it) {
            //it->second.offset is the offset for the data in the new file. We don't need it though, because we're doing writes in sequence.
            //We want the offset for the data in the old file.
            //This doesn't yet support assets that have been added to the BSA.

            //Allocate memory for this file's data, read it in, write it out, then free memory.
            uint8_t * fileData;
            try {
                fileData = new uint8_t[it->size];  //Doesn't matter where we get size from.
            } catch (bad_alloc& e) {
                throw error(LIBBSA_ERROR_NO_MEM, e.what());
            }

            //Read data in.
            in.seekg(oldOffsets[i], ios_base::beg);
            in.read((char*)fileData, it->size);

            //Write data out.
            out.write((char*)fileData, it->size);

            //Free memory.
            delete [] fileData;

            i++;
        }

        //Update member vars.
        filePath = path;
        hashOffset = header.hashOffset;

        in.close();
        out.close();

        //Now rename the output file.
    /*  if (fs::path(path).extension().string() == ".new") {
            try {
                fs::rename(path, fs::path(path).stem());
            } catch (fs::filesystem_error& e) {
                throw error(LIBBSA_ERROR_FILESYSTEM_ERROR, e.what());
            }
        }*/
    }

    void BSA::ExtractFromStream(std::ifstream& in, const libbsa::BsaAsset& data, const std::string& outPath, const bool overwrite) {
        //Create parent directories.
        fs::create_directories(fs::path(outPath).parent_path());  //This creates any directories in the path that don't already exist.

        if (!overwrite && fs::exists(outPath))
            throw error(LIBBSA_ERROR_FILESYSTEM_ERROR, "The file \"" + outPath + "\" already exists.");

        //Just need to use size and offset to write to binary file stream.
        uint8_t * buffer;

        try {
            buffer = new uint8_t[data.size];
        } catch (bad_alloc& e) {
            throw error(LIBBSA_ERROR_NO_MEM, e.what());
        }

        in.seekg(data.offset, ios_base::beg);
        in.read((char*)buffer, data.size);

        try {
            ofstream out(outPath.c_str(), ios::binary | ios::trunc);
            out.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

            out.write((char*)buffer, data.size);

            out.close();
        } catch (ios_base::failure& e) {
            throw error(LIBBSA_ERROR_FILESYSTEM_ERROR, e.what());
        }

        delete [] buffer;
    }

    uint64_t BSA::CalcHash(const std::string& path) {
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

    bool hash_comp(const BsaAsset& first, const BsaAsset& second) {
        //Data losses are intentional.
        uint32_t f1 = first.hash;
        uint32_t f2 = first.hash >> 32;
        uint32_t s1 = second.hash;
        uint32_t s2 = second.hash >> 32;

        if (f1 < s1)
            return true;
        else if (f1 > s1)
            return false;
        else if (f2 < s2)
            return true;
        else if (f2 > s2)
            return false;

        return first.path < second.path;
    }

    bool path_comp(const BsaAsset& first, const BsaAsset& second) {
        return first.path < second.path;
    }

    //Check if a given file is a Tes3-type BSA.
    bool IsBSA(const std::string& path) {
        //Check if file exists.
        if (!fs::exists(path))
            return false;
        else {
            uint32_t magic;
            ifstream in(path.c_str(), ios::binary);
            in.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);  //Causes ifstream::failure to be thrown if problem is encountered.

            in.read((char*)&magic, sizeof(uint32_t));
            in.close();

            return magic == BSA_VERSION_TES3;  //Magic is actually tes3 bsa version.
        }
    }

}}
