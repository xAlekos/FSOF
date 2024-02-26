/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPLv2.
  See the file COPYING.
*/

/** @file
 *
 * minimal example filesystem using high-level API
 *
 * Compile with:
 *
 *     gcc -Wall hello.c `pkg-config fuse3 --cflags --libs` -o hello
 *
 * ## Source code ##
 * \include hello.c
 */


#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>

struct file {
    char name[50];
    char content[50];
};

struct file nodi_file[50];

void InitFS(struct file* nodi_file){
	char nomino[50];
    for(int i = 0; i<50; i++){
		snprintf(nomino,50,"Petrus_%d",i);
        strcpy(nodi_file[i].content,"Jhon K. Bridge");
        strcpy(nodi_file[i].name,nomino);

    }
	printf("Initializing File System\n");
}

uint8_t FsIsInDir(const char* path, uint8_t* address){
	for(int i = 0 ; i<50;i++){
		if(strcmp(nodi_file[i].name, path + 1) == 0){
			*address = i;
			return 1;
		}
	}
	return -ENOENT;
}



static void *hello_init(struct fuse_conn_info *conn,
			struct fuse_config *cfg)
{
	(void) conn;
	cfg->kernel_cache = 1;
	return NULL;
	printf("Init Connection to Kernel Module\n");
}

static int hello_getattr(const char *path, struct stat *stbuf,
			 struct fuse_file_info *fi)
{
	(void) fi;
	int res = 0;
	uint8_t address;
	printf("Get attr of %s\n",path);
	memset(stbuf, 0, sizeof(struct stat));

	if(strcmp(path,"/") == 0){
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return res;
	}

	if(FsIsInDir(path,&address) == 1){
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(nodi_file[address].content);
	}
	else
		return -ENOENT;
		
	return res;
}

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi,
			 enum fuse_readdir_flags flags)
{
	(void) offset;
	(void) fi;
	(void) flags;
	printf("Readdir %s\n",path);
	if (strcmp(path, "/") != 0)
		return -ENOENT;


	filler(buf, ".", NULL, 0, 0);
	filler(buf, "..", NULL, 0, 0);

	for(int i = 0; i < 50; i++){
		filler(buf,nodi_file[i].name,NULL,0,0);
	}

	return 0;
}

static int hello_open(const char *path, struct fuse_file_info *fi)
{
	uint8_t address;
	printf("Open file at %s\n",path);
	if (!FsIsInDir(path,&address))
		return -ENOENT;

	if ((fi->flags & O_ACCMODE) != O_RDONLY)
		return -EACCES;

	return 0;
}

static int hello_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	size_t len;
	(void) fi;
	uint8_t address;
	printf("Reading file at %s\n",path);
	if(!FsIsInDir(path,&address))
		return -ENOENT;

	len = strlen(nodi_file[address].content);
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, nodi_file[address].content + offset, size);
	} else
		size = 0;

	return size;
}

static const struct fuse_operations hello_oper = {
	.init           = hello_init,
	.getattr	= hello_getattr,
	.readdir	= hello_readdir,
	.open		= hello_open,
	.read		= hello_read,
};

int main(int argc, char *argv[])
{
	InitFS(nodi_file);
	int ret;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	ret = fuse_main(args.argc, args.argv, &hello_oper, NULL);
	fuse_opt_free_args(&args);
	return ret;
}
