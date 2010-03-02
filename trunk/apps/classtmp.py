# -*- coding: utf-8 -*-

import os, sys
import datetime

__author__ = "cuisw"
__author_mail__ = "shaovie@gmail.com"

nl = os.linesep 

def gen_temp(path, name):
    """
    """
    generate_cpp = True 
    if name[-2:] == '.h':
        generate_cpp = False
        file_h = name
    else:
        file_h = name + '.h'

    gen_header_1(path, file_h)
    if generate_cpp:
        gen_cpp_1(path, file_h[:-2] + '.cpp')

def gen_header_1(path, name):
    """
    """
    file = open(os.path.join(path, name), "wt")
    temp = """// -*- C++ -*-

{sep}
/**
 * Author   : {author} <{mail}>
 * Date     : {date}
 */
{sep}

#ifndef {cls_name_u}_H_
#define {cls_name_u}_H_

/**
 * @class {cls_name_l}
 * 
 * @brief
 */
class {cls_name_l}
{{
public:
  {cls_name_l}()
  {{
  }}

  virtual ~{cls_name_l}()
  {{
  }}
}};

#endif // {cls_name_u}_H_

"""
    content = {}
    content['author'] = __author__
    content['mail']   = __author_mail__
    content['date']   = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
    content['sep']    = '//' + '=' * 72
    content['cls_name_u'] = name[:-2].upper()
    content['cls_name_l'] = name[:-2].lower()
    file.write(temp.format(**content))

    file.close()

def gen_cpp_1(path, name):
    """
    """
    file = open(os.path.join(path, name), "wt")
    content = {}
    content['header'] = name[:-4] + ".h"
    temp="""#include "{header}"

"""
    file.write(temp.format(**content))
    file.close()

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print "no args"
        sys.exit(1)
    for f in sys.argv[2:]:
        gen_temp(sys.argv[1], f)
