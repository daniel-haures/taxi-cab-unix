#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <signal.h> 
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include "common.h"
#include <time.h>

int SO_HOLES,SO_TOP_CELLS,SO_SOURCESGEN,SO_TAXI,SO_CAP_MIN,SO_CAP_MAX; 
int SO_TIMENSEC_MIN,SO_TIMENSEC_MAX,SO_TIMEOUT,SO_DURATION; 

/*indirizzi di array da dealocare alla fine*/
int * sourcepos[2];
int * notsourcepos[2];
pid_t * sourcepids;
pid_t * taxipids;
char ** passargv;
int res[2];
pid_t supportpid;
pid_t secondsupportpid;


int shm_id,sem_id;
int * q_id;
mainstructure * mains;
int counttaxi;
int countsources;
int totalargs;

void createtaxi(int index,int increment);
void shutdown(int endprint);
void setmap();
void enddata();
void printmap();

void sigalarm_handler(int signum){
	shutdown(1);
}

void childsigterm_handler(int signum){
	int i;
	free(sourcepids);
	free(taxipids);
	free(sourcepos[0]);
	free(sourcepos[1]);
	free(notsourcepos[0]);
	free(notsourcepos[1]);
	for(i=0;i<totalargs;i++){
		free(passargv[i]);
	}
	free(q_id);
	shmdt(mains);
	exit(0);
}


