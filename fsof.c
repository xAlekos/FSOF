#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stddef.h>
#include <assert.h>

#include "filesystem.h"



static void *myfs_init(struct fuse_conn_info *conn,
			struct fuse_config *cfg)
{
	(void) conn;
	printf("Init Conn Kernel Module\n");
	cfg->kernel_cache = 1;
	return NULL;
	
}

static int myfs_getattr(const char *path, struct stat *stbuf,
			 struct fuse_file_info *fi)
{
	(void) fi;
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));

	printf("Get attr %s\n",path);

	if(strcmp(path, "/") == 0){
		stbuf->st_mode = S_IFDIR | 0444;
		stbuf->st_nlink = 2;
		return 0;
	}
	
	filenode_t* req_file = FileFromPath(path);


	if(req_file != NULL){

		stbuf->st_mode = req_file->mode;
		stbuf->st_nlink = 1;
		stbuf->st_size = req_file->file_size;
	}
	else
		return -ENOENT;

	return res;
}

void ReadCollisionList(dir_entry_t* collision_list_head, void* buf, fuse_fill_dir_t filler){
    
    while(collision_list_head != NULL){

		filler(buf,collision_list_head->filenode->name,NULL,0,0);
		collision_list_head = collision_list_head->next_colliding;
    }
        
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

	for(int i = 0; i < DIR_ENTRIES_DIM; i++){

		if(req_dir->dir_content[i] != NULL && req_dir->dir_content[i]->filenode != NULL)
			ReadCollisionList(req_dir->dir_content[i],buf,filler);

	}

	return 0;
}

static int myfs_open(const char *path, struct fuse_file_info *fi)
{

	filenode_t* req_file = FileFromPath(path);
	printf("Open file %s\n",path);
	if (req_file == NULL)
		return -ENOENT;

	if ((fi->flags & O_ACCMODE) != O_RDONLY && (fi->flags & O_ACCMODE) != O_WRONLY) 
		return -EACCES;

	return 0;
}

static int myfs_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	(void) fi;

	filenode_t* req_file = FileFromPath(path);
	size_t len;
	printf("Reading file %s\n",path);
	if(req_file == NULL)
		return -ENOENT;
	if(req_file->file_size == 0)
		return 0;

	len = req_file->file_size;
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, req_file->content + offset, size);
	} else
		size = 0;

	return size;
}

static int myfs_write(const char *path, const char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	(void) fi;

	filenode_t* req_file = FileFromPath(path);
	printf("Writing to file %s\n",path);

	if(req_file == NULL)
		return -ENOENT;

	if(offset + size < MAX_FILE_SIZE)
		memmove(req_file->content + offset, buf,size);
	else
		size = 0;
	req_file->file_size = (req_file->file_size - (req_file->file_size - offset)) + (size);
	return size;
}

static int myfs_truncate(const char* path, off_t off, struct fuse_file_info *fi){
	
	(void)fi;
	filenode_t* req_file = FileFromPath(path);
	printf("Truncating to %ld file %s\n",off, path);
	req_file->file_size = off;
	return 0;

}

static int myfs_mkdir (const char *path, mode_t mode){
	//TODO sistemare rilevazione errori
	filenode_t* parent_dir = GetParentDir(path);
	char* name = FileNameFromPath(path);

	MakeDir(parent_dir, name, mode | S_IFDIR);
	free(name);
	printf("create dir %s\n",path);
	return 0;
}

static int myfs_create(const char* path, mode_t mode, struct fuse_file_info * fi){
	//TODO sistemare rilevazione errori
	(void)fi;
	filenode_t* parent_dir = GetParentDir(path);
	
	char* name = FileNameFromPath(path);
	filenode_t* newfile = AddNewFileToDir(parent_dir, name, NULL , S_IFREG | mode, REG , NULL);
	if(newfile == NULL)
		return -EEXIST;
	free(name);
	printf("create file %s\n",path);
	return 0;

}

static int myfs_rename(const char* path, const char* new_path, unsigned int flags){
	//TODO sistemare roba, gestire flag e altro :(
	filenode_t* req_file;
	filenode_t* old_parent_dir = GetParentDir(path);
	filenode_t* parent_dir = GetParentDir(new_path);
	char* old_file_name = FileNameFromPath(path);
	char* new_file_name = FileNameFromPath(new_path);

	printf("Renaming file %s to %s with flags: %d\n", path , new_path,flags);

	filenode_t* file_with_same_name = FileFromPath(new_path);

	if(file_with_same_name != NULL && flags != 0){
		return -EEXIST;
	}

	if(file_with_same_name != NULL && flags == 0){
		FreeFileFromPath(new_path);
		file_with_same_name = NULL;
	}
	
	req_file = UnlinkFileFromDir(old_parent_dir, old_file_name);

	if(req_file == NULL)
		return -ENOENT; 

	AddExistingFileToDir(parent_dir,req_file, new_file_name);

	free(old_file_name);
	free(new_file_name);
	return 0;

}

static int myfs_unlink(const char* path){	
	printf("Removing file %s\n", path);
	return FreeFileFromPath(path);

}

static int myfs_chmod(const char* path, mode_t new_mode, struct fuse_file_info *fi){
	
	(void)fi;
	filenode_t* req_file = FileFromPath(path);

	if(req_file == NULL)
		return -ENOENT;

	printf("Changing mode of file %s to %d\n", path, new_mode);
	
	EditFileAttributes(req_file,NULL,NULL,0,new_mode,NULL);

	return 0;
}

static const struct fuse_operations myfs_oper = {

	.init       = myfs_init,
	.getattr	= myfs_getattr,
	.readdir	= myfs_readdir,
	.open		= myfs_open,
	.read		= myfs_read,
	.write 		= myfs_write,
	.mkdir      = myfs_mkdir,
	.create     = myfs_create,
	.truncate   = myfs_truncate,
	.rename     = myfs_rename,
	.unlink     = myfs_unlink,
	.chmod   	= myfs_chmod
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
