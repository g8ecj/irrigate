

ifeq ($(TARGET_CC),)
   STRIP := strip
   CFLAGS := -DPC
   COMPILER = CC
else
   COMPILER = XCC
endif




DEFINES = -DMONGOOSE_NO_THREADS -DNS_DISABLE_THREADS

override LIBS += -ljson-c -lowcapi
override CFLAGS += $(DEFINES) -O1 -g -Wall -W 

RM := rm -rf

# Disable standard make output
MAKEFLAGS += --silent


SOURCES += \
./mongoose.c \
./irr_actions.c \
./irr_chanmap.c \
./irr_onewire.c \
./irr_parse.c \
./irr_queue.c \
./irr_schedule.c \
./irr_stats.c \
./irr_web.c \
./irr_utils.c \
./irrigate.c


OBJS = $(SOURCES:.c=.o)
BINARY = irrigate
CLEAN_FILES = *~ *.o core $(BINARY)

all: $(BINARY)

$(BINARY): $(OBJS)
	echo "$(COMPILER) $(OBJS) -> $(BINARY)"
	$(CC) $(CFLAGS) $(OBJS) -o $(BINARY) $(LIBS)
#	$(STRIP) $(BINARY)
	@echo 'Finished building target: $@'

%.o: %.c
	echo "$(COMPILER) $@"
	$(CC) $(CFLAGS) -c $<

# Other Targets
clean:
	echo "RM $(CLEAN_FILES)"
	$(RM) $(CLEAN_FILES)
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

