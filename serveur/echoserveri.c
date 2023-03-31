/*
 * echoserveri.c - An iterative echo server
 */

#include "csapp.h"

#define MAX_NAME_LEN 256
#define N_PROC 1
#define TAILLE_BLOC 1024

typedef struct{
    char *commande; // par exemple get / bye / ls
    char *nom_fichier; 
} REQ_MSG;

void handler(int sig){
	int status,pid;
	while((pid=waitpid(-1,&status,0))>0){
		kill(pid,SIGINT);
	}
	exit(0);
}

void echo(int connfd);

void send_file(int connfd,char *nom_fichier,int reprise){
    FILE *fp = fopen(nom_fichier,"r");
    if(fp == NULL){
        Rio_writen(connfd,"Le fichier demandé n'existe pas",sizeof("Le fichier demandé n'existe pas"));
        fclose(fp);
        perror("Le fichier demandé n'existe pas");
        return;
    }

    // Obtient la taille du fichier attendu
    struct stat st;
    if (stat(nom_fichier, &st) == -1) {
        perror("Erreur lors de la récupération de la taille du fichier");
        exit(1);
    }
    off_t tailleAttendue = st.st_size;
    char data[TAILLE_BLOC];
    size_t octets_lus = 0;

    printf("Envoie des données en cours...");
    char tailleEnString[TAILLE_BLOC];
    snprintf(tailleEnString,TAILLE_BLOC,"%ld",tailleAttendue);

    if(rio_writen(connfd,tailleEnString,TAILLE_BLOC)==-1){
        printf("\rEchec de l'envoie, le client s'est deconnecté...\n");
        fclose(fp);
        return;
    }
    

    // reprendre où on s'est arrété precedemment :
    while(octets_lus<reprise){
        size_t n1 = fread(data,1,TAILLE_BLOC,fp);
        octets_lus+=n1;
    }

    while(!feof(fp)){
        size_t n = fread(data,1,TAILLE_BLOC,fp);
        octets_lus+=n;
        if(rio_writen(connfd,data,TAILLE_BLOC)==-1){
            printf("\r Echec de l'envoie , le client s'est deconnecté...\n");
            fclose(fp);
            return;
        }
    }
    fclose(fp);
    printf("\rFichier envoyé !                                                    \n");
}

void gerer_demande(int connfd){ // Gestionnaire de commande
    char buf[MAXLINE], temp[MAXLINE],commande[MAXLINE],nom_fichier[MAXLINE];
    int aFichier=1;
    rio_t rio;

    //Rio_readinitb(&rio, connfd);

    while(1){
        Rio_readinitb(&rio, connfd);
        memset(buf,0,MAXLINE);
        memset(temp,0,MAXLINE);
        memset(commande,0,MAXLINE);
        memset(nom_fichier,0,MAXLINE);
        aFichier = 1; // remise à 1 de aFichier pour la prochaine requête
        printf("En attente d'une requete...\n");
        // lire la prochaine requête
        if (Rio_readlineb(&rio, buf, MAXLINE) <= 0) {
            break; // fin de la connexion
        }

        //printf("buffer : |%s| %ld\n",buf,strlen(buf));
        printf("reception de la demande : %s \n",buf);
        if(strlen(buf)!=1){
            int i=0;
            if(buf!=NULL){
                while(buf[i]!=' '){ // pour avoir la commande 
                    if(buf[i]!='\n'){
                        temp[i] = buf[i];
                    } 
                    i++; 
                    if(buf[i]=='\0'){
                        aFichier=0;
                        break;
                    }
                }
                strcpy(commande,temp);
                memset(temp,0,strlen(temp)); // remise à 0 de temp
                if(aFichier){
                    while(buf[i]==' '){
                        i++;
                    }
                    int j=0;
                    while(buf[i]!='\0'){
                        if(buf[i]!='\n'){
                            temp[j] = buf[i];
                            j++;
                        }           
                        i++;
                    }
                    strcpy(nom_fichier,temp);
                }
            }
            if(strcmp(commande,"get")==0){
                if(aFichier==0){
                    Rio_writen(connfd, "nom de fichier requis avec la commande get\n", sizeof("nom de fichier requis avec la commande get\n"));
                    continue; // lire la prochaine requête
                }else{
                    char *dossier = "serveur/data/";
                    char *chemin = malloc(strlen(dossier) + strlen(nom_fichier) +1);
                    strcpy(chemin,dossier);
                    strcat(chemin,nom_fichier);

                    send_file(connfd,chemin,0);
                }  
            }
            else if(strcmp(commande,"bye")==0){
                printf("Le client %d s'est déconnecté.\n",connfd);
            }
            else if(strcmp(commande,"reprendre")==0){
                char *dossier = "serveur/data/";
                char nom[sizeof(nom_fichier)];  // le nom du fichier
                char donnee[sizeof(nom_fichier)]; // le nombre d'octet deja reçu par le client
                i=0;
                while(nom_fichier[i]!=' '){ // on recupere le nom du fichier
                    nom[i] = nom_fichier[i];
                    i++;
                }
                i++;
                int j=0;
                while(nom_fichier[i]!='\0'){
                    donnee[j] = nom_fichier[i];
                    j++;
                    i++;
                }

                char *chemin = malloc(strlen(dossier) + strlen(nom_fichier) +1);
                strcpy(chemin,dossier);
                strcat(chemin,nom);

                send_file(connfd,chemin,atoi(donnee));
            }
            else{
                printf("Commande: %s\nFichier: %s\n",commande,nom_fichier); 
                exit(1);
            }
        }        
    }
}    




/* 
 * Note that this code only works with IPv4 addresses
 * (IPv6 is not supported)
 */
int main(int argc, char **argv)
{
    int listenfd,connfd;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    //char client_ip_string[INET_ADDRSTRLEN];
    //char client_hostname[MAX_NAME_LEN];
    struct hostent *hp;
    char *haddrp;
    pid_t pid;

    signal(SIGINT,handler);
    listenfd = Open_listenfd(2121);

    for(int i=0;i<N_PROC;i++){
        if((pid = fork())==0){  /* fils*/
            while(1){
                clientlen = (socklen_t)sizeof(clientaddr);
                connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

                hp = Gethostbyaddr((const char*)&clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr),AF_INET);
                haddrp = inet_ntoa(clientaddr.sin_addr);
                printf("Server conected to %s (%s) \n", hp->h_name, haddrp);

                gerer_demande(connfd);

                Close(connfd);
            }
            exit(0);
        }
    }

    while (1) {
        // on reste en attente
    }
    exit(0);
}

