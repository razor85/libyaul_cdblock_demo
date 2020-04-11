/*
 * Copyright (c) 2020 - Romulo Fernandes Machado Leitao
 * See LICENSE for details.
 *
 * Romulo Fernandes Machado Leitao <abra185@gmail.com>
 */

#ifndef _FILESYSTEM_H_
#define _FILESYSTEM_H_

#include <yaul.h>
#include "cdblock.h"

#define INVALID_FILE_SIZE 0xFFFFFFFF

enum class FilesystemBackend {
  CDBLOCK,
  USB,

  // Pick based on user configuration of Filesystem.
  AUTO
};

// Forward declaration.
class Filesystem;

class File {

friend class Filesystem;
public:
  File() = delete;
  File(File& other) = delete;
  File(File&& other) = default;

  File& operator = (File& other);
  File& operator = (File&& other);

  uint32_t readData(void* dest, uint32_t len);
  void skipData(uint32_t len);

  void seek(uint32_t fromPosition, uint32_t numOfBytes);
  void close();

  inline void *getData() const { return ptr; }
  inline uint32_t size() const { return length; }

  ~File();

private:
  File(void *ptr, const char* filename, FilesystemBackend backend);

  FilesystemBackend backend;
  uint32_t length;
  uint32_t seekPos;
  void *ptr;
};

class Filesystem {
public:
  static void initialize();
  static void printCdStructure();
  static void setDefaultBackend(FilesystemBackend backend);

  static File open(const char* filename, 
    FilesystemBackend backend = FilesystemBackend::AUTO);

  static uint32_t getFileSize(const char* filename);

  static uint32_t getFileSize(uint32_t filenameHash);

  static CdBlock::FilesystemHeaderTable *getCdBlockHeaderTable() { 
    return &cdHeaderTable; 
  }

private:
  static FilesystemBackend defaultBackend;

  static void* filesystemPtr;
  static CdBlock::FilesystemData cdFilesystemData;
  static CdBlock::FilesystemHeaderTable cdHeaderTable;
};


#endif // _FILESYSTEM_H_
