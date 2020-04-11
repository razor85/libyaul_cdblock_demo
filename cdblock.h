/*
 * Copyright (c) 2020 - Romulo Fernandes Machado Leitao
 * See LICENSE for details.
 *
 * Romulo Fernandes Machado Leitao <abra185@gmail.com>
 */

#pragma once

#include <yaul.h>

#define HASH_PRIME 31
#define HASH_CUT_NUMBER 1000000009
#define HASH_CHAR(X) ((X) - 31)

namespace CdBlock {


enum VolumeDescriptorTypes {
  VD_BOOT_RECORD = 0,
  VD_PRIMARY,
  VD_SUPPLEMENTARY,
  VD_PARTITION_DESCRIPTOR,

  VD_SET_TERMINATOR = 0xFF
};

template <typename T>
struct MultiEndianNumber {
  T l;
  T b;

  // Return big endian on saturn.
  const T operator()() const { return b; }
} __packed;

struct Sector {
  uint8_t data[2048];
} __packed;

struct Date {
  uint8_t date[17];
} __packed;

struct RecordingDateTime {
  uint8_t date[7];
} __packed;

#define FLAG_CDBLOCK_HIDDEN               (1 << 0)
#define FLAG_CDBLOCK_DIRECTORY            (1 << 1)
#define FLAG_CDBLOCK_ASSOCIATED_FILE      (1 << 2)
#define FLAG_CDBLOCK_EXT_FORMAT           (1 << 3)
#define FLAG_CDBLOCK_EXT_PERMISSIONS      (1 << 4)
#define FLAG_CDBLOCK_CONTINUE_NEXT_EXTENT (1 << 7)

struct DirectoryRecord {
  uint8_t length;
  uint8_t extendedAttributeLength;
  MultiEndianNumber<uint32_t> extentLocation;
  MultiEndianNumber<uint32_t> extentLength;
  RecordingDateTime recordingDateTime;
  uint8_t flags;
  uint8_t unitSizeInterleavedMode;
  uint8_t gapSizeInterleavedMode;
  MultiEndianNumber<uint16_t> volumeSequenceNumber;
  uint8_t identifierLength;
  
  inline bool isDirectory() const { return flags & FLAG_CDBLOCK_DIRECTORY; }
  char* identifierPtr() const { 
    char* ptr = (char*) &identifierLength;
    ptr++;

    return ptr;
  }

  DirectoryRecord *nextDir() {
    uint8_t *dirPtr = (uint8_t*) this;
    dirPtr += length;

    return (DirectoryRecord*) dirPtr;
  }
} __packed;

struct RootDirectoryRecord : public DirectoryRecord {
  uint8_t identifier;
} __packed;

struct VolumeDescriptorSetCommon {
  uint8_t type;
  uint8_t identifier[5];
  uint8_t version;

  inline bool isTerminator() const { return type == VD_SET_TERMINATOR; }
} __packed;

struct VolumeDescriptorSet : public VolumeDescriptorSetCommon {
  uint8_t data[2041];
} __packed;

struct PrimaryVolumeDescriptor : public VolumeDescriptorSetCommon {
  uint8_t unused;
  char systemIdentifier[32];
  char volumeIdentifier[32];
  uint8_t unused2[8];

  MultiEndianNumber<int32_t> volumeSpaceSize;
  uint8_t unused3[32];

  MultiEndianNumber<int16_t> volumeSetSize;
  MultiEndianNumber<int16_t> volumeSequenceNumber;
  MultiEndianNumber<int16_t> logicalBlockSize;
  MultiEndianNumber<int32_t> pathTableSize;

  int32_t locationPathTableLittle;
  int32_t locationOptionalPathTableLittle;

  int32_t locationPathTableBig;
  int32_t locationOptionalPathTableBig;

  RootDirectoryRecord rootDirectoryRecord;

  char volumeSetIdentifier[128];
  char publisherIdentifier[128];
  char dataPreparerIdentifier[128];
  char applicationIdentifier[128];
  char copyrightFileIdentifier[38];
  char abstractFileIdentifier[36];
  char bibliographicFileIdentifier[37];

  Date volumeCreationDateTime;
  Date volumeModificationDateTime;
  Date volumeExpirationDateTime;
  Date volumeEffectiveDateTime;

  int8_t fileStructureVersion;
  int8_t unused4;
  
  uint8_t applicationUsed[512];
  uint8_t isoReserved[653];
} __packed;

/**
 * ISO9660 Disk Data.
 */
struct FilesystemData {
  // Root sector read from the filesystem.
  Sector rootSector;

  // Sector to operate temporary data.
  Sector tempSector;

  inline RootDirectoryRecord* root() {
    return (RootDirectoryRecord*)& rootSector;
  };
};

/**
 * Entry in the file table.
 */
struct FilesystemEntry {
  uint32_t filenameHash;
  uint32_t lba;
  uint32_t size;

