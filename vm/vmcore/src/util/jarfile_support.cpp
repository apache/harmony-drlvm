/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
/**
 * @author Pavel Pervov
 * @version $Revision: 1.1.2.2.4.4 $
 */
#include "jarfile_support.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef PLATFORM_POSIX
#include <unistd.h>
#endif

#include "zlib.h"

using namespace std;

void JarEntry::ConstructFixed( const unsigned char* stream )
{
    m_version = stream[4] + ((unsigned short)stream[5]<<8);
    m_flags = stream[8] + ((unsigned short)stream[9]<<8);
    m_method = stream[10] + ((unsigned short)stream[11]<<8);
    m_sizeCompressed = stream[20] + ((unsigned int)stream[21]<<8) +
        ((unsigned int)stream[22]<<16) + ((unsigned int)stream[23]<<24);
    m_sizeUncompressed = stream[24] + ((unsigned int)stream[25]<<8) +
        ((unsigned int)stream[26]<<16) + ((unsigned int)stream[27]<<24);
    m_nameLength = stream[28] + ((unsigned short)stream[29]<<8);
    m_extraLength = stream[30] + ((unsigned short)stream[31]<<8);
    m_relOffset = stream[42] + ((unsigned int)stream[43]<<8) +
        ((unsigned int)stream[44]<<16) + ((unsigned int)stream[45]<<24);
}


std::vector<JarFile*> JarFile::m_jars = std::vector<JarFile*>();


bool JarEntry::GetContent(unsigned char* content) const
{
    // length of content should be enough for storing m_size_uncompressed bytes
    JarFile* jf = JarFile::GetJar(m_jarFileIdx);
    int inFile = jf->jfh;
    // gregory - It is necessary to lock per jar file because the file
    // handle is global to all threads, and file operations like seek,
    // read, etc may be confused when many threads operate on the same
    // jar file
    LMAutoUnlock lock(&jf->lock);

    if( lseek( inFile, m_contentOffset, SEEK_SET ) == -1 ) return false;

    switch( m_method )
    {
    case JAR_FILE_STORED:
        return ( read( inFile, content, m_sizeCompressed ) == m_sizeCompressed );
    case JAR_FILE_SHRUNK:
        printf( "Found SHRUNK content. No support as of yet.\n" );
        return false;
    case JAR_FILE_REDUCED1:
        printf( "Found REDUCED1 content. No support as of yet.\n" );
        return false;
    case JAR_FILE_REDUCED2:
        printf( "Found REDUCED2 content. No support as of yet.\n" );
        return false;
    case JAR_FILE_REDUCED3:
        printf( "Found REDUCED3 content. No support as of yet.\n" );
        return false;
    case JAR_FILE_REDUCED4:
        printf( "Found REDUCED4 content. No support as of yet.\n" );
        return false;
    case JAR_FILE_IMPLODED:
        printf( "Found IMPLODED content. No support as of yet.\n" );
        return false;
    case JAR_FILE_RESERVED_TCA:
        printf( "Found RESERVED_TCA content. No support as of yet.\n" );
        return false;
    case JAR_FILE_DEFLATED:
#ifdef _IPF
        printf( "Found DEFLATED content. No support as of yet.\n" );
        return false;
#else // _IPF
        {
            unsigned char* data = (unsigned char*)STD_MALLOC(m_sizeCompressed + 1);
            // FIXME: check that memory was allocated
            if( read( inFile, data, m_sizeCompressed ) < m_sizeCompressed ) {
                STD_FREE(data);
                return false;
            }

            z_stream inf;
            memset( &inf, 0, sizeof(z_stream) );
            int infRes;
            inf.next_out = content;
            inf.avail_out = m_sizeUncompressed;
            // Using -MAX_WBITS (actually any integer less than zero)
            // disables zlib to expect specific header
            infRes = inflateInit2( &inf, -MAX_WBITS );
            if( infRes != Z_OK ) {
                STD_FREE(data);
                return false;
            }

            inf.next_in = data;
            inf.avail_in = m_sizeCompressed;
            //inf.data_type = (m_fileEntry.m_internalAttrs&JAR_FILE_TEXT)?Z_ASCII:Z_BINARY;
            inf.data_type = Z_BINARY;
            infRes = inflate( &inf, Z_FINISH );
            STD_FREE(data);
            if( infRes != Z_STREAM_END ) {
                break;
            }
            infRes = inflateEnd( &inf );
        }
#endif //_IPF
        break;
    case JAR_FILE_ENCHANCED_DEFLATE:
        printf( "Found ENCHANCED DEFLATE content. No support as of yet.\n" );
        return false;
    case JAR_FILE_PKWARED:
        printf( "Found PKWARED content. No support as of yet.\n" );
        return false;
    default:
        // all other methods are not supported for now
        return false;
    }

    return true;
}

