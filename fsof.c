#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stddef.h>
#include <assert.h>

#include "filesystem.h"



static void *myfs_init(struct fuse_conn_info *conn,
			struct fuse_config *cfg)
{
	(void) conn;
	cfg->kernel_cache = 1;
	return NULL;
	printf("Init Connection to Kernel Module\n");
}

static int myfs_getattr(const char *path, struct stat *stbuf,
			 struct fuse_file_info *fi)
{
	(void) fi;
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));

	printf("Get attr of %s\n",path);

	if(strcmp(path, "/") == 0){
		stbuf->st_mode = S_IFDIR | 0444;
		stbuf->st_nlink = 2;
		return 0;
	}
	
	filenode_t* req_file = FileFromPath(path);


	if(req_file != NULL && req_file->check != 0){

		stbuf->st_mode = req_file->mode;
		stbuf->st_nlink = 1;

		if(req_file->content != NULL)
			stbuf->st_size = strlen(req_file->content);

		else
			stbuf->st_size = 0;
	}
	else
		return -ENOENT;

	return res;
}

static int myfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi,
			 enum fuse_readdir_flags flags)
{
	(void) offset;
	(void) fi;
	(void) flags;
	filenode_t* req_dir = FileFromPath(path);
	printf("Readdir %s\n",path);

	if (req_dir == NULL || req_dir->type != DIR)
		return -ENOENT;
	
	filler(buf, ".", NULL, 0, 0);
	filler(buf, "..", NULL, 0, 0);

	for(int i = 0; i < 50; i++){
		if(req_dir->dir_content[i].check != 0)
			filler(buf,req_dir->dir_content[i].name,NULL,0,0);
	}

	return 0;
}

static int myfs_open(const char *path, struct fuse_file_info *fi)
{

	filenode_t* req_file = FileFromPath(path);
	printf("Open file at %s\n",path);
	if (req_file == NULL)
		return -ENOENT;

	if ((fi->flags & O_ACCMODE) != O_RDONLY)
		return -EACCES;

	return 0;
}

static int myfs_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	(void) fi;

	filenode_t* req_file = FileFromPath(path);
	size_t len;
	printf("Reading file at %s\n",path);
	if(req_file == NULL)
		return -ENOENT;
	if(req_file->content == NULL)
		return 0;

	len = strlen(req_file->content);
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, req_file->content + offset, size);
	} else
		size = 0;

	return size;
}

static const struct fuse_operations myfs_oper = {
	.init           = myfs_init,
	.getattr	= myfs_getattr,
	.readdir	= myfs_readdir,
	.open		= myfs_open,
	.read		= myfs_read,
};

int main(int argc, char *argv[])
{
	InitFS();
	int ret;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	ret = fuse_main(args.argc, args.argv, &myfs_oper, NULL);
	fuse_opt_free_args(&args);
	return ret;
}
