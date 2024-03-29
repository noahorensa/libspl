/*
 * Copyright (c) 2021-2023 Noah Orensa.
 * Licensed under the MIT license. See LICENSE file in the project root for details.
*/

#include <file.h>

using namespace spl;

#include <glob.h>
#include <cstring>
#include <base64.h>

Path File::uniquePath(const char *dir, const char *prefix) {
    using namespace std::chrono;

    Path p(dir);
    std::string pre(prefix);
    Path unique;

    do {
        uint64_t now = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
        size_t len;
        char *tmp = Base64::encode(&now, sizeof(now), len);
        char *unsafe = nullptr;
        while ((unsafe = strchr(tmp, '/')) != nullptr) *unsafe = '-';
        unique = p.append((pre + tmp).c_str());
        free(tmp);
    } while (exists(unique));

    return unique;
}

void File::mkdirs(const char *path) {
    char *tmp = strdup(path);
    size_t len = strlen(tmp);

    if(tmp[len - 1] == Path::SEPARATOR) {
        tmp[len - 1] = '\0';
    }

    bool noExist = false;
    for(char *p = tmp + 1; *p != '\0'; ++p) {
        if(*p == Path::SEPARATOR) {
            *p = '\0';
            if (noExist || ! exists(tmp)) {
                noExist = true;
                try { mkdir(tmp); }
                catch (Error &e) { free(tmp); throw e; }
            }
            *p = Path::SEPARATOR;
        }
    }
    if (! exists(tmp)) {
        try { mkdir(tmp); }
        catch (Error &e) { free(tmp); throw e; }
    }

    free(tmp);
}

void File::rmdirs(const Path &path) {
    PathInfo pi(path);

    if (pi.isDir()) {
        list(path.append("*")).foreach([] (const Path &path) {
            rmdirs(path);
        });
    }

    remove(path);
}

List<Path> File::list(const char *pattern) {
    List<Path> children;

    glob_t globbuf; globbuf.gl_offs = 0;
    if (glob(pattern, GLOB_DOOFFS, NULL, &globbuf) == 0) {
        for (size_t i = 0; i < globbuf.gl_pathc; ++i) {
            children.append(globbuf.gl_pathv[i]);
        }
    }
    globfree(&globbuf);

    return children;
}

size_t File::read(void *buf, size_t len) {
    if (_fd == -1) open();
    size_t readBytes = 0;
    while (len > 0) {
        ssize_t x = ::read(_fd, (uint8_t *) buf + readBytes, len);
        if (x == -1) throw ErrnoRuntimeError();
        if (x == 0) break;
        readBytes += x;
        len -= x;
    }
    return readBytes;
}

size_t File::read(off_t offset, void *buf, size_t len) {
    if (_fd == -1) open();
    size_t readBytes = 0;
    while (len > 0) {
        ssize_t x = ::pread(_fd, (uint8_t *) buf + readBytes, len, offset + (off_t) readBytes);
        if (x == -1) throw ErrnoRuntimeError();
        if (x == 0) break;
        readBytes += x;
        len -= x;
    }
    return readBytes;
}

void File::write(const void *buf, size_t len) {
    if (_fd == -1) open();
    size_t writtenBytes = 0;
    while (len > 0) {
        ssize_t x = ::write(_fd, (uint8_t *) buf + writtenBytes, len);
        if (x == -1) throw ErrnoRuntimeError();
        writtenBytes += x;
        len -= x;
    }
    _info.clear();
}

void File::write(off_t offset, const void *buf, size_t len) {
    if (_fd == -1) open();
    size_t writtenBytes = 0;
    while (len > 0) {
        ssize_t x = ::pwrite(_fd, (uint8_t *) buf + writtenBytes, len, offset + (off_t) writtenBytes);
        if (x == -1) throw ErrnoRuntimeError();
        writtenBytes += x;
        len -= x;
    }
    _info.clear();
}

File & File::allocate(off_t offset, off_t len) {
    if (_fd == -1) open();
    if (fallocate(_fd, 0, offset, len) != 0) {
        throw ErrnoRuntimeError();
    }
    _info.clear();
    return *this;
}

File & File::deallocate(off_t offset, off_t len) {
    if (_fd == -1) open();
    if (fallocate(_fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, offset, len) != 0) {
        throw ErrnoRuntimeError();
    }
    _info.clear();
    return *this;
}

File & File::insert(off_t offset, off_t len) {
    if (_fd == -1) open();
    if (fallocate(_fd, FALLOC_FL_INSERT_RANGE, offset, len) != 0) {
        throw ErrnoRuntimeError();
    }
    _info.clear();
    return *this;
}

File & File::collapse(off_t offset, off_t len) {
    if (_fd == -1) open();
    if (fallocate(_fd, FALLOC_FL_COLLAPSE_RANGE, offset, len) != 0) {
        throw ErrnoRuntimeError();
    }
    _info.clear();
    return *this;
}

MemoryMapping File::map(off_t offset, size_t len, bool writeable) {
    if (_fd == -1) open();

    int flags = MAP_NONBLOCK | MAP_NORESERVE;
    if (writeable) flags |= MAP_SHARED;

    int prot = PROT_READ;
    if (writeable) prot |= PROT_WRITE;

    void *ptr = mmap(
        nullptr,
        len,
        prot,
        flags,
        _fd,
        offset
    );

    if (ptr == MAP_FAILED) {
        throw ErrnoRuntimeError();
    }

    return MemoryMapping(ptr, len);
}

flock _make_flock(short mode, off_t offset, off_t len) {
    flock l;
    l.l_type = mode;

    if (offset == (off_t) -1) {
        l.l_whence = SEEK_CUR;
        l.l_start = (off_t) 0;
    }
    else if (offset == (off_t) -2) {
        l.l_whence = SEEK_END;
        l.l_start = (off_t) 0;
    }
    else {
        l.l_whence = SEEK_SET;
        l.l_start = offset;
    }

    l.l_len = len;
    l.l_pid = 0;

    return l;
}

bool File::lock(short mode, off_t offset, off_t len, bool blocking) {
    if (_fd == -1) open();

    flock l = _make_flock(mode, offset, len);

    if (blocking) {
        while (true) {
            if (fcntl(_fd, F_OFD_SETLKW, &l) == -1) {
                if (errno == EINTR) continue;
                throw ErrnoRuntimeError();
            }
            return true;
        }
    }
    else {
        if (fcntl(_fd, F_OFD_SETLK, &l) == -1) {
            if (errno == EAGAIN) return false;
            throw ErrnoRuntimeError();
        }
        return true;
    }
}

bool File::lock_test(short mode, off_t offset, off_t len) {
    if (mode == LOCK_UNLOCK) {
        throw InvalidArgument("Cannot test for LOCK_UNLOCK mode");
    }

    if (_fd == -1) open();

    flock l = _make_flock(mode, offset, len);

    if (fcntl(_fd, F_OFD_GETLK, &l) == -1) throw ErrnoRuntimeError();
    return l.l_type == F_UNLCK;
}
