#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

#define FILES_VECTOR_MAX_DIM 50
#define MAX_FILE_NAME 50
#define MAX_FILE_SIZE 5000

/*
 Utilizzati nel campo type di variabili di tipo filenode_t
*/
enum filetypes{
	REG,
	DIR
};


/*
File che compone il file system, tutti i file sono di questo tipo diffenziati dal campo mode.

@name Il nome del file
@content Il contenuto del file ( Ignorato nel caso in cui questo sia una cartella)
@check 1 se file è stato inizializzato altrimenti 0
@type definisce tipo file, (vedi enum filetypes)
@mode Il tipo di file e flag di permesso.
@dir_content Ignorato se type è diverso da DIR, riferimento a struttura contenente i file presenti nella cartella.
*/
typedef struct filenode {
    
	char name[MAX_FILE_NAME];

    char content[MAX_FILE_SIZE];

    uint8_t check;

	uint8_t type;

	__mode_t mode;

	struct filenode** dir_content;
    uint8_t dir_entries;

} filenode_t;


/*
Alloca in memoria lo spazio per contenere un file.
*/
filenode_t* AllocateFile(){

    filenode_t* files = malloc(sizeof(filenode_t)); 
    if(files == NULL)
        return NULL;
    memset(files,0,sizeof(filenode_t));
    return files;

}

/*
Alloca in memoria la tabella dei contenuti di una directory.
Questa contiene i riferimenti ad i file contenuti in una directory

@file directory in cui allocare la tabella dei contenuti
@num dimensioni della tabella
*/
filenode_t** AllocateDirEntries(filenode_t* dir,uint16_t num){
    filenode_t** pointers_vector= malloc(sizeof(filenode_t*) * num);
    if(pointers_vector == NULL)
        return NULL;
    return pointers_vector;
}

/*
Riempe i campi del file con i parametri non nulli passati alla funzione,
imposta il check di validità ad 1 indicando il file come inizializzato.
*/
void EditFileAttributes(filenode_t* file, char* new_name, char* new_content,uint8_t new_type, __mode_t new_mode, filenode_t** new_dir_content){
    
    if(new_name != NULL)
        strcpy(file->name,new_name);
    
    if(new_content != NULL)
        strcpy(file->content,new_content);

    if(new_type != 0)
        file->type = new_type;

    if(new_mode != 0) 
        file->mode = new_mode;

    if(new_dir_content != NULL)
        file->dir_content = new_dir_content;

    if(file->check == 0)
        file->check = 1;
}



/*
Salva in buffer i token che compongono il path
Ritorna il numero di token
@path percorso del file ottenuto dall'interfaccia FUSE
@buffer Buffer in cui salvare i token che compongono il path utilizzati per la localizzazione del file nel file system.
*/
uint16_t TokenizePath(const char* path, char** tokens_buffer){
    
    char path_buffer[1000];
    strcpy(path_buffer,path);
    int n_tokens = 0;

    char* saveptr; //Usato per garantire la rientranza di strtok_r
    char* path_token = strtok_r(path_buffer,"/",&saveptr);
    
    while(path_token != NULL){
        
        strcpy(tokens_buffer[n_tokens],path_token);
        path_token = strtok_r(NULL,"/",&saveptr);
        n_tokens++;
    }

    return n_tokens;
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


filenode_t* root_dir;

/*
Tramite l'hash di ogni token per directory trova il file corrispondente.
*/
filenode_t* FileFromPath(const char* path){
	
	if(strcmp(path,"/") == 0)
		return root_dir;

    char* tokens[500];
    uint8_t i = 0;
    filenode_t** rt_dir_content = root_dir->dir_content;

    for(int i = 0 ; i<500; i++)
        tokens[i] = malloc(500);

    uint16_t n_tokens = TokenizePath(path,tokens);

    uint8_t address = HashFileName(tokens[i++]) % FILES_VECTOR_MAX_DIM;
    filenode_t* last_file = rt_dir_content[address];
    n_tokens--;
    if(last_file == NULL)
        return NULL;
    if(strcmp(last_file->name,tokens[0]) != 0) //TODO AGGIUNGERE CHECK COLLISIONI
        return NULL;
    
    while(n_tokens>0){
        address = HashFileName(tokens[i]) % FILES_VECTOR_MAX_DIM;
        last_file = last_file->dir_content[address];
        if(last_file == NULL)
            return NULL;
        if(strcmp(last_file->name,tokens[i]) != 0)
            return NULL;
        n_tokens--;
        i++;
    }

    for(int j = 0 ; j<500; j++)
        free(tokens[j]);

   return last_file;
}
 

/*
Inizializza filesnum files allocando la memoria necessaria per memorizzarli
e salva un riferimento ai file appena allocati nel campo dir_content di una directory.

@dir directory che si vuole inizializzare
@filesnum numero di file da inizializzare
@contenuto contenuto con cui inizializzare i file
*/
void InitDir(filenode_t* dir, uint16_t filesnum,char* contenuto){
    char name_buff[MAX_FILE_NAME];
    uint16_t address;
    filenode_t* newfile;
    for(int i = 0; i<filesnum; i++){
        newfile = malloc(sizeof(filenode_t));
        snprintf(name_buff,MAX_FILE_NAME,"File_%d",i);
        address = HashFileName(name_buff) % FILES_VECTOR_MAX_DIM;
        dir->dir_content[address] = newfile;
        EditFileAttributes(dir->dir_content[address], name_buff, contenuto ,REG, S_IFREG | 0755, NULL);
        dir->dir_entries++;
    }

}

/*
Crea una

*/
filenode_t* MakeDir(filenode_t* parent_dir,char* name, uint16_t filesnum){
    uint16_t address = HashFileName(name) % FILES_VECTOR_MAX_DIM;
    EditFileAttributes(parent_dir->dir_content[address] , name, NULL , DIR, S_IFDIR | 0444, AllocateDirEntries(parent_dir->dir_content[address],FILES_VECTOR_MAX_DIM));
    return parent_dir->dir_content[address];
}


/*
    Funzione che carica in memoria tutti i file che compongono il file system
*/

void InitFS(){
	/*Inizializzazione ROOT DIR*/
	
    root_dir = AllocateFile();
    EditFileAttributes(root_dir , "/", NULL , DIR, S_IFDIR | 0444, AllocateDirEntries(root_dir,FILES_VECTOR_MAX_DIM));

    InitDir(root_dir, FILES_VECTOR_MAX_DIM - 1,"Contenuto file 1");
	/*--------------------------------------*/

    /*Inializzazione directory 'DIR' */
    filenode_t* dir = MakeDir(root_dir,"DIR",50);
	/*---------------------------------*/

    /*Inizializzazione contenuto di '/DIR'*/
    InitDir(dir, FILES_VECTOR_MAX_DIM ,"Contenuto file 2");
	/*--------------------------------------*/
	printf("Initializing File System");
}
