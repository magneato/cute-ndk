# -*- coding: utf-8 -*-

"""
summery:  
    auto publish tools
author: 
    cui shaowei
"""

import os
import sys
import shutil
from xml.dom import minidom
from time import time, strftime, localtime, sleep

log_fd = None

LOG_ERROR = 1
LOG_RINFO = 2

LOG_LEVEL = ["", "LOG_ERROR", "LOG_RINFO"]
def log(log_level, msg):
    global log_fd
    if not log_fd:
        log_fd = open("publish.log", "a")
    now = localtime(time())
    ts = strftime("%Y-%m-%d %H:%M:%S", now)
    s = "[" + ts + "]" + "<" + LOG_LEVEL[log_level] + "> - " + msg + os.linesep
    log_fd.write(s)

def remove_dir(dir):
    # clear dir
    dir_list = []
    for r, dirs, files in os.walk(dir):
        for file in files:
            os.unlink(os.path.join(r, file))
        dir_list.append(r)
    for dir in dir_list:
        if os.path.exists(dir):
            try: os.removedirs(dir)
            except: pass
##
class PackageError(Exception):
    pass

class Package(object):
    """package object
    """
    def __init__(self):
        self.attrib = {
                    "name" : "",
           "compress_type" : "",
                 "version" : "",
        }
        self.pub_svr = {
                "type" : "",
                "host" : "",
                "path" : "",
                "user" : "",
                "pswd" : "",
                }
        self.projects = []
        self.change_log = u""

    def __str__(self):
        return str(self.__dict__)

    def build(self):
        tmp_dir = "w23er45ty6" 
        try:
            try:
                if os.path.exists(tmp_dir):
                    remove_dir(tmp_dir)
                os.mkdir(tmp_dir)
            except OSError, desc:
                log(LOG_ERROR, "mkdir or clear dir failed! [%s]" % desc)
                raise ProjectError

            log(LOG_RINFO, "build package [%s] ...." % self.attrib['name'])

            abs_dist_dir = os.path.abspath(tmp_dir)

            # 1. build projects
            self.build_projects(abs_dist_dir)

            # 2. build package
            package_name = os.path.join(abs_dist_dir, "%s-v_%s.%d-%s.%s" % \
                    (self.attrib['name'], \
                    self.attrib['version'], \
                    self.build_counter(), \
                    strftime("%Y%m%d", localtime(time())), \
                    self.attrib['compress_type']))
            self.build_package(abs_dist_dir, package_name)

            # 4. create change log file
            changelog = os.path.join(tmp_dir, "%s-v_%s.%d-%s.%s" % ("ChangeLog", \
                    self.attrib['version'], \
                    self.build_counter(), \
                    strftime("%Y%m%d", localtime(time())), \
                    "txt"))
            open(changelog, "wt").write(self.change_log.encode('utf-8'))

            # 5. distribute package
            t = self.pub_svr['host'].split(":")
            host, port = t[0], 0
            if len(t) == 2: port = int(t[1])

            try:
                if self.pub_svr["type"].lower() == u'ftp':
                    if port == 0: port = 21
                    self.ftp_upload(host, port, [package_name, changelog])
                else:
                    log(LOG_ERROR, "not support '%s' upload type" % \
                            self.pub_svr["type"])
                    return None
            except Exception, des:
                log(LOG_ERROR, "upload package to [%s:%d] failed! [%s]" % \
                        (host, port, des))
                return None
            # 6. copy package to publish-dir
            if not os.path.exists(self.attrib['name']):
                try: os.makedirs(self.attrib['name'])
                except: 
                    log(LOG_ERROR, "make dir [%s] failed" % self.attrib['name'])
                    return None
            log(LOG_RINFO, "upload package [%s] to [%s:%d] successfully!" % \
                    (package_name, host, port))
            pname = os.path.split(package_name)[1]
            shutil.copyfile(package_name, os.path.join(self.attrib['name'], pname))
            pname = os.path.split(changelog)[1]
            shutil.copyfile(changelog, os.path.join(self.attrib['name'], pname))
            self.reset_build_counter()
        finally:
            remove_dir(tmp_dir)
        ##
    def build_package(self, abs_dist_dir, dist_name):
        # 1. packet
        old_dir = os.getcwd()
        try:
            try: os.chdir(abs_dist_dir)
            except OSError, desc:
                log(LOG_ERROR, "change dir [%s] failed [desc = %s]" % \
                        (abs_dist_dir, desc))
                raise PackageError
            if self.attrib['compress_type'].lower() in (u"tar.gz", u"tar"):
                try:
                    import tarfile
                    mode = "w"
                    if self.attrib['compress_type'] == u"tar.gz":
                        mode = "w|gz"
                    tf = tarfile.open(str(dist_name), str(mode))
                    try:
                        for r, dirs, files in os.walk("."):
                            for file in files:
                                if file != os.path.split(dist_name)[1]:
                                    tf.add(os.path.join(r, file))
                    finally:
                        tf.close()
                except Exception, des:
                    log(LOG_ERROR, "compress files failed! [%s]" % des)
                    raise PackageError
            elif self.attrib['compress_type'].lower() == u'zip':
                try:
                    import zipfile
                    zf = zipfile.ZipFile(str(dist_name), mode = "w")
                    try:
                        for r, dirs, files in os.walk("."):
                            for file in files:
                                if file != os.path.split(dist_name)[1]:
                                    zf.write(os.path.join(r, file))
                    finally:
                        zf.close()
                except Exception, des:
                    log(LOG_ERROR, "compress files failed! [%s]" % des)
                    raise PackageError
            else:
                log(LOG_ERROR, "not support [%s] compress type" % \
                        self.attrib['compress_type'])
                raise PackageError
        finally:
            os.chdir(old_dir)
        
    def build_projects(self, abs_dist_dir):
        for proj in self.projects:
            log(LOG_RINFO, "build project [%s] ...." % proj.attrib['name'])

            old_path = os.getcwd()
            proj.cd_workspace()
            try: proj.build(abs_dist_dir)
            except: pass
            finally: os.chdir(old_path)

    def ftp_upload(self, host, port, files):
        import ftplib
        ftp = ftplib.FTP()
        try: ftp.connect(host, port)
        except:
            log(LOG_ERROR, "connect ftp host [%s:%d] failed" % (host, port))
            raise PackageError
        try:
            ftp.login(self.pub_svr["user"], self.pub_svr["pswd"])
        except:
            log(LOG_ERROR, "login ftp server [%s:%d] failed" % (host, port))
            raise PackageError
        #
        paths = self.pub_svr["path"].split(os.path.sep)
        for p in paths:
            try: ftp.cwd(p)
            except:
                try:
                    ftp.mkd(p)
                    ftp.cwd(p)
                except: raise PackageError
        try:
            for file in files:
                ftp.storbinary("STOR " + os.path.split(file)[1] + "\r\n", 
                        open(file, "rb"))
        except: raise PackageError

        try: ftp.quit()    # sometime vsftpd response '500 OPS'
        except: pass

    def changelog(self):
        self.change_log = self.attrib['name'].upper() + " " + "CHANGES" + os.linesep
        self.change_log += "=" * 28 + os.linesep * 2
        for proj in self.projects:
            self.change_log += "%s version %s" % (proj.attrib['name'], \
                    proj.attrib['vcs'] + "-" + proj.version()) + os.linesep
        self.change_log += "-" * 28 + os.linesep * 2
        i = 1
        complete = True
        while True:
            header = "change %d> " % i
            line = raw_input(header)
            line = unicode(line, 'utf-8').strip()

            if len(line) == 0: continue
            if line == u"q": break

            if line[-1] == u'\\':
                self.change_log += line[:-1] + os.linesep
                complete = False
                continue
            self.change_log += (complete and header or " " * len(header)) + \
                    line + os.linesep * 2
            i += 1
            complete = True
        return 

    def build_counter(self):
        fname = self.attrib['name'] + u".counter"
        if os.path.exists(fname):
            bc = int(open(fname, "rt").read())
            return bc
        else:
            open(fname, "wt").write("1")
            return 1

    def reset_build_counter(self):
        bc = self.build_counter()
        fname = self.attrib['name'] + u".counter"
        open(fname, "wt").write(str(bc + 1))
        
