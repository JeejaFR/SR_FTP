/*
 * echoclient.c - An echo client
 */
#include "csapp.h"
#define TAILLE_BLOC 1024

typedef struct{
    char *commande; // par exemple get / bye / ls
    char *nom_fichier; 
} REQ_MSG;

void recevoir_fichier(rio_t rio,REQ_MSG *req,int clientfd){
    char *nom_fichier = req->nom_fichier;
    if(strcmp(req->commande,"bye")==0){
        printf("Deconnexion...\n");
        Close(clientfd);
        exit(0);
    }
    if(strcmp(req->commande,"get")!=0){
        return;
    }

    char *dossier = "client/";
    char  *chemin = malloc(strlen(nom_fichier)+strlen(dossier)+1);  
    strcpy(chemin,dossier);
    strcat(chemin,nom_fichier);

    // pour avoir le temps de telechargement : 
    struct timespec debut,fin;
    double temps_ecoule;

    char buffer[TAILLE_BLOC];
    ssize_t nread;
    ssize_t total = 0;
    int fd = open(chemin, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    
    
    char tailleAttendueEnString[TAILLE_BLOC];
    long int tailleAttendue;
    Rio_readn(clientfd,tailleAttendueEnString,TAILLE_BLOC);
    clock_gettime(CLOCK_MONOTONIC,&debut);
    while((nread = Rio_readn(clientfd,buffer,TAILLE_BLOC))>0){     // ! ATTENTION ! ici utiliser Rio_readn pour les fichiers binaires (exemple : photo) 
        total+=nread;
        if(strcmp(buffer,"Le fichier demandé n'existe pas")==0){      // Cas où le fichier n'existe pas 
            printf("Le fichier demandé n'existe pas\n");
            remove(chemin);   // supprime le fichier créer s'il n'existe pas sur le serveur 
            //close(fd);
            //Close(clientfd);
            //exit(1);
        }
        if(write(fd,buffer,nread)==-1){
            perror("Erreur d'ecriture client");
            exit(1);
        }
        if(nread == -1){
            perror("Erreur de lecture client");
            exit(1);
        }

        // ON VERIFIE SI ON A TOUT RECU     (pour eviter de rester en attente)
        FILE *fp = fopen(chemin,"rb");
        if(fp == NULL){
            perror("Erreur d'ouverture du fichier");
            exit(1);
        }
        fseek(fp, 0, SEEK_END); // Déplace le curseur à la fin du fichier
        long int taille = ftell(fp); // Obtient la position courante du curseur, qui est la taille du fichier
        fclose(fp); // Ferme le fichier
        sscanf(tailleAttendueEnString,"%ld",&tailleAttendue);
        if (taille>tailleAttendue) {
            break;  // Sort de la boucle while
        }
    }
    clock_gettime(CLOCK_MONOTONIC,&fin);
    temps_ecoule = (fin.tv_sec - debut.tv_sec) +  (fin.tv_nsec - debut.tv_nsec) / 1000000000.0;
    struct stat st;
    stat(chemin,&st);

    printf("Telechargement terminé !\n");
    printf("%ld octets reçu en %f secondes (%0.2f Ko/s)\n",st.st_size,temps_ecoule,(st.st_size/temps_ecoule)/1000);
    close(fd);
    //Close(clientfd);
    //exit(0);
}


void requete_client(rio_t rio,int clientfd){
    char  buf[MAXLINE];
    REQ_MSG *req = (REQ_MSG*)malloc(sizeof(REQ_MSG));

    printf("ftp> ");
    while (Fgets(buf, MAXLINE, stdin) != NULL ) {
        //printf("buf : !%s!\n",buf);
        Rio_writen(clientfd, &buf, sizeof(buf));
        if(buf!=NULL){
            int i=0, aFichier=1;
            char temp[MAXLINE];
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
            req->commande = malloc(sizeof(temp));
            strcpy(req->commande,temp);
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
                req->nom_fichier = malloc(sizeof(temp));
                strcpy(req->nom_fichier,temp);
            }
            memset(temp,0,sizeof(temp));
        }
        recevoir_fichier(rio,req,clientfd);
        printf("ftp> ");

        memset(buf,0,sizeof(buf));
        int t = sizeof(req->commande);
        memset(req->commande,0,t);
        t = sizeof(req->nom_fichier);
        memset(req->nom_fichier,0,t);

    }
}





int main(int argc, char **argv)
{
    rio_t rio;
    int clientfd, port;
    char *host;
    

    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }
    host = argv[1];
    port = atoi(argv[2]);

    /*
     * Note that the 'host' can be a name or an IP address.
     * If necessary, Open_clientfd will perform the name resolution
     * to obtain the IP address.
     */
    clientfd = Open_clientfd(host, port);
    struct sockaddr_in peeraddr;
    socklen_t peerlen = sizeof(peeraddr);
    getpeername(clientfd,(SA *) &peeraddr,&peerlen);
    printf("numero de port distant : %d\n", ntohs(peeraddr.sin_port));

    Rio_readinitb(&rio, clientfd);
    /*
     * At this stage, the connection is established between the client
     * and the server OS ... but it is possible that the server application
     * has not yet called "Accept" for this connection
     */
    printf("client connected to server OS\n"); 

    while(1){
        requete_client(rio,clientfd);
    }


    Close(clientfd);
    exit(0);
}