int main (int argc, char * argv[],char * envp[]) {

	int i,j;	
	int lenght,randomvalue;
	int tent;
	int support,msgsupport,pidi,oldpid;
	int status;
	struct timespec timer;
	struct msgtaxi rcvmsg;
	sigset_t old_mask;
	
	counttaxi=0;
	countsources=0;
	supportpid=0;
	secondsupportpid=0;
	srand ( time(NULL) );

	if(getenv("SO_HOLES")==NULL || getenv("SO_TOP_CELLS")==NULL ||
	getenv("SO_SOURCESGEN")==NULL || getenv("SO_TAXI")==NULL ||
	getenv("SO_CAP_MIN")==NULL || getenv("SO_CAP_MAX")==NULL ||
	getenv("SO_TIMENSEC_MIN")==NULL || getenv("SO_TIMENSEC_MAX")==NULL ||
	getenv("SO_TIMEOUT")==NULL || getenv("SO_DURATION")==NULL){
		printf("VARIABILI D'AMBIENTE NON DICHIARATE\n");
		printf("Caricare le variabili d'ambiente dagli apositi file sh utilizando il comando source\n");
		exit(0);
	}

	/*Imposto i handler per la chiusura coretta del programma*/
	set_handler(SIGALRM,&sigalarm_handler);
	set_handler(SIGINT,&sigalarm_handler);
	set_handler(SIGTERM,&sigalarm_handler);

	/*Carico le variabili d'ambiente*/
	SO_HOLES=atoi(getenv("SO_HOLES")); 
	SO_TOP_CELLS=atoi(getenv("SO_TOP_CELLS"));
	SO_SOURCESGEN=atoi(getenv("SO_SOURCESGEN"));
	SO_TAXI=atoi(getenv("SO_TAXI"));
	SO_CAP_MIN=atoi(getenv("SO_CAP_MIN"));
	SO_CAP_MAX=atoi(getenv("SO_CAP_MAX"));
	SO_TIMENSEC_MIN=atoi(getenv("SO_TIMENSEC_MIN"));
	SO_TIMENSEC_MAX=atoi(getenv("SO_TIMENSEC_MAX"));
	SO_TIMEOUT=atoi(getenv("SO_TIMEOUT"));
	SO_DURATION=atoi(getenv("SO_DURATION"));
	
	if(SO_DURATION>0 && SO_TIMEOUT>0){
	printf("OK\n");
	}else{
	printf("Controlla le variabili d'ambiente\n");
	exit(0);
	}
	
	totalargs=11+maxsecsize();
	
	/*Allocazione della memoria per i vettori la cui lunghezza dipende dalle variabili d'ambiente*/
	sourcepos[0]=calloc(SO_SOURCESGEN,sizeof(int));
	sourcepos[1]=calloc(SO_SOURCESGEN,sizeof(int));
	notsourcepos[0]=calloc((SO_WIDTH*SO_HEIGHT-SO_HOLES)-SO_SOURCESGEN,sizeof(int));
	notsourcepos[1]=calloc((SO_WIDTH*SO_HEIGHT-SO_HOLES)-SO_SOURCESGEN,sizeof(int));
	sourcepids=calloc(SO_SOURCESGEN,sizeof(pid_t));
	taxipids=calloc(SO_TAXI,sizeof(pid_t));
	q_id=calloc(maxsecsize()+1,sizeof(int));
	passargv=calloc(totalargs,sizeof(char *));
	
	
	
	/*Creo un area di memoria condivisa*/
	shm_id = shmget(IPC_PRIVATE,sizeof(mainstructure),0600 |IPC_CREAT);
	/*Attach della memoria condivisa*/
	mains= shmat(shm_id,NULL,0);
	/*Chiusura della shm, una volta che è stata dettach da tutti i processi*/
	shmctl ( shm_id , IPC_RMID , NULL ) ;
	
	/*Creo una coda di messaggi per ogni sezione*/
	for(i=0;i<maxsecsize();i++){
	q_id[i]= msgget(IPC_PRIVATE+i, IPC_CREAT | 0600);
	}
	
	/*Creo un coda per segnalare al padre di creare un nuovo taxi*/
	support=maxsecsize();
	q_id[support]= msgget(IPC_PRIVATE+support, IPC_CREAT | 0600);
	support=0;
	
	
	/*Creo due semafori per ogni cella, uno per definire quanti processi vi possono avere acesso
	e un altro per permettere la modifica in mutua esclusione dei paramentri della cella*/
	/*Vengono creati anche semafori per la gestione dei processi e delle code di messaggi*/
	sem_id=semget(IPC_PRIVATE,(SO_WIDTH*SO_HEIGHT-SO_HOLES)*2+4+(maxsecsize()+1),0600 | IPC_CREAT);
	/*Semaforo per stampare la mappa senza l'interferenza dei processi figli*/
	semctl(sem_id,(SO_WIDTH*SO_HEIGHT-SO_HOLES)*2, SETVAL,(SO_TAXI));
	/*Semaforo per aspettare l'inizializzazione dei processi child*/
	semctl(sem_id,(SO_WIDTH*SO_HEIGHT-SO_HOLES)*2+1, SETVAL,0);
	/*Semaforo per permettere a tutti i processi child di iniziare simultaneamente*/
	semctl(sem_id,(SO_WIDTH*SO_HEIGHT-SO_HOLES)*2+2, SETVAL,0);
	/*Semaforo per modifica parametri di successo nella shm*/
	semctl(sem_id,(SO_WIDTH*SO_HEIGHT-SO_HOLES)*2+3, SETVAL,1);
	/*Semafori per accesso esclusivo alla coda di messaggi, uno per ogni sezione*/
	/*Aggiungiamo un semaforo per la coda di messaggi tra master e taxi*/
	for(i=0;i<maxsecsize()+1;i++){
	semctl(sem_id,(SO_WIDTH*SO_HEIGHT-SO_HOLES)*2+4+i, SETVAL,1);
	}
	
	
	/*Imposto la mappa, posizione dei buchi e sources*/
	setmap();
	
	/*Definisco elementi in comune da passare ai processi taxi e source*/
	/*Inserisco l'ID della memoria condivisa*/
	printf("%d\n",shm_id);
	lenght= snprintf( NULL, 0, "%d", shm_id );
	passargv[1] = malloc(lenght+1);
	snprintf( passargv[1], lenght+1, "%d", shm_id );
	
	/*Inserisco l'ID del vettore di semafori*/
	printf("%d\n",sem_id);
	lenght= snprintf( NULL, 0, "%d", sem_id );
	passargv[2] = malloc(lenght+1);
	snprintf( passargv[2], lenght+1, "%d", sem_id );	
	
	/*Inserisco SO_HOLES*/
	lenght= snprintf( NULL, 0, "%d", SO_HOLES );
	passargv[5] = malloc(lenght+1);
	snprintf( passargv[5], lenght+1, "%d", SO_HOLES);
			
	/*Inserisco SO_TIMEOUT*/
	lenght= snprintf( NULL, 0, "%d", SO_TIMEOUT );
	passargv[6] = malloc(lenght+1);
	snprintf( passargv[6], lenght+1, "%d", SO_TIMEOUT);
			
	/*Inserisco SO_TAXI*/
	lenght= snprintf( NULL, 0, "%d", SO_TAXI );
	passargv[7] = malloc(lenght+1);
	snprintf( passargv[7], lenght+1, "%d", SO_TAXI);
			
	/*Id coda verso master*/
	support=maxsecsize();
	lenght= snprintf( NULL, 0, "%d", q_id[support] );
	passargv[8] = malloc(lenght+1);
	snprintf(passargv[8], lenght+1, "%d", q_id[support]);
	
	/*Inserisco gli ID delle code di messaggi*/
	for(i=0;i<maxsecsize();i++){
		lenght= snprintf( NULL, 0, "%d", q_id[i] );
		passargv[10+i] = malloc(lenght+1);
		snprintf( passargv[10+i], lenght+1, "%d", q_id[i]);
	}
			
	/*L'ultimo elemento di argv deve essere un puntatore a NULL*/
	passargv[11+(maxsecsize()-1)] = NULL;
	
	
	/*FORK DEI TAXI*/
	for(i=0;i<SO_TAXI;i++){
		/*Ricerco una posizione per il taxi i e incremento il semaforo in quella posizione*/
		createtaxi(i,1);
		
	}
	
	
	/*FORK DELLE SOURCE*/
	for(i=0;i<SO_SOURCESGEN;i++){
		switch(sourcepids[i]=fork()){
		case 0:
		
			printf("I am the source child %d\n",getpid());
			
			passargv[0]="source";
			
			/*Inserisco la posizione casuale x*/
			lenght= snprintf( NULL, 0, "%d", sourcepos[0][i] );
			passargv[3] = malloc(lenght+1);
			snprintf( passargv[3], lenght+1, "%d", sourcepos[0][i]  );
			
			/*Inserisco la posizione casuale y*/
			lenght= snprintf( NULL, 0, "%d", sourcepos[1][i]   );
			passargv[4] = malloc(lenght+1);
			snprintf( passargv[4], lenght+1, "%d", sourcepos[1][i] );
			
			/*indice del processo*/
			lenght= snprintf( NULL, 0, "%d", i);
			passargv[9] = malloc(lenght+1);
			snprintf(passargv[9], lenght+1, "%d", i);
			
			
			
			/*Genero un altro processo con aree di memoria reinizializate*/
			execve("source",passargv,NULL);
			printf("Error for execve erno:%s\n",strerror(errno));
			exit(0);
			break;
			
		case -1:
			printf("Error for fork erno:%s\n",strerror(errno));
			break;
		default:
			countsources++;
			break;
		
		}
	}
	
	
	/*Aspetto l'inizializzazione dei processi figli*/
	sem_cmd(sem_id,(SO_WIDTH*SO_HEIGHT-SO_HOLES)*2+1,-(SO_TAXI+SO_SOURCESGEN),0);
	printf("procesi inizializzati\n");
	printf("counttaxi:%d\n",counttaxi);
	printf("%d = %d \n",countsources,SO_SOURCESGEN);

	/*Invio il segnale di start ai processi figli*/
	sem_cmd(sem_id,(SO_WIDTH*SO_HEIGHT-SO_HOLES)*2+2,(SO_TAXI+SO_SOURCESGEN),0);

	/*imposto la durata della simulazione*/
	alarm(SO_DURATION);
	printf("alarm send\n");
	/*Un processo per inviare i segnali alle so_source*/
	switch(supportpid=fork()){
		case 0:
			/*Per il figlio utilizo un handler di terminazione meno complicato*/
			set_handler(SIGTERM,&childsigterm_handler);
			set_handler(SIGINT,&childsigterm_handler);
			set_handler(SIGALRM,&childsigterm_handler);
			do{
				/*Ogni un secondo viene inviato un segnale di alarm a un figlio source scelto a caso*/
				timer.tv_sec=1;
				timer.tv_nsec=0;
				nanosleep(&timer,NULL);
				randomvalue=rand() % countsources;
				kill(sourcepids[randomvalue],SIGALRM);
				
				
			}while(1);
			/*Il padre inviera un SIGTERM a questo processo*/
			
			break;
		case -1:
			printf("Error for fork erno:%s\n",strerror(errno));
			break;
		default:
			
			break;
	}
	
	/*Un processo per il print della mappa*/
	switch(secondsupportpid=fork()){
		case 0:
			/*Per il figlio utilizo un handler di terminazione meno complicato*/
			set_handler(SIGTERM,&childsigterm_handler);
			set_handler(SIGINT,&childsigterm_handler);
			set_handler(SIGALRM,&childsigterm_handler);
			/*Ogni 1 secondo stampo la mappa*/
			do{
				timer.tv_sec=1;
				timer.tv_nsec=0;
				nanosleep(&timer,NULL);
				printmap();
			}while(1);
			
			break;
		
		case -1:
			printf("Error for fork erno:%s\n",strerror(errno));
			break;
		default:
			
			break;
	}

	/*Il master si mette in attessa di segnali per la creazione di nuovi taxi*/
	support=maxsecsize();
	printf("I am waiting\n");
	do{
	
		/*Ricerca un messaggio*/
		do{
			sem_wait(sem_id,(SO_WIDTH*SO_HEIGHT-SO_HOLES)*2+4+support);
			msgsupport=msgrcv(q_id[support], &rcvmsg ,MSGSIZETAXI,1, IPC_NOWAIT);
			sem_signal(sem_id,(SO_WIDTH*SO_HEIGHT-SO_HOLES)*2+4+support);
			
		}while(msgsupport==-1);
	
		/*Estrae dal messagio l'indice che rapresenta il pid del figlio nel array taxipids*/
		pidi=rcvmsg.index;
		oldpid=taxipids[pidi];
		/*Devo concedergli la partenza*/
		sem_signal(sem_id,(((SO_WIDTH*SO_HEIGHT-SO_HOLES)*2)+2));
		createtaxi(pidi,0);
		/*Chiudo ufficialmente il processo precedente perchè attualmente è in stato di zombie*/
		while(waitpid(oldpid,&status,0)!=-1){
		
			if (WIFEXITED(status)) {
				/*printf("Il figlio è stato chiuso corettamente %d\n", WEXITSTATUS(status));*/
			}else{
				printf("Errore chiusura Taxi");
			}
			/*printf("Error:%s\n",strerror(errno));*/
		}
	
	}while(1);
	
	
      
}

