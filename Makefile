CC            := gcc
CFLAGS        := -Wall -std=gnu99
MWFLAGS       := -DMEMWATCH -DMW_STDIO
INCFLAGS      := -I./src/include
SRCSDIR       := ./src
BINSDIR       := ./bin
SERVERSRCS    := ./src/procnanny.server.c ./src/bst.c ./src/cfgparser.c \
		./src/pwlog.c ./src/pwwrapper.c ./src/memwatch.c
SERVEROBJS    := $(SERVERSRCS:.c=.o)
CLIENTSRCS    := ./src/procnanny.client.c

all: $(BINSDIR)/procnanny.server #$(BINSDIR)/procnanny.client

rebuild: clean all

debug: $(BINSDIR)/procnanny.server_debug #$(BINSDIR)/procnanny.client_debug

rebuild_debug: clean #debug

clean:
	-rm -f $(SRCSDIR)/*.o $(SRCSDIR)/*.gch \
		$(BINSDIR)/{procnanny.server,procnanny.client} \
		$(BINSDIR)/{procnanny.server_debug,procnanny.client_debug}

# Server
$(BINSDIR)/procnanny.server : server_objs
	@mkdir -p $(BINSDIR)
	$(CC) $(CFLAGS) $(SERVEROBJS) -o $@

server_objs : $(SERVERSRCS)
	-rm -f $(SERVEROBJS)
	$(CC) $(CFLAGS) $(MWFLAGS) $(INCFLAGS) -c $^ -O2
	-mv *.o $(SRCSDIR)

$(BINSDIR)/procnanny.server_debug : server_debug_objs
	@mkdir -p $(BINSDIR)
	$(CC) $(CFLAGS) $(SERVEROBJS) -o $@

server_debug_objs : $(SERVERSRCS)
	-rm -f $(SERVEROBJS)
	$(CC) $(CFLAGS) $(MWFLAGS) $(INCFLAGS) -c $^ -ggdb3 -O0
	-mv *.o $(SRCSDIR)
# Server

.PHONY: all rebuild debug rebuild_debug clean \
	server_objs server_debug_objs
.IGNORE:
