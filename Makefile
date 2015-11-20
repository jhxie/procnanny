CC         := gcc
CFLAGS     := -Wall -std=gnu99
MWFLAGS    := -DMEMWATCH -DMW_STDIO
INCFLAGS   := -I./src/include
SRCS       := $(wildcard ./src/*.c)
SERVERSRCS := ./src/procnanny.server.c ./src/pwwrapper.c
SERVEROBJS := $(SERVERSRCS:.c=.o)
CLIENTSRCS := $(filter-out ./src/procnanny.server.c, ./src/castack.c $(SRCS))
CLIENTOBJS := $(CLIENTSRCS:.c=.o)
SRCSDIR    := ./src
BINSDIR    := ./bin

all: $(BINSDIR)/procnanny.server $(BINSDIR)/procnanny.client

rebuild: clean all

debug: $(BINSDIR)/procnanny.server_debug $(BINSDIR)/procnanny.client_debug

rebuild_debug: clean debug

clean:
	-rm -f $(SRCSDIR)/*.o $(SRCSDIR)/*.gch \
		$(BINSDIR)/{procnanny.server,procnanny.client} \
		$(BINSDIR)/{procnanny.server_debug,procnanny.client_debug}

# Server
$(BINSDIR)/procnanny.server: $(SERVEROBJS)
	@mkdir -p $(BINSDIR)
	$(CC) $(CFLAGS) $(SERVEROBJS) -o $@

$(SERVEROBJS): $(SERVERSRCS)
	$(CC) $(CFLAGS) $(MWFLAGS) $(INCFLAGS) -c $^ -O2
	-mv *.o $(SRCSDIR)

$(BINSDIR)/procnanny.server_debug: server_debug
	@mkdir -p $(BINSDIR)
	$(CC) $(CFLAGS) $(SERVEROBJS) -o $@

server_debug: $(SERVERSRCS)
	$(CC) $(CFLAGS) $(MWFLAGS) $(INCFLAGS) -c $^ -g3 -O0
	-mv *.o $(SRCSDIR)

# Server

.PHONY: all rebuild debug rebuild_debug clean
.IGNORE:
