TARGET = hello
OBJS = hello.o
CFLAGS = -O2 -G0 -Wall
LIBS = -lpspdebug -lpspdisplay -lpspge -lpspctrl
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PSP Builder Windows Test
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
