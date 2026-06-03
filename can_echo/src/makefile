
# source /home/jack/omapl138/ti/mcsdk_1_01_00_02/linux-devkit/environment-setup

#CC       = gcc
CFLAGS   = -Wall -g
INCLUDES = -I.
LDFLAGS  =
VPATH    = .
OBJS     = can_echo.o

TARGETS  = can_echo

all : $(TARGETS)

tl_can_echo: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o:%.c
	$(CC) -c $< -o $@ $(INCLUDES) $(CFLAGS)

.PHONY:
clean:
	rm -f $(TARGETS) *.o
