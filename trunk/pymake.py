# -*- coding: utf-8 -*-

import os, sys
try:
    from termcolor import colored
except:
    print("import 'termcolor' failed!")
    colored = lambda x, y : x

__logo__ =  \
r"""###########################################################################
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

"""
## config dirs >> scan source files >> 
## global vars

## functions

def generate_makefile():
    # 1. config project dirs
    proj_root = 'ndk';
    proj_dirs = config_proj_dirs(proj_root)

    # 2. scan src files
    file_type  = ('.c', '.cpp', '.cxx', '.cc')
    proj_src_files = scan_src_files(file_type, proj_dirs)

    # 3. build makefile
    build_makefile(src_files = proj_src_files)

def config_proj_dirs(proj_root):
    """
    Return project directory struct.
    """
    proj_dirs = []
    for r, dirs, files in os.walk(proj_root):
        if r[0] != '.' or r == os.path.curdir: proj_dirs.append(r)
    return proj_dirs

def scan_src_files(file_type, proj_dirs):
    """
    Scan <src_files> from project directory struct.
    """
    src_files = {}
    for proj_dir in proj_dirs:
        for r, dirs, files in os.walk(proj_dir):
            l = [file for file in files \
                if os.path.splitext(file)[-1] in file_type]
            if len(l) > 0: src_files[r] = l
    return src_files

def build_makefile(**kargs):
    if os.path.exists('makefile'):
        while True:
            input = raw_input(colored("Do you want overwrite `makefile'? [y/n] ",
              'yellow'))
            if input.lower() not in ('y', 'yes', 'n', 'no'):
                print colored('Please input [y/n] !', 'yellow')
                continue
            if input.lower() in ('n', 'no'):
                sys.exit(1)
            break
    fm = open('makefile', 'wt')
    fm.write(__logo__)
    content = build_c_cpp_makefile(**kargs)
    fm.write(content)
    fm.close()