#ifndef PLATFORM_POSIX
#pragma warning( disable: 4786 ) // identifier was truncated to 255 characters in the browser information
#endif

bool JarFile::Parse( const char* fileName )
{
    int flags = O_RDONLY;
#ifndef PLATFORM_POSIX
    flags |= O_BINARY;
#endif
    int fp = open( fileName, flags, 0 );
    jfh = fp;
    if( fp == -1 ) return false;

    m_name = fileName;

    struct stat fs;
    if(stat(fileName, &fs) == -1) return false;
    unsigned long fsize = fs.st_size;

    m_jars.push_back(this);

    int cd_size = fsize < 67000 ? fsize : 67000;
    lseek(fp, -cd_size, SEEK_END);
    unsigned char *buf = (unsigned char *)STD_ALLOCA(cd_size);
    long off = read(fp, buf, cd_size) - 22; // 22 - EOD size
    long offsetCD; // start of the Central Dir
    int number; // number of entries

    JarEntry je;
    je.m_jarFileIdx = (int)(m_jars.size() - 1);

    while (1){
        if (je.IsPK(buf + off)){
            if (je.IsEODMagic(buf + off + 2)){
                offsetCD = buf[off + 16] + ((unsigned int)buf[off + 17]<<8) +
                    ((unsigned int)buf[off + 18]<<16) + ((unsigned int)buf[off + 19]<<24);
                number = buf[off + 10] + ((unsigned int)buf[off + 11]<<8);
                break;
            }
        }
        if (--off < 0)
            return false;
    }

    m_manifest = new Manifest();
    if(!m_manifest) return false;

    if(m_ownCache) {
        void* jec_mem = pool.alloc(sizeof(JarEntryCache));
        m_entries = new (jec_mem) JarEntryCache();
    }

    lseek(fp, offsetCD, SEEK_SET);

    buf = (unsigned char *)STD_MALLOC(fsize - offsetCD);
    fsize = read(fp, buf, fsize - offsetCD);

    off = 0;
    for (int i = 0; i < number; i++){
        if (!je.IsPK(buf + off) || !je.IsDirEntryMagic( buf + off + 2))
            return false;

        je.ConstructFixed(buf + off);
        je.m_fileName = (char *)pool.alloc(je.m_nameLength + 1);
        strncpy(je.m_fileName, (const char *)buf + off + JAR_DIRECTORYENTRY_LEN, je.m_nameLength );
        je.m_fileName[je.m_nameLength] = '\0';
        je.m_contentOffset = je.m_relOffset + JarEntry::sizeFixed + je.m_nameLength + je.m_extraLength;
        if(!strcmp(je.m_fileName, "META-INF/MANIFEST.MF")) {
            // parse manifest
            if(!m_manifest->Parse(&je)) return false;
            if(!(*m_manifest)) {
                delete m_manifest;
                return false;
            }
        } else {
            // Insertion in common cache preserves the order of inserted
            // elements with the same hash value, so lookup will return
            // the first occurrence. Although more memory is spent here,
            // we do not need to lookup on each entry.
            m_entries->Insert(je);
        }
        off += JAR_DIRECTORYENTRY_LEN + je.m_nameLength + je.m_extraLength;
    }
    STD_FREE(buf);

    return true;
}

#ifndef PLATFORM_POSIX
#pragma warning( default: 4786 ) // identifier was truncated to 255 characters in the browser information
#endif