##
class ProjectError(Exception):
    pass

class Project(object):
    """project object
    """
    def __init__(self):
        self.attrib = {
                    "name" : "",
                 "manager" : "",
                     "vcs" : "",
                "vcs_user" : "",
              "vcs_passwd" : "",
                    "path" : "",
                 "edition" : "",
                 }
        self.build_cmd = ""
        self.binary = []
        self.dependencies = []
        self.revision = ""

    def __str__(self):
        return str(self.__dict__)

    def build(self, dist_dir):
        """run in workspace
        """
        # 1. update source
        try:
            self.update()
        except:
            log(LOG_RINFO, "update the source of project [%s] failed!" % \
                    self.attrib['name'])
            raise ProjectError

        # 2. build project
        for bin in self.binary:
            if os.path.exists(bin):
                try: os.unlink(bin)
                except:
                    log(LOG_ERROR, "remove file [%s] failed! [desc = %s]" % bin)
                    raise ProjectError
        if '%(desc)s' in self.build_cmd:
            self.build_cmd = self.build_cmd % {'desc' : "%s-%s" % \
                    (self.attrib['vcs'], self.version())}
        try: os.system(self.build_cmd)
        except: pass
        #
        for bin in self.binary:
            if os.path.exists(bin):
                if os.path.islink(bin):
                    linkto = os.readlink(bin)
                    #os.symlink(linkto, os.path.join(dist_dir, bin))
                else:
                    shutil.copyfile(bin, os.path.join(dist_dir, \
                            os.path.split(bin)[1]))
            else:
                log(LOG_RINFO, "build project [%s] failed!" % self.attrib['name'])
                raise ProjectError
        log(LOG_RINFO, "build project [%s] successfully!" % self.attrib['name'])

        # 3. copy dependencies file to tmp distribute dir
        for depend in self.dependencies:
            depend.build(dist_dir)

    def cd_workspace(self):
        proj_root = os.getenv(self.attrib['path'])
        if not proj_root or len(proj_root) == 0:
            log(LOG_ERROR, "get env [%s] failed" % self.attrib['path'])
            raise ProjectError
        proj_root = os.path.abspath(proj_root)
        try: os.chdir(proj_root)
        except OSError, desc:
            log(LOG_ERROR, "change dir to [%s] failed! [desc = %s]" % \
                    (proj_root, desc))
            raise ProjectError

    def version(self):
        if self.attrib['vcs'].lower() == u'svn':
            return self.svn_version()
        return ""

    def svn_version(self):
        if len(self.revision) != 0:
            return self.revision
        old_path = os.getcwd()
        self.cd_workspace()
        try:
            self.svn_update()
            proj_root = os.getenv(self.attrib['path'])
            if not proj_root or len(proj_root) == 0:
                log(LOG_ERROR, "get env [%s] failed" % self.attrib['path'])
                return ""
            dom = minidom.parse(os.path.join(proj_root, ".svn/entries"))
            self.revision = \
                    dom.getElementsByTagName('entry')[0].getAttribute('revision')
        except: pass
        finally: os.chdir(old_path)

        return self.revision

    def update(self):
        if self.attrib['vcs'].lower() == u'svn':
            self.svn_update()
        else:
            log(LOG_ERROR, "not support [%s] vision control system!" % \
                    self.attrib['vcs'])
            raise ProjectError

    def svn_update(self):
        os.system("svn update --username=%(vcs_user)s --password=%(vcs_passwd)s \
                 --non-interactive -q" % self.attrib)
        
