/*
 * Copyright (c) 2020 - Romulo Fernandes Machado Leitao
 * See LICENSE for details.
 *
 * Romulo Fernandes Machado Leitao <abra185@gmail.com>
 */


#include <yaul.h>
#include "crc.h"
#include "filesystem.h"

FilesystemBackend Filesystem::defaultBackend;
CdBlock::FilesystemData Filesystem::cdFilesystemData;
CdBlock::FilesystemHeaderTable Filesystem::cdHeaderTable;

// Debugging?
// #define DEBUG_FILESYSTEM

namespace {


enum TransferCommands {
  TC_REQUEST_FILE = 0,
  TC_REQUEST_FILE_SIZE,
  TC_INVALID = 0xFF
};

uint32_t usbGetFileSize(uint32_t filenameHash) {

  // Send command and wait for our bytes.
  usb_cart_byte_send((uint8_t)TC_REQUEST_FILE_SIZE);
  usb_cart_long_send(filenameHash);

  // Wait for answer.
  return usb_cart_long_read();
}

inline uint32_t usbGetFileSize(const char* filename, uint32_t length) {
  return usbGetFileSize(CdBlock::getFilenameHash(filename, length));
}

uint32_t usbGetFileData(const char* filename, uint32_t filenameLength, 
  void* buffer) {

  // Calculate hash.
  const uint32_t hash = CdBlock::getFilenameHash(filename, filenameLength);

  // Send command and wait for our bytes.
  usb_cart_byte_send((uint8_t)TC_REQUEST_FILE);
  usb_cart_long_send(hash);

  const uint32_t fileSize = usb_cart_long_read();
  if (fileSize == 0)
    return 0;

  for (uint32_t i = 0; i < fileSize; ++i)
    ((uint8_t*)buffer)[i] = (unsigned char) usb_cart_byte_read();

  // Check crc.
  const uint8_t crc = usb_cart_byte_read();

  // Calculate crc.
  const uint8_t calculatedCRC = crc_finalize(crc_update(0, 
    (const unsigned char*) buffer, fileSize));

  usb_cart_byte_send(calculatedCRC != crc);

  // Read bytes.
  return fileSize;
}


} // namespace ''


File::File(void *passPtr, const char *filename, FilesystemBackend pBackend)
  : backend(pBackend),
    length(0),
    seekPos(0) {

  switch (backend) {
  case FilesystemBackend::CDBLOCK:
    {
      CdBlock::FilesystemEntry *fsEntry = nullptr;
      CdBlock::getFileEntry(Filesystem::getCdBlockHeaderTable(),
        CdBlock::getFilenameHash(filename, strlen(filename)), &fsEntry);

#ifdef DEBUG_FILESYSTEM
      if (fsEntry == nullptr) {
        char tmpBuffer[1024];
        sprintf(tmpBuffer, "File %s not found!\n", filename);
        dbgio_buffer(tmpBuffer);
        dbgio_flush();
      }
#endif

      assert(fsEntry != nullptr);
      length = fsEntry->size;
      ptr = malloc(fsEntry->size);

      assert(ptr != nullptr);

      const int stat = CdBlock::getFileContents(fsEntry, ptr);
      assert(stat == 0);
    }
    break;

  case FilesystemBackend::USB:
    {
      length = usbGetFileSize(filename, strlen(filename));

#ifdef DEBUG_FILESYSTEM
      if (length == 0) {
        char tmpBuffer[1024];
        sprintf(tmpBuffer, "File %s not found!\n", filename);
        dbgio_buffer(tmpBuffer);
        dbgio_flush();
      }
#endif

      assert(length != 0);
      ptr = malloc(length);
      assert(ptr != nullptr);

      uint32_t getSize = 0;
      do {
        getSize = usbGetFileData(filename, strlen(filename), ptr);
      } while (getSize != length);
    }
    break;
  default:
  case FilesystemBackend::AUTO:
    assert(false);
    break;
  }
}

File& File::operator = (File& other) {
  memcpy(this, &other, sizeof(File));
  memset(&other, 0, sizeof(File));

  return *this;
}
  
