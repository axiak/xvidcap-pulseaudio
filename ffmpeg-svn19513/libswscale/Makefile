
include ../config.mak

NAME=swscale
ifeq ($(BUILD_SHARED),yes)
LIBVERSION=$(SWSVERSION)
LIBMAJOR=$(SWSMAJOR)
EXTRALIBS := -L../libavutil -lavutil$(BUILDSUF) $(EXTRALIBS)
endif

# NOTE: -I.. is needed to include config.h
CFLAGS=-I.. -I$(SRC_PATH) -I$(SRC_PATH)/libavutil $(OPTFLAGS) \
       -DHAVE_AV_CONFIG_H -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE \
       -D_GNU_SOURCE

OBJS= swscale.o rgb2rgb.o yuv2rgb.o
ifeq ($(TARGET_ALTIVEC),yes)
OBJS+=  yuv2rgb_altivec.o
endif

HEADERS = swscale.h rgb2rgb.h

include $(SRC_PATH)/common.mak

cs_test: cs_test.c
	$(CC) $(CFLAGS) -o $@ $^ -L. -lswscale -L$(SRC_PATH)/libavutil -lavutil -lm

swscale-example: swscale-example.c
	$(CC) $(CFLAGS) -o $@ $^ -L. -lswscale -L$(SRC_PATH)/libavutil -lavutil -lm
