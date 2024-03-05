#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

#define DIR_ENTRIES_DIM 1000
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
Elemento contenuto in una cartella, si tratta nodi di una lista di file il
cui hash collide all'interno della stessa directory. 
@filenode riferimento al file vero e proprio
@next_colliding riferimento al prossimo file all'interno della lista di collisioni.
*/
typedef struct dir_entry{

    struct filenode* filenode;
    
    struct dir_entry* next_colliding;

} dir_entry_t;


/*
File che compone il file system, tutti i file sono di questo tipo diffenziati dal campo mode.

@name Il nome del file
@content Il contenuto del file ( Ignorato nel caso in cui questo sia una cartella)
@type definisce tipo file, (vedi enum filetypes)
@mode Il tipo di file e flag di permesso.
@dir_content Ignorato se type è diverso da DIR, riferimento a struttura contenente i file presenti nella cartella.
*/
typedef struct filenode {
    
	char name[MAX_FILE_NAME];

    char content[MAX_FILE_SIZE];

	uint8_t type;

	__mode_t mode;

	dir_entry_t** dir_content;

    uint8_t dir_entries;

} filenode_t;


/*
Alloca in memoria lo spazio per contenere un file.
Ritorna un riferimento al file appena allocato
*/
filenode_t* AllocateFile(){

    filenode_t* files = malloc(sizeof(filenode_t)); 

    if(files == NULL)
        return NULL;

    memset(files,0,sizeof(filenode_t));

    return files;

}

/*
Alloca in memoria lo spazio per contenre un elemento
all'interno della directory. 
*/
dir_entry_t* AllocateDirEntry(){

    dir_entry_t* new_dir_entry = malloc(sizeof(dir_entry_t));

    if(new_dir_entry == NULL)
        return NULL;

    memset(new_dir_entry,0,sizeof(dir_entry_t));

    return new_dir_entry;
}

/*
Alloca in memoria un array di dir_entries, 
ciascun dir_entry contiene un riferimento ad una lista di file il cui hash corrisponde all'indice
della dir_entry all'interno dell'array.

@num dimensioni della tabella
*/
dir_entry_t** AllocateDirEntries(uint16_t num){

    dir_entry_t** pointers_vector= malloc(sizeof(dir_entry_t*) * num);

    if(pointers_vector == NULL)
        return NULL;

    return pointers_vector;
}

