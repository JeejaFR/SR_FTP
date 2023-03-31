/*
 * echoclient.c - An echo client
 */
#include "csapp.h"
#define TAILLE_BLOC 1024
/*
* VARIABLE GLOBALE
*/
long int telecharge_global = 0;
int telechargement_en_cours = 0;
char nom_fichier_global[MAXLINE];


typedef struct{
    char *commande; // par exemple get / bye / ls
    char *nom_fichier; 
    char *donnee;
} REQ_MSG;

void handler(){
    printf("\n Déconnecté \n");
    FILE *log = fopen("client/log","w");
    if(telechargement_en_cours==1){
        char telecharge[MAXLINE];
        memset(telecharge,0,MAXLINE);
        sprintf(telecharge,"%ld",telecharge_global);
        if(nom_fichier_global[strlen(nom_fichier_global)-1]==' ') nom_fichier_global[strlen(nom_fichier_global)-1] = '\0';
        fputs(nom_fichier_global,log);
        fputc('\n',log);
        fputs(telecharge,log);
    }
    fclose(log);
    exit(1);
}

void recevoir_fichier(rio_t rio,REQ_MSG *req,int clientfd,int reprise){
    char *nom_fichier = req->nom_fichier;
    char *dossier = "client/";
    char  *chemin = malloc(strlen(nom_fichier)+strlen(dossier)+1);  
    strcpy(chemin,dossier);
    strcat(chemin,nom_fichier);

    if(chemin[strlen(chemin)-1]==' ') chemin[strlen(chemin)-1] = '\0';
    strcpy(nom_fichier_global,nom_fichier); // pour les logs
    // pour avoir le temps de telechargement : 
    struct timespec debut,fin;
    double temps_ecoule;

    char buffer[TAILLE_BLOC];
    ssize_t nread;
    ssize_t total = reprise;
    int fd;
   
    if(reprise==0) fd = open(chemin, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    else fd = open(chemin, O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR);
    
    telechargement_en_cours = 1;
    char tailleAttendueEnString[TAILLE_BLOC];
    Rio_readn(clientfd,tailleAttendueEnString,TAILLE_BLOC);
    long int tailleAttendue = atoi(tailleAttendueEnString);
    clock_gettime(CLOCK_MONOTONIC,&debut);
    //printf("total : %ld | tailleAttendu : %s | chemin : %s \n",total,tailleAttendueEnString,chemin);
    while((nread = Rio_readn(clientfd,buffer,TAILLE_BLOC))>0){     // ! ATTENTION ! ici utiliser Rio_readn pour les fichiers binaires (exemple : photo) 
        total+=nread;
        telecharge_global = total;
       

        if(strcmp(buffer,"Le fichier demandé n'existe pas")==0){      // Cas où le fichier n'existe pas 
            printf("Le fichier demandé n'existe pas\n");
            remove(chemin);   // supprime le fichier créer s'il n'existe pas sur le serveur 
        }
        if(write(fd,buffer,nread)==-1){
            perror("Erreur d'ecriture client");
            exit(1);
        }
        if(nread == -1){
            perror("Erreur de lecture client");
            exit(1);
        }

        // affichage telechargement
        printf("\r%ld / %ld Mo telechargés", total/1000000,tailleAttendue/1000000);
        fflush(stdout);  // pour forcer l'affichage immédiat

        // ON VERIFIE SI ON A TOUT RECU     (pour eviter de rester en attente)
        off_t save = SEEK_CUR;
        off_t taille = lseek(fd,0,SEEK_END);
        lseek(fd,0,save);
        if (taille>tailleAttendue) {
            break;  // Sort de la boucle while
        }
    }  
    clock_gettime(CLOCK_MONOTONIC,&fin);
    temps_ecoule = (fin.tv_sec - debut.tv_sec) +  (fin.tv_nsec - debut.tv_nsec) / 1000000000.0;
    struct stat st;
    stat(chemin,&st);

    printf("\rTelechargement terminé !\n");
    printf("%ld octets reçu en %f secondes (%0.2f Ko/s)\n",st.st_size,temps_ecoule,(st.st_size/temps_ecoule)/1000);
    telechargement_en_cours = 0;
    close(fd);
}


void gestion_reception(rio_t rio,REQ_MSG *req,int clientfd){
    if(strcmp(req->commande,"bye")==0){
        printf("Deconnexion...\n");
        FILE *log = fopen("client/log","w");
        fclose(log);
        Close(clientfd);
        exit(0);
    }
    if(strcmp(req->commande,"get")==0){
        recevoir_fichier(rio,req,clientfd,0);
    }
    else{
        if(req->donnee==NULL) recevoir_fichier(rio,req,clientfd,telecharge_global);
        else recevoir_fichier(rio,req,clientfd,atoi(req->donnee));
        //printf("Commande : %s\n",req->commande);
        //Close(clientfd);
        //exit(0);
    }
}

void reprendre_telechargement(rio_t rio,int clientfd,char* nom_fichier,long int donnee){
    REQ_MSG *req = (REQ_MSG*)malloc(sizeof(REQ_MSG));
    char *reprendre = "reprendre ";
    // donne en string pour l'envoie
    char *donnee_string = malloc(sizeof((char *)donnee));
    sprintf(donnee_string,"%ld",donnee);
    // on créer la commande à envoyer
    char commande[MAXLINE];
    strcpy(commande,reprendre);
    // on enleve le \n de nom_fichier
    nom_fichier[strlen(nom_fichier)-1] = ' ';

    strcat(commande,nom_fichier);

    strcat(commande,donnee_string);

    //on envoie la commande au serveur
    Rio_writen(clientfd, commande, MAXLINE);
    
    
    // initialiser les champs de la structure REQ_MSG
    req->commande = malloc(strlen("reprendre") + 1);
    strcpy(req->commande, "reprendre");

    req->nom_fichier = malloc(strlen(nom_fichier) + 1);
    strcpy(req->nom_fichier, nom_fichier);

    req->donnee = malloc(strlen(donnee_string) + 1);
    strcpy(req->donnee, donnee_string);
     
    gestion_reception(rio,req,clientfd);
}

void requete_client(rio_t rio,int clientfd){
    char  buf[MAXLINE],temp[MAXLINE];
    REQ_MSG *req = (REQ_MSG*)malloc(sizeof(REQ_MSG));

    printf("ftp> ");
    while (Fgets(buf, MAXLINE, stdin) != NULL ) {
        Rio_writen(clientfd, &buf, sizeof(buf)); // envoie requete au serveur
        memset(temp,0,strlen(temp));
        if(buf!=NULL){
            int i=0, aFichier=1,aTaille=1;
            while(buf[i]!=' '){ // pour avoir la commande 
                if(buf[i]!='\n'){
                    temp[i] = buf[i];
                }  
                i++;
                if(buf[i]=='\0'){
                    aFichier=0;
                    aTaille=0;
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
                while(buf[i]!='\0' && buf[i]!=' '){
                    if(buf[i]!='\n'){
                        temp[j] = buf[i];
                        j++;
                    }
                    i++;
                    if(buf[i]=='\0'){
                    aTaille=0;
                    break;
                    }
                }
                req->nom_fichier = malloc(sizeof(temp));
                strcpy(req->nom_fichier,temp);
            }
            memset(temp,0,sizeof(temp));
            if(aTaille){
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
                    if(buf[i]=='\0'){
                    aTaille=0;
                    break;
                    }
                }
                req->donnee = malloc(sizeof(temp));
                strcpy(req->donnee,temp);
            }
            memset(temp,0,sizeof(temp));
        }
        gestion_reception(rio,req,clientfd);
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
    FILE *log;

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
    signal(SIGINT,handler);
    clientfd = Open_clientfd(host, port);
    struct sockaddr_in peeraddr;
    socklen_t peerlen = sizeof(peeraddr);
    getpeername(clientfd,(SA *) &peeraddr,&peerlen);
    printf("numero de port distant : %d\n", ntohs(peeraddr.sin_port));

    Rio_readinitb(&rio, clientfd);
    printf("client connected to server OS\n"); 
    /*
     * At this stage, the connection is established between the client
     * and the server OS ... but it is possible that the server application
     * has not yet called "Accept" for this connection
     */
    log = fopen("client/log","r");
    char nom_fichier[MAXLINE];
    if(log!=NULL){
        if(fgets(nom_fichier,MAXLINE,log) != NULL){ // Dans ce cas, il y a des logs donc on doit reprendre le telechargement.
            fscanf(log,"%ld",&telecharge_global);
            reprendre_telechargement(rio,clientfd,nom_fichier,telecharge_global);
        }    
    }


    while(1){
        requete_client(rio,clientfd);   
    }


    Close(clientfd);
    exit(0);
}
