/*
 * Copyright (c) 2020 - Romulo Fernandes Machado Leitao
 * See LICENSE for details.
 *
 * Romulo Fernandes Machado Leitao <abra185@gmail.com>
 */

#include <yaul.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "cdblock.h"
#include "filesystem.h"


namespace {


void printToBuffer(const char* contents, uint32_t size) {
  static char tmpMsgBuffer[1024];

  sprintf(tmpMsgBuffer, "%s\n", contents);
  tmpMsgBuffer[size] = 0;

  dbgio_buffer(tmpMsgBuffer);
}

void printFileContents(const char* filename) {
  File handle = Filesystem::open(filename);
  printToBuffer(static_cast<const char*>(handle.getData()), 
    handle.size());
}

void hardwareInit() {
  // Make sure USB cart is working for remote access.
  usb_cart_init();

  vdp2_tvmd_display_res_set(VDP2_TVMD_INTERLACE_NONE, VDP2_TVMD_HORZ_NORMAL_A,
      VDP2_TVMD_VERT_224);

  vdp2_scrn_back_screen_color_set(VDP2_VRAM_ADDR(3, 0x01FFFE),
      COLOR_RGB555(0, 3, 15));

  cpu_intc_mask_set(0);

  vdp2_tvmd_display_set();
}


} // namespace ''


void _assert(const char *file, const char *line, const char *func, 
  const char *expression) {

  char tmpBuffer[1024];
  sprintf(tmpBuffer, "Assertion failed at %s:%s (%s)\n", file, line, 
    expression);

  dbgio_buffer(tmpBuffer);
  dbgio_flush();
  vdp_sync(0);

  while (true) 
    ;
}


int main(void) {
  hardwareInit();

  dbgio_dev_default_init(DBGIO_DEV_VDP2_ASYNC);

  // Start filesystem
  Filesystem::initialize();

  dbgio_buffer("\nSaturn Drive contents:\n");
  Filesystem::printCdStructure();

  // Select between loading from the USB (cd folder) or from the disk itself.
  // Filesystem::setDefaultBackend(FilesystemBackend::USB);
  Filesystem::setDefaultBackend(FilesystemBackend::CDBLOCK);

  char tmpBuffer[1024];
  sprintf(tmpBuffer, "\n\nTEST_FILE.TXT contents:\n");
  dbgio_buffer(tmpBuffer);

  printFileContents("TEST_FILE.TXT");

  sprintf(tmpBuffer, "\n\nA_FOLDER/ANOTHER_TEST_FILE.TXT contents:\n");
  dbgio_buffer(tmpBuffer);
  printFileContents("A_FOLDER/ANOTHER_TEST_FILE.TXT");

  dbgio_flush();
  vdp_sync(0);

  while (true)
    ;
}

