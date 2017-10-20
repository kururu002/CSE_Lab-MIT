// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>



yfs_client::yfs_client(std::string extent_dst)
{
  ec = new extent_client(extent_dst);
  if (ec->put(1, "") != extent_protocol::OK)
      printf("error init root dir\n"); // XYB: init root dir
}


yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE)
    {
        printf("isfile: %lld is a file\n", inum);
        return true;
    }
    printf("isfile: %lld is not a file\n", inum);
    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 *
 * */
bool yfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_DIR)
    {
        printf("isdir: %lld is a dir\n", inum);
        return true;
    }
    printf("isdir: %lld is not a dir\n", inum);
    return false;
}

int yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}

#define EXT_RPC(xx)                                                \
    do                                                             \
    {                                                              \
        if ((xx) != extent_protocol::OK)                           \
        {                                                          \
            printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
            r = IOERR;                                             \
            goto release;                                          \
        }                                                          \
    } while (0)

// Only support set size of attr
int yfs_client::setattr(inum ino, size_t size)
{
    int r = OK;
    /*
     * your lab2 code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
    std::string buf;
    ec->get(ino, buf);
    buf.resize(size);
    ec->put(ino, buf);
    return r;
}

int yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;
    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    bool found = false;
    lookup(parent, name, found, ino_out);
    if (found)
        return EXIST;
    else
    {
        ec->create(extent_protocol::T_FILE, ino_out);
        std::string buf;
        ec->get(parent, buf);
        buf.append('/' + std::string(name) + '/' + filename(ino_out));
        ec->put(parent, buf);
    }
    return r;
}

int yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;
    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    bool found = false;
    lookup(parent, name, found, ino_out);
    if (found)
    {
        printf("FILE EXIST!");
        return EXIST;
    }

    else
    {
        ec->create(extent_protocol::T_DIR, ino_out);
        std::string buf;
        ec->get(parent, buf);
        buf.append('/' + std::string(name) + '/' + filename(ino_out));
        ec->put(parent, buf);
    }

    return r;
}

int yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;
    /*
     * your lab2 code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    found = false;
    if (!isdir(parent))
    {
        printf("NOT DIR!\n");
        return IOERR;
    }

    std::string buf;
    if (ec->get(parent, buf) != extent_protocol::OK)
    {
        printf("GET SERVER ERR!\n");
        return IOERR;
    }
    else
    {
        std::string filename;
        std::string inodenumber;
        uint32_t pos = 1; //include the '/'
        uint32_t len = buf.length();
        while (pos < len)
        {
            //get filename
            while (buf[pos] != '/')
            {
                filename += buf[pos];
                pos++;
            }

            pos++;
            // get inodenum

            while (buf[pos] != '/' && pos < len)
            {
                inodenumber += buf[pos];
                pos++;
            }

            // compare

            if (filename == std::string(name))
            {

                found = true;
                ino_out = n2i(inodenumber);
                return r;
            }
            //reset
            filename = inodenumber = "";
            //next file
            pos++;
        }
    }
    return NOENT;
}

int yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;
    /*
     * your lab2 code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    if (!isdir(dir))
    {
        printf("NOT DIR!\n");
        return IOERR;
    }

    std::string buf;
    if (ec->get(dir, buf) != extent_protocol::OK)
    {
        printf("GET SERVER ERR!\n");
        return IOERR;
    }

    dirent *entry = new dirent();
    std::string filename;
    std::string inodenumber;

    uint32_t pos = 1;
    uint32_t len = buf.length();
    while (pos < len)
    {
        //get filename
        while (buf[pos] != '/')
        {
            filename += buf[pos];
            pos++;
        }

        pos++;
        // get inodenum

        while (buf[pos] != '/' && pos < len)
        {
            inodenumber += buf[pos];
            pos++;
        }

        // compare

        entry->name = filename;
        entry->inum = n2i(inodenumber);
        list.push_back(*entry);
        //reset
        filename = inodenumber = "";
        //next file
        pos++;
    }

    delete entry;
    return r;
}

int yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;
    /*
     * your lab2 code goes here.
     * note: read using ec->get().
     */
    std::string buf;
    if (ec->get(ino, buf) != extent_protocol::OK)
    {
        printf("GET ERR!\n");
        return IOERR;
    }
    int bufsize = buf.size();
    if (bufsize <= off)
        data = "";
    else
    {
        data = buf.substr(off, size);
    }
    return r;
}