void createtaxi(int index,int increment){

	int tent,lenght,support,i;

	
	/*Trovo una posizione idonea per il taxi e la prenoto*/
	tent=0;
	do{
		/*Se non trova la posizione per i taxi chiude il programma*/
		if(tent>INSERTTENTATIVES){
			printf("TAXI ECCESSIVI\n");
			shutdown(0);
		}
		randomxycell(mains,res);	
		tent++;
		
	}while(testsem(sem_id,mains->mat[res[0]][res[1]].semind)==-1);
			
	/*Efettuo il fork*/
	switch(taxipids[index]=fork()){
		case 0:
					
			/*printf("I am the taxi child %d\n",getpid());*/
			
			/*Evito di azionare l'handler del padre se l'execve non
			è stato ancora azionata*/	
			set_handler(SIGTERM,&childsigterm_handler);

			
			passargv[0]="taxi";
		
			/*Inserisco la posizione casuale x*/
			lenght= snprintf( NULL, 0, "%d", res[0] );
			passargv[3] = malloc(lenght+1);
					snprintf( passargv[3], lenght+1, "%d", res[0] );
					
			/*Inserisco la posizione casuale y*/
			lenght= snprintf( NULL, 0, "%d", res[1]  );
			passargv[4] = malloc(lenght+1);
			snprintf( passargv[4], lenght+1, "%d", res[1] );
					
			/*inidice processo*/
			lenght= snprintf( NULL, 0, "%d", index );
			passargv[9] = malloc(lenght+1);
			snprintf(passargv[9], lenght+1, "%d", index);
				
			
			/*Genero un altro processo con aree di memoria reinizializate*/
			execve("taxi",passargv,NULL);
			printf("Error for execve erno:%s\n",strerror(errno));
			exit(0);
					
		break;
		case -1:
			printf("Error for fork erno:%s\n",strerror(errno));
		break;
		default:
			/*se richiesto incremento counttaxi*/
			if(increment){
			counttaxi=counttaxi+1;
			printf("counttaxi:%d\n",counttaxi);
			}
		break;
	}

}