  // We compare entries by the hash.
  inline bool operator == (const FilesystemEntry& other) const {
      return filenameHash == other.filenameHash;
  }
    
  inline bool operator < (const FilesystemEntry& other) const {
      return filenameHash < other.filenameHash;
  }
    
  inline bool operator > (const FilesystemEntry& other) const {
      return filenameHash > other.filenameHash;
  }
};

/**
 * Filesystem Header Table.
 */
struct FilesystemHeaderTable {
  uint32_t numEntries;

  // Entries will point to an user allocated memory region that will
  // store all entries. When deallocating this, user should free the
  // 'entries' pointer.
  FilesystemEntry *entries;
};

/**
 * Can be called by navigateDirectory when an entry is found. Parameters
 * are the directory entry first, navigation depth in filesystem and
 * and optional user data pointer that is passed to navigateDirectory.
 */
typedef void (*RecordFunction)(DirectoryRecord*, int, void*);

/**
 * Initialize CDBlock subsystem.
 */
extern int initialize();

/**
 * Read the disk as a ISO9660 filesystem.
 *
 * @param fsData Pointer to where store the read FilesystemData.
 *
 * @return 0 If successful.
 */
extern int readFilesystem(FilesystemData *fsData);

/**
 * Navigate the filesystem recursively, applying the passed recordFunction
 * to every file/directory entry in the filesystem.
 *
 * @param fsData Pointer to initialized filesystem entry.
 *
 * @param recordFunction User specified function to be applied to every
 *                       read entry of the filesystem.
 *
 * @param userData Optinal user data pointer that can be passed to the
 *                 recordFunction.
 */
extern void navigateFilesystem(FilesystemData *fsData, 
  RecordFunction recordFunction, void *userData);

/**
 * Print (dbgio_buffer) every entry found in the filesystem.
 *
 * @param fsData Pointer to initialized filesystem entry.
 */
extern void printCdStructure(FilesystemData *fsData);

/**
 * Return the size in bytes required to store the Filesystem Header Table.
 */
extern uint32_t getHeaderTableSize(FilesystemData *fsData);

/**
 * Fill the passed Filesystem Header Table. The allocated header table 
 * pointer must point to a memory location with at least 
 * getHeaderTableSize() bytes available.
 */
extern void fillHeaderTable(FilesystemData *fsData, 
  FilesystemHeaderTable *headerTable);

/**
 * Return file entry.
 * @param headerTable Filesystem header table.
 * @param filenameHash Hash of the file to be searched. Use 
 *                     getFilenameHash for this.
 * @param resultingEntry If file is found, the resulting entry point to 
 *                       the file entry on the header table.
 */
extern void getFileEntry(FilesystemHeaderTable *headerTable, 
  uint32_t filenameHash, FilesystemEntry **resultingEntry);

/**
 * Return file contents from the specified entry.
 * @param entry A file entry in the header table.
 * @param buffer File contents will be returned in this buffer.
 *
 * @return 0 If reading was successful.
 */
extern int getFileContents(FilesystemEntry *entry, void *buffer);

/**
 * Generate a hash based on passed parameters.
 *
 * @param filename Name of the file to generate the hash.
 * @param length Length of the filename string.
 * @param startingHash In case of appending to a hash, this is the 
 *                     starting hash.
 * @param firstPrime The first prime to be used in the sequence.
 * @param primeFactor The number we will multiply the prime each iteration.
 * @param lastPrime If not nullptr, returns the last prime used.
 */
#ifdef __cplusplus
constexpr uint32_t generateHash(const char* filename, uint32_t length, 
  uint32_t startingHash, uint32_t firstPrime, uint32_t primeFactor, 
  uint32_t *lastPrime) {

#else // __cplusplus
uint32_t generateHash(const char* filename, uint32_t length, 
  uint32_t startingHash, uint32_t firstPrime, uint32_t primeFactor, 
  uint32_t *lastPrime) {
#endif

  assert(filename != nullptr);

  uint32_t hash = startingHash;
  uint32_t prime = firstPrime;
  for (uint32_t i = 0; i < length; ++i) {
    hash += HASH_CHAR(filename[i]) * prime;
    hash %= HASH_CUT_NUMBER;
    prime *= primeFactor;
  }
    
  if (lastPrime != nullptr)
    *lastPrime = prime;

  return hash;
}

/**
 * Generate a cdblock filename hash.
 */
#ifdef __cplusplus
constexpr uint32_t getFilenameHash(const char *filename, uint32_t length) {
#else // __cplusplus
uint32_t getFilenameHash(const char *filename, uint32_t length) {
#endif

  return generateHash(filename, length, 0, HASH_PRIME, 
    HASH_PRIME, nullptr);
}


} // namespace cdblock