/*
Riempe i campi del file con i parametri non nulli passati alla funzione,
*/
void EditFileAttributes(filenode_t* file, char* new_name, char* new_content,uint8_t new_type, __mode_t new_mode, dir_entry_t** new_dir_content){
    
    if(new_name != NULL)
        strcpy(file->name,new_name);
    
    if(new_content != NULL)
        strcpy(file->content,new_content);

    if(new_type != 0)
        file->type = new_type;

    if(new_mode != 0) 
        file->mode = new_mode;

    if(new_dir_content != NULL && file->type == DIR)
        file->dir_content = new_dir_content;

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
Funzione hash DJB2
Ritorna l'hash della stringa passatagli.
*/
uint16_t HashFileName(char* name){

    uint16_t hash = 5381;
        int c;

        while(c = *name++)
            hash = ((hash << 5) + hash) + c; 

        return hash;

}

/*
Dato il nome di un file effetta una ricerca lienare in una lista di collisioni
Ritorna un riferimento al file se trovato, NULL altrimenti

@file_name Nome del file da cercare
@collision_list_head Head della lista di collisioni
*/
filenode_t* SearchInCollisionList(const char* file_name, dir_entry_t* collision_list_head){

    while(collision_list_head != NULL){
        printf(" %s",collision_list_head->filenode->name);

        if(strcmp(collision_list_head->filenode->name,file_name) == 0)
            return collision_list_head->filenode;

        collision_list_head = collision_list_head->next_colliding;
    }
        return NULL;
}


/*
Crea un nuovo nodo nella lista di collisioni e ci aggiunge un file indicato da newfile.


@collision_list_head head della lista di collisioni in cui crare il nuovo nodo
@newfile Riferimento al file da aggiungere nel nodo della lista
*/
dir_entry_t* AddCollidingElement(dir_entry_t* collision_list_head, filenode_t* newfile){

    dir_entry_t* new_dir_entry = malloc(sizeof(dir_entry_t));
    if(new_dir_entry == NULL)
        return NULL;

    new_dir_entry->next_colliding = NULL;
    new_dir_entry->filenode = newfile;

    while(collision_list_head->next_colliding != NULL){
        collision_list_head = collision_list_head->next_colliding;
    }
    collision_list_head->next_colliding = new_dir_entry;
    return new_dir_entry;
}


/*
Dato il nome di un file ed il riferimento ad una directory
ritorna il file contenuto nella directory.

@file_name il nome del file di cui si vuole il riferimento
@dir riferimento alla directory contenente il file desiderato.
*/
filenode_t* GetDirElement(char* file_name, filenode_t* dir){
    
    filenode_t* req_file;
    uint16_t address = HashFileName(file_name) % DIR_ENTRIES_DIM;
    
        if(dir->dir_content[address] == NULL)
            return NULL;

        req_file = dir->dir_content[address]->filenode;
    
        if(strcmp(req_file->name,file_name) != 0)
            req_file = SearchInCollisionList(file_name,dir->dir_content[address]);

        return req_file;
}




filenode_t* root_dir;

/*
Tramite l'hash di ogni token per directory trova il file corrispondente,
ritorna un riferimento al file individuato da path
*/
filenode_t* FileFromPath(const char* path){
	
	if(strcmp(path,"/") == 0)
		return root_dir;

    char* tokens[500];
    uint8_t i = 0;

    for(int i = 0 ; i<500; i++)
        tokens[i] = malloc(500);

    uint16_t n_tokens = TokenizePath(path,tokens);

    filenode_t* last_file = GetDirElement(tokens[i++],root_dir);

    if(last_file == NULL)
        return NULL;

    n_tokens--;
    
    while(n_tokens>0){
        last_file = GetDirElement(tokens[i++],last_file);
        
        if(last_file == NULL)
            return NULL;

        n_tokens--;
    }

    for(int j = 0 ; j<500; j++)
        free(tokens[j]);

   return last_file;
}
 
/*
Alloca un nuovo file e lo inserisce all'intero di una cartella.
Alloca lo spazio per la dir_entry e per il file.

@dir Directory in cui inserire il nuovo file
@name Nome del nuovo file
*/
filenode_t* AddNewFileToDir(filenode_t* dir, char* name){
    
    uint16_t address = HashFileName(name) % DIR_ENTRIES_DIM;
    filenode_t* newfile = malloc(sizeof(filenode_t));
    strcpy(newfile->name,name);

    if(dir->dir_content[address] == NULL){
        dir->dir_content[address] = AllocateDirEntry();
        dir->dir_content[address]->filenode = newfile;
    }
    else
        AddCollidingElement(dir->dir_content[address],newfile);

    return newfile; 

}



/*
Riempe una directory con filesnum file regolari.

@dir directory che si vuole riempire
@filesnum numero di file da aggiungere
@contenuto contenuto con cui inizializzare i file
*/
void FillDir(filenode_t* dir, uint16_t filesnum,char* contenuto){
    
    char name_buff[MAX_FILE_NAME];
    filenode_t* newfile;

    for(uint16_t i = 0; i<filesnum; i++){

        snprintf(name_buff,MAX_FILE_NAME,"File_%d",i);
        newfile = AddNewFileToDir(dir,name_buff);
        EditFileAttributes(newfile, name_buff, contenuto ,REG, S_IFREG | 0755, NULL);
        dir->dir_entries++;

    }

}

/*
Alloca una directory con un vettore di dir entries di dimensione filesnum.

@parent_dir La directory all'interno della quale verrà allocata la nuova directory.
@name Il nome della nuova directory
*/
filenode_t* MakeDir(filenode_t* parent_dir,char* name){

    filenode_t* newdir = AddNewFileToDir(parent_dir,"DIR");
    EditFileAttributes(newdir , name, NULL , DIR, S_IFDIR | 0444, AllocateDirEntries(DIR_ENTRIES_DIM));
    return newdir;
}


/*
    Funzione che alloca in memoria tutti i file che compongono il file system
*/

void InitFS(){
	/*Inizializzazione ROOT DIR*/
	
    root_dir = AllocateFile();
    EditFileAttributes(root_dir , "/", NULL , DIR, S_IFDIR | 0444, AllocateDirEntries(DIR_ENTRIES_DIM));

    FillDir(root_dir, 50,"Contenuto file 1");
	/*--------------------------------------*/

    /*Inializzazione directory 'DIR' */
    filenode_t* dir = MakeDir(root_dir,"DIR");
	/*---------------------------------*/

    /*Inizializzazione contenuto di '/DIR'*/
    FillDir(dir, 50 ,"Contenuto file 2");
	/*--------------------------------------*/
	printf("Initializing File System");
}