void setmap(){
	int numblock,possiblelocation,randompos,onyblock,onxblock,i,j;
	int invalid,countentry;
	int countsem,randomvalue,countnosource;
	int tent;
	
	/*Controllo se le celle source hanno spazio sulla mappa*/
	if(SO_SOURCESGEN>SO_WIDTH*SO_HEIGHT-SO_HOLES){
	printf("CELLE SOURCE ECCESSIVE\n");
	shutdown(0);
	}
	/*Controllo se le top cells hanno spazio sulla mappa*/
	if(SO_TOP_CELLS>(SO_WIDTH*SO_HEIGHT)-SO_HOLES){
	printf("TOP CELLS ECCESSIVE\n");
	shutdown(0);
	}
	
	/*Inizializzo le informazioni di successo*/
	mains->aborted=0;
	mains->successes=0;
	mains->inevasi=0;	
	mains->topattnum=0;
	mains->toptravelnum=0;
	mains->topcallnum=0;
	/*Inizializzo le informazioni delle le celle*/
	for(i=0;i<SO_WIDTH;i++){
		for(j=0;j<SO_HEIGHT;j++){
			mains->mat[i][j].info=0;
			mains->mat[i][j].curtaxi=0;
			mains->mat[i][j].issource=0;
			mains->mat[i][j].totatt=0;
		}
	}
	
	/*Tramite un algoritmo imposto le celle di blocco*/
	possiblelocation=(SO_WIDTH)*(SO_HEIGHT);
	for(numblock=0;numblock<SO_HOLES;numblock++){
	
		/*Controllo le celle adiacenti*/
		tent=0;
		do{
		
		/*Se i tentativi sono eccessivi, il processo non trova ulteriori celle di blocco*/
		if(tent>(INSERTTENTATIVES*2)){
		printf("SO_HOLES ECCESSIVI\n");
		shutdown(0);
		}
		
		
		/*Estraggo una posizione casuale*/
		randompos = rand() % possiblelocation;
		onxblock=(randompos%(SO_WIDTH));
		onyblock=(possiblelocation-(randompos-onxblock))/(SO_WIDTH);
		
		/*Controllo se la posizione estratta è idonea*/
		invalid=0;
		if(onxblock-1>=0 && onyblock-1>=0){
			if(mains->mat[onxblock-1][onyblock-1].info!=0)invalid=1;
		}
		if(onyblock-1>=0){
			if(mains->mat[onxblock][onyblock-1].info!=0)invalid=1;
		}
		if(onxblock+1<=SO_WIDTH && onyblock-1>=0){
			if(mains->mat[onxblock+1][onyblock-1].info!=0)invalid=1;
		}
		if(onxblock-1>=0){
			if(mains->mat[onxblock-1][onyblock].info!=0)invalid=1;
		}
		if(onxblock+1<=SO_WIDTH){
			if(mains->mat[onxblock+1][onyblock].info!=0)invalid=1;
		}
		if(onxblock-1>=0 && onyblock+1<=SO_HEIGHT){
			if(mains->mat[onxblock-1][onyblock+1].info!=0)invalid=1;
		}
		if(onyblock+1<=SO_HEIGHT){
			if(mains->mat[onxblock][onyblock+1].info!=0)invalid=1;
		}
		if(onxblock+1<=SO_WIDTH && onyblock+1<=SO_HEIGHT){
			if(mains->mat[onxblock+1][onyblock+1].info!=0)invalid=1;
		}
		if(mains->mat[onxblock][onyblock].info==1)invalid=1;
		tent++;
		}while(invalid);
		/*Se info è 1 la cella è una SO_HOLES*/
		mains->mat[onxblock][onyblock].info=1;
	
	}
	
	
	/*Definisco un semaforo per ogni cella e le sue impostazioni*/
	countsem=0;
	printf("%d\n",SO_SOURCESGEN);
	for(i=0;i<SO_WIDTH;i++){
		for(j=0;j<SO_HEIGHT;j++){
			if(mains->mat[i][j].info==0){
			/*Capienza massima della cella*/
			randomvalue=SO_CAP_MIN+(rand() % ((SO_CAP_MAX-SO_CAP_MIN)+1));
			printf("%i\n",randomvalue);
			printf("%d\n",countsem);
			/*Un semaforo per contare e impedire ai taxi l'attraversamento della cella*/
			semctl(sem_id,countsem, SETVAL,randomvalue);
			/*Un semaforo per cambiare gli attributi della cella cella in maniera esclusiva*/
			semctl(sem_id,((SO_WIDTH*SO_HEIGHT-SO_HOLES))+countsem, SETVAL,1);
			/*Associo a ogni cella il suo semaforo*/
			mains->mat[i][j].semind=countsem;
			/*Tempo di attraversamento della cella*/
			randomvalue=SO_TIMENSEC_MIN+(rand() % ((SO_TIMENSEC_MAX-SO_TIMENSEC_MIN)));
			printf("%i\n",randomvalue);
			mains->mat[i][j].atttime=randomvalue;
			countsem++;
			}
		}
	}


	/*Trovo la posizione delle sourcegen, se sourcegen è grande trovo prima le celle che 
	NON saranno utilizzate come source*/
	
	if(SO_SOURCESGEN>((SO_WIDTH*SO_HEIGHT-SO_HOLES)/2)){
	
		/*Random delle celle in cui non ci sara la source*/
		for(i=0;i<(SO_WIDTH*SO_HEIGHT-SO_HOLES)-SO_SOURCESGEN;i++){
		res[0]=0;
		res[1]=0;
		randomxycell(mains,res);
			
			if(mains->mat[res[0]][res[1]].issource==1){
				i--;
				printf("Stessa posizione\n");
			}else{
				notsourcepos[1][i]=res[1];
				notsourcepos[0][i]=res[0];
				mains->mat[res[0]][res[1]].issource=1;
				printf("x:%d y:%d\n",notsourcepos[0][i],notsourcepos[1][i]);
				
			}
			
		}
		
		
		
		/*Assegnazione delle celle una dopo l'altra escludendo quelle estratte*/
		
		countentry=0;
		for(i=0;i<SO_WIDTH;i++){
			for(j=0;j<SO_HEIGHT;j++){
				if(mains->mat[i][j].issource==1){
					mains->mat[i][j].issource=0;
				}else{
				if(mains->mat[i][j].info!=1){
					mains->mat[i][j].issource=1;
					sourcepos[0][countentry]=i;
					sourcepos[1][countentry]=j;
					countentry++;
				}
				
				}
			}
		}
	
	}else{
		printf("Sone else\n");
		/*Random delle source*/
		for(countentry=0;countentry<SO_SOURCESGEN;countentry++){
			res[0]=0;
			res[1]=0;
				do{
					randomxycell(mains,res);
				}while(mains->mat[res[0]][res[1]].issource==1);
			mains->mat[res[0]][res[1]].issource=1;
			sourcepos[0][countentry]=res[0];
			sourcepos[1][countentry]=res[1];
			printf("Ho trovato la mia posizione x:%d y:%d\n",sourcepos[0][countentry],sourcepos[1][countentry]);

		}
		
	
	}
} 