File& File::operator = (File&& other) {
  memcpy(this, &other, sizeof(File));
  memset(&other, 0, sizeof(File));

  return *this;
}

File::~File() {
  close();
}

uint32_t File::readData(void* dest, uint32_t len) {
  switch (backend) {
  case FilesystemBackend::CDBLOCK:
  case FilesystemBackend::USB:
    assert(dest != nullptr);
    memcpy(dest, (uint8_t*)ptr + seekPos, len);
    seekPos += len;
    return len;
     
  default:
  case FilesystemBackend::AUTO:
    assert(false);
    break;
  }
}

void File::skipData(uint32_t len) {
  switch (backend) {
  case FilesystemBackend::CDBLOCK:
  case FilesystemBackend::USB:
    seekPos += len;
    break;
     
  default:
  case FilesystemBackend::AUTO:
    assert(false);
    break;
  }
}

void File::seek(uint32_t fromPosition, uint32_t numOfBytes) {
  switch (backend) {
  case FilesystemBackend::CDBLOCK:
  case FilesystemBackend::USB:
    if (fromPosition == SEEK_SET)
      seekPos = numOfBytes;
    else if (fromPosition == SEEK_CUR)
      seekPos += numOfBytes;
    else if (fromPosition == SEEK_END)
      seekPos = length + numOfBytes;
    break;
     
  default:
  case FilesystemBackend::AUTO:
    assert(false);
    break;
  }
}
  
void File::close() {
  switch (backend) {
  case FilesystemBackend::CDBLOCK:
  case FilesystemBackend::USB:
    if (ptr != nullptr)
      free(ptr);
    break;

  default:
  case FilesystemBackend::AUTO:
    assert(false);
    break;
  }
  
  ptr = nullptr;
  length = 0;
  seekPos = 0;
}

void Filesystem::initialize() {
  // CDBlock Initialization.
  const int stat = CdBlock::initialize();
  assert(stat == 0);

  CdBlock::readFilesystem(&cdFilesystemData);

  // Create cd entries table (necessary for looking for files).
  const uint32_t tableSize = CdBlock::getHeaderTableSize(&cdFilesystemData);
  cdHeaderTable.entries = (CdBlock::FilesystemEntry*) malloc(tableSize);
  CdBlock::fillHeaderTable(&cdFilesystemData, &cdHeaderTable);

  // Set default backend.
  defaultBackend = FilesystemBackend::CDBLOCK;
}
  
void Filesystem::printCdStructure() {
  CdBlock::printCdStructure(&cdFilesystemData);
}

void Filesystem::setDefaultBackend(FilesystemBackend backend) {
  defaultBackend = backend;
}
  
File Filesystem::open(const char* filename, FilesystemBackend backend) {
  const FilesystemBackend usingBackend = 
    (backend == FilesystemBackend::AUTO) ? 
    defaultBackend 
    : 
    backend;

  switch (usingBackend) {
  case FilesystemBackend::CDBLOCK:
  case FilesystemBackend::USB:
    return File(nullptr, filename, usingBackend);

  case FilesystemBackend::AUTO:
  default:
    break;
  }

  // Never reaches.
  assert(false);
  return File(nullptr, filename, usingBackend);
}
  
uint32_t Filesystem::getFileSize(uint32_t filenameHash) {
  switch (defaultBackend) {
  case FilesystemBackend::CDBLOCK:
    {
      CdBlock::FilesystemEntry *fsEntry = nullptr;
      CdBlock::getFileEntry(getCdBlockHeaderTable(), 
        filenameHash, &fsEntry);

      if (fsEntry == nullptr)
        return INVALID_FILE_SIZE;
      else
        return fsEntry->size;
    }

  case FilesystemBackend::USB:
    {
      const uint32_t size = usbGetFileSize(filenameHash);
      if (size == 0)
        return INVALID_FILE_SIZE;
      else
        return size;
    }

  default:
  case FilesystemBackend::AUTO:
    break;
  }

  assert(false);
  return INVALID_FILE_SIZE;
}
  
uint32_t Filesystem::getFileSize(const char* filename) {
  return getFileSize(CdBlock::getFilenameHash(filename, strlen(filename)));
}

