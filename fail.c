#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct file {
    char* name;
    char* content;
};

struct file nodi_file[50];

void init_file(struct file* nodi_file){
    for(int i = 0; i<50; i++){
        nodi_file->content ="Giancia";
        nodi_file->name = "Petrus";
    }
}



void printfile(struct file* nodo){
    for(int i = 0; i < 50; i ++){
    printf("NOME : %s\nCONTENT:%s\n",nodo->name,nodo->content);
    }
}

int main(){
    init_file(nodi_file);
    printfile(nodi_file);

}