void shutdown(int endprint){
	int i;
	int support;
	/*Segnali di terminazione ai figli*/
	for(i=0;i<countsources;i++){
		kill(sourcepids[i], SIGTERM);
	}
	
	for(i=0;i<counttaxi;i++){
		kill(taxipids[i], SIGTERM);
	}

	if(supportpid!=0)kill(supportpid, SIGTERM);
	if(secondsupportpid!=0)kill(secondsupportpid, SIGTERM);

	
	/*Se ho creato dei figli, aspetto la loro terminazione*/
	if(counttaxi>0 || countsources>0 || supportpid!=0 || secondsupportpid!=0)while(wait(NULL)!=-1);
	printf("Childs terminated \n");

	

	/*La shm si chiuderà automaticamente quando tutti i processi effettuerano il suo dettach*/
	
	/*Rimuovo le code di messaggi*/
	for(i=0;i<maxsecsize();i++){
		msgctl(q_id[i], IPC_RMID , 0);
	}
	support=maxsecsize();
	msgctl(q_id[support], IPC_RMID , 0);
	
	/*Rimuovo l'array di semafori*/
	semctl(sem_id,IPC_RMID,0);
	
	/*Se richiesto stampo le statistiche finali*/
	if(endprint==1){
	enddata();
	}
	
	/*Libero la memoria condivisa, verrà eleminata perchè all'inizio ho gia richiesto l'eleminazione*/
	shmdt(mains);
	
	/*Libero le aree di memoria precedentemente alocate*/
	free(sourcepids);
	free(taxipids);
	free(sourcepos[0]);
	free(sourcepos[1]);
	free(notsourcepos[0]);
	free(notsourcepos[1]);
	for(i=0;i<totalargs;i++){
		free(passargv[i]);
	}
	free(q_id);
	
	exit(0);
}