def build_c_cpp_makefile(**kargs):
    """
    c/cpp makefile template
    """
    include_dirs = []
    dir = raw_input(colored('[c/c++] project include dirs: ', 'green'))
    include_dirs = ['-I ' + d for d in dir.split(' ') if (len) > 0]

    content = ""
    ## 1. vars
    lines = {}
    lines['CC']  = 'gcc'
    lines['CPP'] = 'g++'
    lines['LINKER'] = 'g++'
    lines['MAKE'] = 'make'

    lines['BIN_DIR'] = './bin'
    lines['LIBS'] = '-lpthread'
    lines['LIB_DIRS'] = ''
    lines['LFLAGS']  = '-Wl -shared -fPIC'
    lines['MACROS']  = '-D_REENTRANT -D__USE_POSIX -DNDK_RTLOG#-DNDK_STRACE'
    lines['C_CFLAGS'] = '-Wall -W -Wpointer-arith -pipe -fPIC'
    lines['CPP_CFLAGS'] = '-Wall -W -Wpointer-arith -pipe -fPIC'
    lines['OBJECT_DIR'] = './.obj/'
    lines['INCLUDE_DIRS'] = ' '.join(include_dirs)
    lines['OPTIM_FLAG'] = '-O2'
    lines['VPATH'] = ''
    for dir in kargs['src_files'].keys():
        lines['VPATH'] += dir + ' '
    lines['TARGET'] = '$(BIN_DIR)/libnetdkit.so.1.0'
    for k, v in lines.items():
        content += '%38s =%s%s' % (k, len(v) > 0 and ' ' \
            + v.strip() or v, os.linesep)

    ## 2. source files
    lines = []
    lines.append('%38s = %s' % ('CPPFILES', '\\'))
    [lines.append('%41s%s  %s' % (' ', file, '\\')) \
        for dir, files in kargs['src_files'].items() \
        for file in files \
        if os.path.splitext(file)[-1].lower() in ('.cpp', '.cxx', '.cc')]
    lines[-1].replace('\\', ' ') 
    lines[-1] = lines[-1].replace('\\', ' ') 
    for l in lines:
        content += '%s%s' % (l, os.linesep)
    content += os.linesep

    lines = []
    lines.append('%38s = %s' % ('CFILES', '\\'))
    [lines.append('%41s%s  %s' % (' ', file, '\\')) \
        for dir, files in kargs['src_files'].items() \
        for file in files \
        if os.path.splitext(file)[-1].lower() in ('.c')]
    lines[-1] = lines[-1].replace('\\', ' ') 
    lines.append("")
    for l in lines:
        content += '%s%s' % (l, os.linesep)

    ## 3. options
    lines = []
    lines.append("# To use 'make debug=0' build release edition.")
    lines.append('ifdef debug')
    lines.append('\tifeq ("$(origin debug)", "command line")')
    lines.append('\t\tIS_DEBUG = $(debug)')
    lines.append('\tendif')
    lines.append('else')
    lines.append('\tIS_DEBUG = 1')
    lines.append('endif')
    lines.append('ifndef IS_DEBUG')
    lines.append('\tIS_DEBUG = 1')
    lines.append('endif')
    lines.append('ifeq ($(IS_DEBUG), 1)')
    lines.append('\tOPTIM_FLAG += -g3')
    lines.append('else')
    lines.append('\tMACROS += -DNDEBUG')
    lines.append('endif')
    lines.append("")
    lines.append("# To use 'make quiet=1' all the build command will be hidden.")
    lines.append("# To use 'make quiet=0' all the build command will be displayed.")
    lines.append('ifdef quiet')
    lines.append('\tifeq ("$(origin quiet)", "command line")')
    lines.append('\t\tQUIET = $(quiet)')
    lines.append('\tendif')
    lines.append('endif')
    lines.append('ifeq ($(QUIET), 1)')
    lines.append('\tQ = @')
    lines.append('else')
    lines.append('\tQ =')
    lines.append('endif')
    lines.append("")
    for l in lines:
        content += '%s%s' % (l, os.linesep)

    ## 4. objects 
    lines = []
    lines.append(r"OBJECTS := $(addprefix $(OBJECT_DIR), $(notdir $(CPPFILES:%.cpp=%.o)))")
    lines.append(r"OBJECTS += $(addprefix $(OBJECT_DIR), $(notdir $(CFILES:%.c=%.o)))")
    lines.append("")
    lines.append(r"CALL_CFLAGS := $(C_CFLAGS) $(INCLUDE_DIRS) $(MACROS) $(OPTIM_FLAG)")
    lines.append(r"CPPALL_CFLAGS := $(CPP_CFLAGS) $(INCLUDE_DIRS) $(MACROS) $(OPTIM_FLAG)")
    lines.append(r"LFLAGS += $(OPTIM_FLAG) $(LIB_DIRS) $(LIBS)")
    lines.append('')

    ## 5. 推导规则
    lines.append(r"all: checkdir $(TARGET)")
    lines.append('')
    lines.append(r"$(TARGET): $(OBJECTS)")
    lines.append("\t$(Q)$(LINKER) -o $@ $(OBJECTS) $(strip $(LFLAGS))")
    lines.append("\t@ln -sf $(notdir $(TARGET)) $(BIN_DIR)/libnetdkit.so")
    lines.append('')
    lines.append(r"$(OBJECT_DIR)%.o:%.cpp")
    lines.append("\t$(Q)$(CPP) $(strip $(CPPALL_CFLAGS)) -c $< -o $@")
    lines.append('')
    lines.append(r"$(OBJECT_DIR)%.o:%.c")
    lines.append("\t$(Q)$(CC) $(strip $(CALL_CFLAGS)) -c $< -o $@")
    lines.append('')

    ## 6. phony
    lines.append("checkdir:")
    #lines.append('\t@echo "`tput setaf 2`Check dependent dirs ..."`tput op`')
    lines.append('\t@if ! [ -d "$(BIN_DIR)" ]; then \\')
    lines.append("\t\tmkdir $(BIN_DIR) ; \\")
    lines.append("\t\tfi")
    lines.append('\t@if ! [ -d "$(OBJECT_DIR)" ]; then \\')
    lines.append('\t\tmkdir $(OBJECT_DIR); \\')
    lines.append('\t\tfi')
    lines.append("clean:")
    lines.append("\trm -f $(OBJECTS)")
    lines.append("cleanall: clean")
    lines.append("\trm -f $(TARGET)")
    lines.append('')
    lines.append(".PHONY: all clean cleanall checkdir")
    for l in lines:
        content += '%s%s' % (l, os.linesep)

    return content

def build_python_makefile():
    """
    """

if __name__ == "__main__":
    generate_makefile()
