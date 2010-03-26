###########################################################################
#                           _              
#   ~Q~                    (_)             
#                 ___ _   ___ __ __      __
#                / __| | | | / __\ \ /\ / /
#               | (__  |_| | \__ \\ V  V / 
#                \___|\__,_|_|___/ \_/\_/  
#
# Copyright (C) 2008 - 8002, Cui Shaowei, <shaovie@gmail.com>, It's free.
# This is a general makefile template.
###########################################################################

                               BIN_DIR = ./bin
                                TARGET = $(BIN_DIR)/libnetdkit.so.1.0
                              C_CFLAGS = -Wall -W -Wpointer-arith -pipe -fPIC
                                MACROS = -D_REENTRANT -D__USE_POSIX -DNDK_RTLOG#-DNDK_STRACE
                                    CC = gcc
                            CPP_CFLAGS = -Wall -W -Wpointer-arith -pipe -fPIC
                                  MAKE = make
                                LINKER = g++
                          INCLUDE_DIRS = -I .
                                  LIBS = -lpthread
                            OPTIM_FLAG = -O2
                                   CPP = g++
                                LFLAGS = -Wl -shared -fPIC
                              LIB_DIRS =
                                 VPATH = ndk
                            OBJECT_DIR = ./.obj/
                              CPPFILES = \
                                         log_to_stdout.cpp  \
                                         logger.cpp  \
                                         task_base.cpp  \
                                         cache_manager.cpp  \
                                         timer_queue.cpp  \
                                         thread.cpp  \
                                         thread_manager.cpp  \
                                         mem_pool.cpp  \
                                         time_value.cpp  \
                                         log_to_file.cpp  \
                                         reactor.cpp  \
                                         inet_addr.cpp  \
                                         log_manager.cpp  \
                                         message_queue.cpp  \
                                         unix_reactor.cpp  \
                                         select_reactor.cpp  \
                                         strace.cpp  \
                                         acceptor.cpp  \
                                         epoll_reactor.cpp  \
                                         task.cpp  \
                                         ndk.cpp  \
                                         connector.cpp  \
                                         token.cpp   

                                CFILES = \
                                         iniparser.c  \
                                         dictionary.c   

# To use 'make debug=0' build release edition.
ifdef debug
	ifeq ("$(origin debug)", "command line")
		IS_DEBUG = $(debug)
	endif
else
	IS_DEBUG = 1
endif
ifndef IS_DEBUG
	IS_DEBUG = 1
endif
ifeq ($(IS_DEBUG), 1)
	OPTIM_FLAG += -g3
else
	MACROS += -DNDEBUG
endif

# To use 'make quiet=1' all the build command will be hidden.
# To use 'make quiet=0' all the build command will be displayed.
ifdef quiet
	ifeq ("$(origin quiet)", "command line")
		QUIET = $(quiet)
	endif
endif
ifeq ($(QUIET), 1)
	Q = @
else
	Q =
endif

OBJECTS := $(addprefix $(OBJECT_DIR), $(notdir $(CPPFILES:%.cpp=%.o)))
OBJECTS += $(addprefix $(OBJECT_DIR), $(notdir $(CFILES:%.c=%.o)))

CALL_CFLAGS := $(C_CFLAGS) $(INCLUDE_DIRS) $(MACROS) $(OPTIM_FLAG)
CPPALL_CFLAGS := $(CPP_CFLAGS) $(INCLUDE_DIRS) $(MACROS) $(OPTIM_FLAG)
LFLAGS += $(LIB_DIRS) $(LIBS) $(OPTIM_FLAG)

all: checkdir $(TARGET)

$(TARGET): $(OBJECTS)
	$(Q)$(LINKER) $(strip $(LFLAGS)) -o $@ $(OBJECTS)
	@ln -sf $(notdir $(TARGET)) $(BIN_DIR)/libnetdkit.so

$(OBJECT_DIR)%.o:%.cpp
	$(Q)$(CPP) $(strip $(CPPALL_CFLAGS)) -c $< -o $@

$(OBJECT_DIR)%.o:%.c
	$(Q)$(CC) $(strip $(CALL_CFLAGS)) -c $< -o $@

checkdir:
	@if ! [ -d "$(BIN_DIR)" ]; then \
		mkdir $(BIN_DIR) ; \
		fi
	@if ! [ -d "$(OBJECT_DIR)" ]; then \
		mkdir $(OBJECT_DIR); \
		fi
clean:
	rm -f $(OBJECTS)
cleanall: clean
	rm -f $(TARGET)

.PHONY: all clean cleanall checkdir