void enddata(){
	/*x,y e il loro valore*/
	int * topcels[3];
	int minpos;
	int minvalue;
	int i,j,k;
	printf("tops:%d\n",SO_TOP_CELLS);
	topcels[0]=calloc(SO_TOP_CELLS,sizeof(int));
	topcels[1]=calloc(SO_TOP_CELLS,sizeof(int));
	topcels[2]=calloc(SO_TOP_CELLS,sizeof(int));
	
	for(i=0;i<SO_TOP_CELLS;i++){
	topcels[0][i]=0;
	topcels[1][i]=0;
	topcels[2][i]=0;
	}
	
	
	
	/*Trovo le celle SO_TOP_CELLS con percorenza magiore*/
	if(SO_TOP_CELLS>0){
		for(i=0;i<SO_WIDTH;i++){
			for(j=0;j<SO_HEIGHT;j++){
				
				if(mains->mat[i][j].info!=1){
				
					/*Trovo la cella con percorenza minore tra l'array delle celle considerate TOP_CELL*/
					minvalue=topcels[2][0];
					minpos=0;
					for(k=0;k<SO_TOP_CELLS;k++){
					
						if(topcels[2][k]<minvalue){
						minpos=k;
						minvalue=topcels[2][k];
						}

					}
					
					/*se l'elemento i j ha percorso di più lo inserisco al posto 
					dell'elemento più piccolo del vettore delle top cell*/
					if(mains->mat[i][j].totatt>minvalue){
						
						topcels[0][minpos]=i;
						topcels[1][minpos]=j;
						topcels[2][minpos]=mains->mat[i][j].totatt;
					}
				}
				
			}
			
		}
		
		/*Segnalo le TOP_CELL*/
		for(i=0;i<SO_TOP_CELLS;i++){
			mains->mat[topcels[0][i]][topcels[1][i]].info=4;
		}
	}
	/*Stampo lo mappa con le top cell individuate*/
	printf("X: Source and Topcell\n"); 	
	for(i=0;i<SO_HEIGHT;i++){
		for(j=0;j<SO_WIDTH;j++){
		
			switch(mains->mat[j][i].info){
			case 1:
				printf("#");
				break;
			case 4:
				if(mains->mat[j][i].issource){
				printf("X");
				}else{
				printf("T");
				
				}
				break;
			default:
				if(mains->mat[j][i].issource){
				printf("S");
				}else{
				printf(" ");
				}
				break;
			}
			
		   }
		   printf("\n");
	}
	
	/*Stampo i parametri di successo*/
	printf("Successi:%d\n",mains->successes);
	printf("Aborted:%d\n",mains->aborted);
	printf("Inevasi:%d\n",mains->inevasi);
	printf("Il proccesso con pid:\n");
	printf("> %d ha fatto più strada in assoluto attraversando %d celle\n",mains->topatt,mains->topattnum);
	printf("> %d ha fatto il viaggio più lungo attraversando %d celle\n",mains->toptravel,mains->toptravelnum);
	printf("> %d ha ricevuto più richieste totalizzando %d richieste\n",mains->topcall,mains->topcallnum);	
	free(topcels[0]);
	free(topcels[1]);
	free(topcels[2]);

}

