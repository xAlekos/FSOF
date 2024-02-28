#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>


enum filestypes{
	
	REG,
	DIR
};


/*
File che compone il file system, tutti i file sono di questo tipo diffenziati dal campo mode.

@name Il nome del file
@content Il contenuto del file ( Ignorato nel caso in cui questo sia una cartella)
@mode Il tipo di file e flag di permesso.
@dir_content Ignorato se type è diverso da DIR, riferimento a struttura contenente i file presenti nella cartella.
*/
typedef struct filenode {
    
	char name[50];

	uint8_t check;

    char* content;

	uint8_t type;

	__mode_t mode;

	struct filenode* dir_content; 
} filenode_t;

/*
Salva in buffer i token che compongono il path
Ritorna il numero di token
@path percorso del file ottenuto dall'interfaccia FUSE
@buffer Buffer in cui salvare i token che compongono il path utilizzati per la localizzazione del file nel file system.
*/
uint16_t TokenizePath(const char* path, char** tokens_buffer){
    
    char path_buffer[1000];
    strcpy(path_buffer,path);
    int n = 0;

    char* saveptr; //Usato per garantire la rientranza di strtok_r
    char* path_token = strtok_r(path_buffer,"/",&saveptr);
    
    while(path_token != NULL){
        
        strcpy(tokens_buffer[n],path_token);
       // tokens_buffer[n] = path_token;
        path_token = strtok_r(NULL,"/",&saveptr);
        n++;
    }

    return n;
}


/*
Funzione hash djb2 
*/
uint16_t HashFileName(char* name){

    uint16_t hash = 5381;
        int c;

        while(c = *name++)
            hash = ((hash << 5) + hash) + c; 

        return hash;

}


filenode_t root_dir; // /;

filenode_t files[50]; //FILES CONTENUTI IN /

filenode_t files2[50]; //FILES CONTENUTI IN /DIR




/*
Tramite l'hash di ogni token per directory trova il file corrispondente.
*/
filenode_t* FileFromPath(const char* path){
	
	if(strcmp(path,"/") == 0)
		return &root_dir;

    char* tokens[500];
    for(int i = 0 ; i<500; i++)
        tokens[i] = malloc(500);

    uint16_t n = TokenizePath(path,tokens);

    uint8_t i = 0;
    filenode_t* root_dir = files;

    uint8_t address = HashFileName(tokens[i++]) % 50;
    filenode_t* last_file = &root_dir[address];

    if(strcmp(last_file->name,tokens[0]) != 0) //TODO AGGIUNGERE CHECK COLLISIONI
        return NULL;
    n--;
    
    while(n>0){
        address = HashFileName(tokens[i]) % 50;
        last_file = &(last_file->dir_content[address]);
        if(strcmp(last_file->name,tokens[i]) != 0)
            return NULL;
        n--;
        i++;
    }

    for(int j = 0 ; j<500; j++)
        free(tokens[j]);

   return last_file;
}
 

/*
    Funzione che carica in memoria tutti i file che compongono il file system
*/

void InitFS(){
	char nomino[50];
    uint8_t address;

	/*Inizializzazione '/'*/
	root_dir.check = 1;
	root_dir.content = NULL;
	root_dir.dir_content = files;
	root_dir.mode = S_IFDIR | 0755;
	root_dir.type = DIR;
	strcpy(root_dir.name,"/");
	/*--------------------------------*/
	
	/*Inizializzazione contenuto di '/' */
	for(int i = 0; i<50;i++)
		memset(&files[i],0,sizeof(filenode_t));

    
    for(int i = 0; i<49; i++){

		snprintf(nomino,50,"Petrus_%d",i);
        address = HashFileName(nomino) % 50;
        strcpy(files[address].name,nomino);
        
		files[address].check = 1;
        files[address].content = "Contenuto File!";
		files[address].mode = S_IFREG | 0755;
		files[address].type = REG;
        files[address].dir_content = NULL;

    }
	/*--------------------------------------*/

    /*Inializzazione directory 'DIR' */
    address = HashFileName("DIR") % 50;
	strcpy(files[address].name, "DIR");
    
	files[address].check = 1;
	files[address].mode = S_IFDIR | 0444;
	files[address].dir_content = files2;
	files[address].type = DIR;
	/*---------------------------------*/

    /*Inizializzazione contenuto di '/DIR'*/
    for(int i = 0; i<49; i++){

		snprintf(nomino,50,"Francesco_%d",i);
        address = HashFileName(nomino) % 50;
        strcpy(files[HashFileName("DIR") % 50].dir_content[address].name,nomino);
		files[HashFileName("DIR") % 50].dir_content[address].check = 1; 
        files[HashFileName("DIR") % 50].dir_content[address].content = "Contenuto File 2!";
		files[HashFileName("DIR") % 50].dir_content[address].mode = S_IFREG | 0755;
		files[HashFileName("DIR") % 50].dir_content[address].type = DIR;
    }
	/*--------------------------------------*/
	printf("Initializing File System\n");
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

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
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

static int hello_open(const char *path, struct fuse_file_info *fi)
{
	filenode_t* req_file = FileFromPath(path);
	printf("Open file at %s\n",path);
	if (req_file == NULL)
		return -ENOENT;

	if ((fi->flags & O_ACCMODE) != O_RDONLY)
		return -EACCES;

	return 0;
}

static int hello_read(const char *path, char *buf, size_t size, off_t offset,
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

static const struct fuse_operations hello_oper = {
	.init           = hello_init,
	.getattr	= hello_getattr,
	.readdir	= hello_readdir,
	.open		= hello_open,
	.read		= hello_read,
};

int main(int argc, char *argv[])
{
	InitFS();
	int ret;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	ret = fuse_main(args.argc, args.argv, &hello_oper, NULL);
	fuse_opt_free_args(&args);
	return ret;
}
