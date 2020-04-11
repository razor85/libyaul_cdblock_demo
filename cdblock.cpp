/*
 * Copyright (c) 2020 - Romulo Fernandes Machado Leitao
 * See LICENSE for details.
 *
 * Romulo Fernandes Machado Leitao <abra185@gmail.com>
 */

#include "cdblock.h"
#include <cd-block.h>
#include <ctype.h>


// Debugging Functions
// #define DEBUG_CDBLOCK


namespace CdBlock {


namespace {

template <typename T>
void binarySearch(T *entries, uint32_t entriesLength, 
  T searchElement, T **foundEntry) {

  const uint32_t pivotIndex = entriesLength / 2;
  T *pivot = &entries[pivotIndex];

  if (*pivot == searchElement) {
    *foundEntry = pivot;

  } else if (entriesLength > 1) {
    if (*pivot > searchElement) {
      binarySearch(entries, pivotIndex, searchElement, foundEntry);

    } else {
      binarySearch(&entries[pivotIndex],
        entriesLength - pivotIndex, searchElement, foundEntry);
    }
  }
}

template <typename T>
void quickSort(T *entries, int32_t left, int32_t right) {

  // Already sorted.
  if (right <= left)
    return;

  int32_t pivotIndex = left + (right - left) / 2;
  int32_t l = left - 1;
  int32_t r = right + 1;
  
  const T pivot = entries[pivotIndex];
  for (;;) {
    do {
      l++;
    } while (entries[l] < pivot);
    
    do {
      r--;
    } while (entries[r] > pivot);

    if (l >= r) {
      pivotIndex = r;
      break;
    }
  
    // Swap
    const T tmp = entries[r];
    entries[r] = entries[l];
    entries[l] = tmp;
  }

  quickSort(entries, left, pivotIndex);
  quickSort(entries, pivotIndex + 1, right);
}

/**
 * Just print every file / folder found (for debugging purposes).
 */
void printDirectoryRecord(DirectoryRecord *record, int level, void*) {
  for (int i = 0; i < level; ++i)
    dbgio_buffer("  ");

  char tmpBuffer[1024];

  // -2 takes into account ';1'
  uint8_t identifierSize = record->identifierLength;
  if (record->isDirectory() == 0 && identifierSize > 2)
    identifierSize -= 2;

  char identifierName[256];
  memcpy(identifierName, record->identifierPtr(), identifierSize);
  identifierName[identifierSize] = 0;

  if (record->isDirectory()) {
    sprintf(tmpBuffer, "- [%s] @ %lud\n", identifierName, record->extentLocation());
  } else {
    sprintf(tmpBuffer, "- %s @ %lud\n", identifierName, record->extentLocation());
  }

  dbgio_buffer(tmpBuffer);
}

void navigateDirectory(DirectoryRecord *record, 
  int level, RecordFunction recordFunction, void *userData, 
  bool continueReading = false) {

  assert(record != nullptr);

  // Skip empty entries.
  if (record->length == 0)
    return;
  
  // Skip '.'
  DirectoryRecord *dir = record;
  if (!continueReading) {
    dir = record->nextDir();
    assert(dir->length != 0);
  
    // Skip '..'
    dir = dir->nextDir();
  }
      
  // Visit every entry on the directory.
  while (dir->length != 0) {
    if (recordFunction != nullptr)
      recordFunction(dir, level, userData);

    // Visit sub-directory recursively.
    if (dir->isDirectory()) {
      uint32_t extraLevels = dir->extentLength() / 2048;
      if (dir->extentLength() % 2048)
        extraLevels++;

      for (uint32_t level = 0; level < extraLevels; ++level) {
        Sector sector;
        const int stat = cd_block_read_data(LBA2FAD(dir->extentLocation()) +
          level, 2048, sector.data);
        
        assert(stat == 0);
  
        DirectoryRecord *newRecord = (DirectoryRecord*) sector.data;
        navigateDirectory(newRecord, level + 1, recordFunction, userData, 
          level > 0);
      }
    }

    dir = dir->nextDir();
  }
}

/**
 * Fill a filesystem entry and then jump to the next entry. In case of a
 * child, the parent hash will be passed to generate the child hash.
 */
void fillHeaderTableEntry(DirectoryRecord *record, uint32_t parentHash, 
  uint32_t parentPrime, FilesystemEntry **entry, 
  FilesystemHeaderTable *headerTable, bool continueReading = false) {

  assert(record != nullptr);
  assert(headerTable != nullptr);

  // Skip empty entries.
  if (record->length == 0)
    return;

  DirectoryRecord *dir = record;
  if (!continueReading) {

    // Skip '.'
    dir = record->nextDir();
    assert( dir->length != 0 );

    // Skip '..'
    dir = dir->nextDir();
  }
      
  // Visit every entry on the directory.
  while (dir->length != 0) {

    // -2 takes into account ';1'
    uint32_t identifierSize = dir->identifierLength;
    if (dir->isDirectory() == 0 && identifierSize > 2)
      identifierSize -= 2;

    uint32_t lastPrime = 0;
    uint32_t hash = generateHash(dir->identifierPtr(), identifierSize,
      parentHash, parentPrime, HASH_PRIME, &lastPrime);

#ifdef DEBUG_CDBLOCK
    char identifierName[256];
    memcpy(identifierName, dir->identifierPtr(), identifierSize);
    identifierName[identifierSize] = 0;

    char tmpBuffer[1024];
    sprintf(tmpBuffer, "Added %s (%lu) to header table\n", identifierName, hash);
    dbgio_buffer(tmpBuffer);
    dbgio_flush();
#endif

    if (dir->isDirectory()) {
      // Add '/'
      hash += HASH_CHAR('/') * lastPrime;
      hash %= HASH_CUT_NUMBER;
      lastPrime *= HASH_PRIME;

      // Visit children.
      uint32_t extraLevels = dir->extentLength() / 2048;
      if (dir->extentLength() % 2048)
        extraLevels++;
      
      for (uint32_t level = 0; level < extraLevels; ++level) {
        Sector sector;

        const int stat = cd_block_read_data(LBA2FAD(dir->extentLocation()) + 
          level, 2048, sector.data);

        assert(stat == 0);

        DirectoryRecord *newRecord = (DirectoryRecord*) sector.data;
        fillHeaderTableEntry(newRecord, hash, lastPrime, entry, 
          headerTable, level > 0);
      }

    } else {

      // Add file entry.
      (*entry)->filenameHash = hash;
      (*entry)->lba = dir->extentLocation();
      (*entry)->size = dir->extentLength();
      
      if (dir->extentLength() == 0) {
        char identifierName[256];
        memcpy(identifierName, dir->identifierPtr(), identifierSize);
        identifierName[identifierSize] = 0;

        char tmpBuffer[1024];
        sprintf(tmpBuffer, "Invalid 0 byte file detected: %s\n", identifierName);
        dbgio_buffer(tmpBuffer);
        dbgio_flush();

        assert(false);
      }

      headerTable->numEntries += 1;
      (*entry) += 1;
    } 

    dir = dir->nextDir();
  }
}


} // namespace ''


int initialize() {
  static_assert(sizeof(VolumeDescriptorSet) == 2048, 
    "VolumeDescriptorSet size mismatch.");

  static_assert(sizeof(PrimaryVolumeDescriptor) == 2048, 
    "PrimaryVolumeDescriptor size mismatch.");

  int returnCode;
  if ((returnCode = cd_block_init(0x0002)) != 0)
    return returnCode;

  if (cd_block_cmd_is_auth(nullptr) == 0) {
    if ((returnCode = cd_block_bypass_copy_protection()) != 0)
      return returnCode;
  }

  return 0;
}

int readFilesystem(FilesystemData *fsData) {
  assert(fsData != nullptr);

  // Skip the first 16 sectors dedicated to IP.BIN
  uint8_t startFAD = LBA2FAD(16);
  CdBlock::VolumeDescriptorSet tempSet;

  // Find Primary Volume Descriptor.
  int cdBlockRet = 0;
  do {
    cdBlockRet = cd_block_read_data(startFAD, 2048, (uint8_t*) &tempSet);

    if (cdBlockRet != 0)
      return cdBlockRet;
    else if (tempSet.type != CdBlock::VD_PRIMARY)
      startFAD++;

  } while (tempSet.type != CdBlock::VD_PRIMARY);

  CdBlock::PrimaryVolumeDescriptor *primaryDescriptor = 
    (CdBlock::PrimaryVolumeDescriptor*) &tempSet;

  // Jump to root sector and retrieve it.
  const int stat = cd_block_read_data(
    LBA2FAD(primaryDescriptor->rootDirectoryRecord.extentLocation()), 
      2048, (uint8_t*) &fsData->rootSector);

  assert(stat == 0);
  return 0;
}

void navigateFilesystem(FilesystemData *fsData, 
  RecordFunction recordFunction, void *userData) {

  assert(fsData != nullptr);
  navigateDirectory(fsData->root(), 0, recordFunction, userData);
}


void printCdStructure(FilesystemData *fsData) {
  assert(fsData != nullptr);
  navigateDirectory(fsData->root(), 0, printDirectoryRecord, nullptr);
}

uint32_t getHeaderTableSize(FilesystemData *fsData) {
  assert(fsData != nullptr);

  uint32_t numEntries = 0;
  navigateDirectory(fsData->root(), 0, 
    [](DirectoryRecord *dir, int, void *length) {
      uint32_t *lengthData = (uint32_t*) length;
      if (!dir->isDirectory())
        (*lengthData)++;

    }, &numEntries
  );

  return (numEntries * sizeof(FilesystemEntry));
}

void fillHeaderTable(FilesystemData *fsData, 
  FilesystemHeaderTable *headerTable) {

  assert(fsData != nullptr);
  assert(headerTable != nullptr);
  assert(headerTable->entries != nullptr);

  headerTable->numEntries = 0;

  FilesystemEntry *iterEntry = headerTable->entries;
  fillHeaderTableEntry(fsData->root(), 0, HASH_PRIME, 
    &iterEntry, headerTable);

  quickSort(headerTable->entries, 0, headerTable->numEntries - 1);
}

void getFileEntry(FilesystemHeaderTable *headerTable, 
  uint32_t filenameHash, FilesystemEntry **resultingEntry) {

  assert(headerTable != nullptr);
  assert(resultingEntry != nullptr);

  binarySearch(headerTable->entries, headerTable->numEntries, 
    { filenameHash, 0, 0 }, resultingEntry);
}

int getFileContents(FilesystemEntry *entry, void *buffer) {
  assert(entry != nullptr);
  assert(buffer != nullptr);

  uint8_t tmpBuffer[2048];
  uint8_t *dstBuffer = (uint8_t*) buffer;

  uint32_t missingBytes = entry->size;
  uint32_t readingLBA = entry->lba;

  while (missingBytes > 0) {
    int ret = cd_block_read_data(LBA2FAD(readingLBA), 2048, tmpBuffer);
    if (ret != 0)
      return ret;

    if (missingBytes > 2048) {
      memcpy(dstBuffer, tmpBuffer, 2048);
      dstBuffer += 2048;
      missingBytes -= 2048;

      // Jump to next LBA.
      readingLBA++;
    } else {
      memcpy(dstBuffer, tmpBuffer, missingBytes);
      missingBytes = 0;
    }
  }

  return 0;
}


} // namespace CdBlock
