CC := gcc
CFLAGS := -Wall -std=gnu99
MWFLAGS := -DMEMWATCH -DMW_STDIO
INCFLAGS := -I./src/include
SRCS := $(wildcard ./src/*.c)
SRCSDIR := src
BINSDIR := bin
BUILDDIR := build
OBJS := $(SRCS:.c=.o)

all: $(BINSDIR)/procnanny

rebuild: clean all

debug: $(BINSDIR)/procnanny_debug

rebuild_debug: clean debug

clean:
	-rm -f $(SRCSDIR)/*.o $(SRCSDIR)/*.gch \
		$(BINSDIR)/{procnanny,procnanny_debug}

$(BINSDIR)/procnanny: $(OBJS)
	@mkdir $(BINSDIR)
	$(CC) $(CFLAGS) $(OBJS) -o $@

$(OBJS): $(SRCS)
	$(CC) $(CFLAGS) $(MWFLAGS) $(INCFLAGS) -c $^ -O2
	-mv *.o $(SRCSDIR)

$(BINSDIR)/procnanny_debug: debug_objs
	@mkdir $(BINSDIR)
	$(CC) $(CFLAGS) $(OBJS) -o $@

debug_objs: $(SRCS)
	$(CC) $(CFLAGS) $(MWFLAGS) $(INCFLAGS) -c $^ -g3 -O0
	-mv *.o $(SRCSDIR)

.IGNORE:
