/*
Copyright (c) 2012-2014 The SSDB Authors. All rights reserved.
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
*/
#ifndef UTIL_FILE_H_
#define UTIL_FILE_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <vector>


typedef struct st_folder
{
    std::string                    dir_name;
    std::vector<std::string>       file_paths;
} FOLDER;


typedef std::vector<FOLDER> FOLDERS;
typedef std::vector<FOLDER>::iterator FOLDER_ITER;

static inline
bool file_exists(const std::string &filename){
	struct stat st;
	return stat(filename.c_str(), &st) == 0;
}

static inline
bool is_dir(const std::string &filename){
	struct stat st;
	if(stat(filename.c_str(), &st) == -1){
		return false;
	}
	return (bool)S_ISDIR(st.st_mode);
}

static inline
bool is_file(const std::string &filename){
	struct stat st;
	if(stat(filename.c_str(), &st) == -1){
		return false;
	}
	return (bool)S_ISREG(st.st_mode);
}

static inline
time_t last_modify_time(const std::string &filename) {
	struct stat st;
	if (stat(filename.c_str(), &st) != 0) {
		return 0;
	}
	return st.st_mtime;
}

static inline
int create_file(const std::string &filename) {
	int fd = open(filename.c_str(), O_CREAT|O_EXCL);
	if (fd < 0) {
		return fd;
	}
	close(fd);
	return 0;
}

static inline
uint64_t filesize(int fd) {
	assert (fd >= 0);

	struct stat st;
	int ret = fstat(fd, &st);
	if (ret != 0) {
		return 0;
	}
	return st.st_size;
}

// return number of bytes read
static inline
int file_get_contents(const std::string &filename, std::string *content){
	char buf[8192];
	FILE *fp = fopen(filename.c_str(), "rb");
	if(!fp){
		return -1;
	}
	int ret = 0;
	while(!feof(fp) && !ferror(fp)){
		int n = fread(buf, 1, sizeof(buf), fp);
		if(n > 0){
			ret += n;
			content->append(buf, n);
		}
	}
	fclose(fp);
	return ret;
}

// return number of bytes written
static inline
int file_put_contents(const std::string &filename, const std::string &content){
	FILE *fp = fopen(filename.c_str(), "wb");
	if(!fp){
		return -1;
	}
	int ret = fwrite(content.data(), 1, content.size(), fp);
	fclose(fp);
	return ret == (int)content.size()? ret : -1;
}


static void get_all_files(const std::string& path, FOLDERS& ret, const char * dir_name = NULL)
{
    std::vector<std::string> vct;
    DIR* dir = opendir(path.c_str());
    if (dir == NULL)
    {
        int err = errno;
        //LOG_WARN(("directory [%s] open failed: [%d:%s]!", path.c_str(), err, strerror(err)));
        return;
    }

    FOLDER f;

    if(dir_name)
    {
        f.dir_name = dir_name;
    }

    struct dirent* ptr;
    while ((ptr = readdir(dir)) != NULL)
    {
        if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0)
        {
            continue;
        }

        switch (ptr->d_type)
        {
            case DT_REG:
                {
                    std::string tmp = path + "/" + ptr->d_name;
                    f.file_paths.push_back(tmp);
                }
                break;
            case DT_DIR:
                {
                    std::string tmp = path + "/" + ptr->d_name;
                    std::string relative_path;
                    if(f.dir_name == "")
                    {
                        relative_path = ptr->d_name;
                    }
                    else
                    {
                        relative_path = f.dir_name + "/" + ptr->d_name;
                    }
                    get_all_files(tmp, ret, relative_path.c_str());
                }
                break;
            default:
                break;
        }
    }

    ret.push_back(f);
    closedir(dir);
}

static
int copy_file(const std::string& dst, const std::string& src)
{
    int fd_src = open(src.c_str(), O_RDONLY);
    if (fd_src < 0)
    {
        int err = errno;
        return err;
    }

    int fd_dest = open(dst.c_str(), O_RDWR | O_CREAT, 0664);
    if (fd_dest < 0)
    {
        int err = errno;
        close(fd_src);
        return err;
    }

    int pipe_fd[2];
    int status = pipe(pipe_fd);
    if (status < 0)
    {
        int err = errno;
        close(fd_src);
        close(fd_dest);
        return err;
    }

    int _buffer_size = 1;
    while (1)
    {
        status = splice(fd_src, NULL, pipe_fd[1], NULL, _buffer_size * 1024 * 1024, SPLICE_F_MORE | SPLICE_F_MOVE);
        if (status > 0)
        {
            status = splice(pipe_fd[0], NULL, fd_dest, NULL, _buffer_size * 1024 * 1024, SPLICE_F_MORE | SPLICE_F_MOVE);
            if (status <= 0)
            {
                if (status < 0)
                {
                    int err = errno;
                }
                break;
            }
        }
        else
        {
            if (status < 0)
            {
                int err = errno;
            }
            break;
        }
    }

    close(fd_src);
    close(fd_dest);
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    return 0;
}
/*
int remove_file(const std::string &filename) {
	return unlink(filename.c_str());
}*/

#endif