int yfs_client::write(inum ino, size_t size, off_t off, const char *data,
                      size_t &bytes_written)
{

    int r = OK;
    /*
         * your lab2 code goes here.
         * note: write using ec->put().
         * when off > length of original file, fill the holes with '\0'.
         */
    std::string buf;
    if (ec->get(ino, buf) != extent_protocol::OK)
    {
        printf("GET ERR!\n");
        return IOERR;
    }
    if (buf.size() < (int)off + (int)size)
    {
        buf.resize(off);
        buf.append(data, size);
    }
    else
    {
        for (int i = 0; i < size; i++)
        {
            buf[i + off] = data[i];
        }
    }
    bytes_written = size;
    ec->put(ino, buf);
    return r;
}

int yfs_client::unlink(inum parent, const char *name)
{
    int r = OK;
    /*
     * your lab2 code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */

    inum inodenum = 0;
    bool found = false;
    r = lookup(parent, name, found, inodenum);
    if (r == IOERR)
    {
        printf("ERR WITH LOOKUP!\n");
        return r;
    }
    if (!found)
    {
        printf("NAME NOT FOUND!\n");
        return NOENT;
    }
    else
    {
        std::string buf;
        if (ec->get(parent, buf) != extent_protocol::OK)
        {
            printf("GET ERR!\n");
            return IOERR;
        }
        ec->remove(inodenum);
        buf.erase(buf.find(name) - 1, strlen(name) + filename(inodenum).size() + 2);
        ec->put(parent, buf);
    }
    return r;
}

int yfs_client::symlink(const char *link, inum parent, const char *name, inum &ino_out)
{
    if (!isdir(parent))
    {
        return IOERR;
    }
    bool found;
    lookup(parent, name, found, ino_out);
    if (found)
    {
        return EXIST;
    }
    if (ec->create(extent_protocol::T_SYMLNK, ino_out) != extent_protocol::OK)
    {
        printf("CREATE ERR\n");
        return IOERR;
    }
    if (ec->put(ino_out, std::string(link)) != extent_protocol::OK)
    {
        printf("PUT ERR\n");
        return IOERR;
    }
    std::string buf;
    if (ec->get(parent, buf) != extent_protocol::OK)
    {
        printf("GET ERR\n");
        return IOERR;
    }
    buf = buf + '/' + std::string(name) + '/' + filename(ino_out);
    if (ec->put(parent, buf) != extent_protocol::OK)
    {
        printf("PUT ERR\n");
        return IOERR;
    }
    return OK;
}
bool yfs_client::issymlink(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        printf("GET ATTR ERR!\n");
        return false;
    }

    if (a.type == extent_protocol::T_SYMLNK)
    {
        printf("issymlink: %lld is a symlink\n", inum);
        return true;
    }
    printf("issymlink: %lld is not a symlink\n", inum);
    return false;
}

int yfs_client::readlink(inum ino, std::string &link)
{
    if (!issymlink(ino))
    {
        return IOERR;
    }
    if (ec->get(ino, link) != extent_protocol::OK)
    {
        printf("GET ERRt\n");
        return IOERR;
    }

    return OK;
}

int yfs_client::getsymlink(inum inum, symlinkinfo &sin)
{
    int r = OK;
    printf("getsymlink %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        r = IOERR;
        goto release;
    }
    sin.atime = a.atime;
    sin.mtime = a.mtime;
    sin.ctime = a.ctime;
    sin.size = a.size;
    printf("getsymlink %016llx -> sz %llu\n", inum, sin.size);
release:
    return r;
}
