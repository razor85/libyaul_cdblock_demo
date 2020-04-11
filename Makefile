ifeq ($(strip $(YAUL_INSTALL_ROOT)),)
  $(error Undefined YAUL_INSTALL_ROOT (install root directory))
endif

include $(YAUL_INSTALL_ROOT)/share/pre.common.mk

SH_PROGRAM:= cdblock_demo
SH_OBJECTS:= cdblock.o \
	crc.o \
	filesystem.o \
  main.o

SH_LIBRARIES:=
SH_CFLAGS+= -O2 -I. -g -std=c++14

IP_VERSION:= V1.000
IP_RELEASE_DATE:= 20200411
IP_AREAS:= JTUBKAEL
IP_PERIPHERALS:= JAMKST
IP_TITLE:= cdblock
IP_MASTER_STACK_ADDR:= 0x06004000
IP_SLAVE_STACK_ADDR:= 0x06002000
IP_1ST_READ_ADDR:= 0x06004000

M68K_PROGRAM:=
M68K_OBJECTS:=

include $(YAUL_INSTALL_ROOT)/share/post.common.mk