void printmap(){
 	int i,j,sumtaxi;
 	int support;
 	int printsource;
 	sigset_t old_mask;
 	printsource=0;
 	
 	support=0;
 	sumtaxi=0;
 	
 	old_mask=maskprocces();
 	/*Aspetto che tutti i processi finiscano di incrementare/decrementare curtaxi*/
 	sem_cmd(sem_id,(SO_WIDTH*SO_HEIGHT-SO_HOLES)*2,-SO_TAXI,0);
	printf("\n");
	/*Stampo le informazioni di ogni cella*/
	for(i=0;i<SO_HEIGHT;i++){
	   for(j=0;j<SO_WIDTH;j++){
		switch(mains->mat[j][i].info){
		case 0:
		if(mains->mat[j][i].issource  && printsource ){
		printf("S");
		}else{
		printf(" ");
		}
		break;
		case 1:
		/*Per verificare se non ci sono sovraposizioni*/
		if(mains->mat[j][i].issource  && printsource ){
		printf("S");
		}else{
		printf("#");
		}
		break;
		case 2:
		if(mains->mat[j][i].issource  && printsource ){
		printf("S");
		}else{
		support=mains->mat[j][i].curtaxi;
		printf("%d",support);
		sumtaxi=support+sumtaxi;
		}
		break;
		case 6:
		printf("%d",whatismysec(j));
		
		break;
		}
		
	   }
	   printf("\n");
	}
	/*Concedo ai processi di incrementare/decrementare curtaxi*/
	sem_cmd(sem_id,(SO_WIDTH*SO_HEIGHT-SO_HOLES)*2,SO_TAXI,0);
	reset_signals(old_mask);
 	
 	
	
	printf("sumtaxi:%d\n",sumtaxi);

	
}