class Dependency(object):
    """dependency object
    """
    def __init__(self):
        self.attrib = {
                "name" : "",
                "path" : "",
                }
    def __str__(self):
        return str(self.__dict__)

    def build(self, dist_dir):
        shutil.copyfile(os.path.join(self.attrib['path'], self.attrib['name']), \
                os.path.join(dist_dir, self.attrib['name']))

def parse_config(cfg_name):
    """return list of package
    """
    def handle_package (xmldoc):
        """return package objects
        """
        packages = []
        nodes = xmldoc.getElementsByTagName("package")
        for node in nodes:
            p = Package()
            for k in p.attrib.keys():
                p.attrib[k] = node.getAttribute(k)
            p.pub_svr.update(handle_pub_svr(node.getElementsByTagName("pub_svr")[0], 
                p.pub_svr))
            p.projects.extend(handle_project(node.getElementsByTagName("project")))
            packages.append(p)
        return packages

    def handle_pub_svr(pub_svr, ps):
        """return publish server attribute
        """
        ret = {}
        for k in ps.keys():
            ret[k] = pub_svr.getAttribute(k)

        return ret
    
    def handle_project(proj_nodes):
        """return project list
        """
        projects = []
        for node in proj_nodes:
            project = Project()
            for k in project.attrib.keys():
                project.attrib[k] = node.getAttribute(k)

            project.build_cmd = \
                    node.getElementsByTagName("build_cmd")[0].childNodes[0].data
            project.binary    = \
                    node.getElementsByTagName("binary")[0].childNodes[0].data.split(':')
            depends = node.getElementsByTagName("dependencies")
            if len(depends) != 0:
                project.dependencies = handle_dependencies(depends[0])
            #
            projects.append(project)

        return projects
    
    def handle_dependencies(dep_nodes):
        """return depend list
        """
        depends = []
        for item in dep_nodes.getElementsByTagName("item"):
            depend = Dependency()
            for k in depend.attrib.keys():
                depend.attrib[k] = item.getAttribute(k)
            depends.append(depend)

        return depends

    #
    xmldoc = minidom.parse(cfg_name)
    package_list = handle_package(xmldoc)
    return package_list

def run():
    launch_log = "+" * 30 + "launched at [%s]" % \
           (strftime("%Y-%m-%d %H:%M:%S", localtime(time()))) \
           + "+" * 30
    log(LOG_RINFO, launch_log)

    packages = parse_config("config.xml")
    if len(packages) == 0:
        log(LOG_ERROR, "no packages")
        sys.exit(0)

    old_path = os.getcwd()
    for package in packages:
        try:
            package.changelog()
        except:
            import traceback
            for l in traceback.format_exc().splitlines():
                log(LOG_ERROR, l)
        finally:
            os.chdir(old_path)

    for package in packages:
        try:
            package.build()
        except:
            import traceback
            for l in traceback.format_exc().splitlines():
                log(LOG_ERROR, l)
        finally:
            os.chdir(old_path)

if __name__ == "__main__":
    run()

