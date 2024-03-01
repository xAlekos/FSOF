#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
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
    filenode_t* rt_dir_content = root_dir.dir_content;

    uint8_t address = HashFileName(tokens[i++]) % 50;
    filenode_t* last_file = &rt_dir_content[address];

    if(last_file->check == 0 || strcmp(last_file->name,tokens[0]) != 0) //TODO AGGIUNGERE CHECK COLLISIONI
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

		snprintf(nomino,50,"FileInRootDir_%d",i);
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

		snprintf(nomino,50,"FileInDirDir_%d",i);
        address = HashFileName(nomino) % 50;
        strcpy(files[HashFileName("DIR") % 50].dir_content[address].name,nomino);
		files[HashFileName("DIR") % 50].dir_content[address].check = 1; 
        files[HashFileName("DIR") % 50].dir_content[address].content = "Contenuto File in DIR!";
		files[HashFileName("DIR") % 50].dir_content[address].mode = S_IFREG | 0755;
		files[HashFileName("DIR") % 50].dir_content[address].type = DIR;
    }
	/*--------------------------------------*/
	printf("Initializing File System\n");
}
