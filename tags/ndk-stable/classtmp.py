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

def gen_header(path, name):
    """
    """
    file = open(os.path.join(path, name), "wt")
    file.write('// -*- C++ -*-' + nl * 2)
    file.write('//' + '=' * 72 + nl)
    file.write('/**' + nl)
    file.write(' * Author   : ' + __author__ + ' ' + '<' + __author_mail__ + '>' + nl)
    file.write(' * Date     : ' + datetime.datetime.now().strftime("%Y-%m-%d %H:%M") + nl)
    file.write(' */' + nl)
    file.write('//' + '=' * 72 + nl * 2)

    mac = 'NDK_' + name.upper() + '_'
    mac = mac.replace('.', '_')

    file.write('#ifndef ' + mac + nl)
    file.write('#define ' + mac + nl * 2)

    file.write('namespace ndk' + nl + '{' + nl)
    # summury
    file.write(' ' * 2 + '/**' + nl)
    file.write(' ' * 2 + ' * ' + '@class ' + name[:-2] + nl)
    file.write(' ' * 2 + ' * ' + nl)
    file.write(' ' * 2 + ' * ' + '@brief ' + nl)
    file.write(' ' * 2 + ' */' + nl)

    file.write(' ' * 2 + 'class ' + name[:-2] + nl)
    file.write(' ' * 2 + '{' + nl)
    file.write(' ' * 2 + 'public:' + nl)
    file.write(' ' * 4 + name[:-2] + '()' + nl)
    file.write(' ' * 4 + '{' + nl)
    file.write(' ' * 4 + '}' + nl * 2)
    file.write(' ' * 4 + '~' + name[:-2] + '()' + nl)
    file.write(' ' * 4 + '{' + nl)
    file.write(' ' * 4 + '}' + nl)
    file.write(' ' * 2 +  '};' + nl)
    file.write('}' + ' // namespace ndk' + nl * 2)
    file.write('#endif // ' + mac + nl)
    file.write(nl)

    file.close()
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

#ifndef NDK_{cls_name_u}_H_
#define NDK_{cls_name_u}_H_

namespace ndk
{{
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
}} // namespace ndk

#endif // NDK_{cls_name_u}_H_

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

def gen_cpp(path, name):
    """
    """
    file = open(os.path.join(path, name), "wt")
    file.write('#include "ndk/' + name.replace('.cpp', '.h') + '"' + nl * 2)
    file.write('namespace ndk' + nl)
    file.write('{' + nl * 2)

    cls_name = name[:-4]
    file.write(cls_name + '::' + cls_name + '()' + nl)
    file.write('{' + nl)
    file.write('}' + nl * 2)
    file.write(cls_name + '::~' + cls_name + '()' + nl)
    file.write('{' + nl)
    file.write('}' + nl * 2)
    
    file.write('}' + ' // namespace ndk' + nl)

    file.write(nl)
    file.close()

def gen_cpp_1(path, name):
    """
    """
    file = open(os.path.join(path, name), "wt")
    content = {}
    content['header'] = name[:-4] + ".h"
    temp="""#include "ndk/{header}"

namespace ndk
{{
}} // namespace ndk

"""
    file.write(temp.format(**content))
    file.close()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print "no args"
        sys.exit(1)
    for f in sys.argv[1:]:
        gen_temp('ndk', f)
