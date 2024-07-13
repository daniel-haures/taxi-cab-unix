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

int shm_id,sem_id;
int * q_id;
mainstructure * mains;

int noholes;

int totatt;
int actualatt;
int maxatt;
int inevaso;
int richieste;

void sigterm_handler(int signum);

void shutdown();

void reset(int mx,int my,char ** passargvs,int index,int addaborted);

int main (int argc, char * argv[]) {

	
	int SO_HOLES,SO_TIMEOUT,SO_TAXI;
	sigset_t old_mask;
	struct sembuf sops;
	struct msgbuf rcvbuf;
	struct timespec timer;
	time_t start, stop;

	int res[2];
	int lenght;
	int returnmsg;
	int mx;
	int my;
	int mutexindex;
	int mysec;
	int mysecy;
	int i,j,support;
	int destinations[2][2];
	int offsetx,offsety,targetx,targety,consideredoff,consideredaxis;
	int direction,lastdirection;
	int index;
	
	/*Per evitare di ottenere gli stessi numeri con la random*/
	srand ( time(NULL) ^ (getpid()<<16));
	
	/*
	printf("I am the exe child %d\n",getpid());
	printf("Last %d\n",getpid());
	*/
	totatt=0;
	actualatt=0;
	maxatt=0;
	inevaso=0;
	
	/*Importo e converto le variabili passate come argomento*/
	shm_id=atoi(argv[1]);
	sem_id=atoi(argv[2]);
	mx=atoi(argv[3]);
	my=atoi(argv[4]);
	SO_HOLES=atoi(argv[5]);
	SO_TIMEOUT=atoi(argv[6]);
	SO_TAXI=atoi(argv[7]);
	mains= shmat(shm_id,NULL,0);
	index=atoi(argv[9]);
	
	q_id=calloc(maxsecsize()+1,sizeof(int));
	
	for(i=0;i<maxsecsize();i++){
	q_id[i]= atoi(argv[10+i]);
	}
	
	support=maxsecsize();
	q_id[support]=atoi(argv[8]);
	support=0;
	
	/*Il master inviera un sigterm per la chiusara che dovra essere gestito corettamente*/
	
	
	noholes=SO_WIDTH*SO_HEIGHT-SO_HOLES;
	
	
	/*inc curtaxi*/
	/*un solo processo alla volta può incrementare curtaxi*/
	mutexindex=(noholes)+mains->mat[mx][my].semind;
	old_mask=maskprocces();
	sem_wait(sem_id,(noholes)*2);
	sem_wait(sem_id,mutexindex);
	mains->mat[mx][my].curtaxi++;
	mains->mat[mx][my].info=2;
	sem_signal(sem_id,mutexindex);
	sem_signal(sem_id,(noholes)*2);
	reset_signals(old_mask);
	
	/*Informo il padre che ho finito l'inizializzazione*/
	sem_signal(sem_id,(noholes*2)+1);

	/*Aspetto di ricevere il segnale di partenza dal padre*/
	sem_wait(sem_id,((noholes)*2)+2);

	richieste=0;
	set_handler(SIGTERM,&sigterm_handler);
	while(1){
	
	/*Tempo inizio ricezione rischieste*/
	start = time(NULL);
	
	do{
		mysec=whatismysec(mx);
		/*printf("start:%d\n",whatismysec(mx));*/
		stop = time(NULL);
		if((stop - start)>=SO_TIMEOUT){
			/*Si puo disabilitare la chiusura dei processi che non ricevono richieste dal common*/
			if(KILLIFNOREQUEST)reset(mx,my,argv,index,0);
		}
		
		if(SO_TAXI<20){
		/*Se ci sono pocchi taxi c'è probabilità che la source di una sezione rimanga inservita,
		perciò un taxi deve cercare anche nelle celle adiacenti*/
			for(i=0;i<maxsecsize();i++){
				for(j=0;j<2;j++){
					if(j==0){
						if(mysec+i<maxsecsize()){
							old_mask=maskprocces();
							sem_wait(sem_id,((noholes)*2)+4+(mysec+i));
							support=msgrcv(q_id[mysec+i], &rcvbuf ,MSGSIZE,1, IPC_NOWAIT);
							sem_signal(sem_id,((noholes)*2)+4+(mysec+i));
							reset_signals(old_mask);
							
							if(support!=-1){
								j=2;
								i=maxsecsize();
							}
						}
						
					}else{
					
						if(mysec-i>=0){
							old_mask=maskprocces();
							sem_wait(sem_id,((noholes)*2)+4+(mysec-i));
							support=msgrcv(q_id[mysec-i], &rcvbuf ,MSGSIZE,1, IPC_NOWAIT);
							sem_signal(sem_id,((noholes)*2)+4+(mysec-i));
							reset_signals(old_mask);
							
							if(support!=-1){
								i=maxsecsize();
								j=2;
							}
						}
					
					}
				}
			
			}
		}else{
			/*Se ci sono tanti taxi è meglio evitare che possano raccogliere richieste in altre sezioni,
			c'è una sudduvisione migliore*/
			
			/*Il processo non deve essere internoto da segnali durante il prelevamento del messaggio,
			perchè altrimenti verà provocato un deadlock*/
			old_mask=maskprocces();
			/*Un processo alla volta preleva i messaggi*/
			sem_wait(sem_id,((noholes)*2)+4+(mysec));
			support=msgrcv(q_id[mysec], &rcvbuf ,MSGSIZE,1, IPC_NOWAIT);
			sem_signal(sem_id,((noholes)*2)+4+(mysec));
			reset_signals(old_mask);
		}
			
		/*Se dall'inizio della ricezione delle richieste interccorre più di SO_TIMEOUT tempo 
		richiedo di chiudere questo processo e riaprirne un altro*/
		

	}while(support==-1);
	
	/*printf("Incarico sx:%d sy:%d verso destx:%d desty:%d\n",(int)rcvbuf.myposx,(int)rcvbuf.myposy,(int)rcvbuf.destx,(int)rcvbuf.desty);*/
	richieste++;
	inevaso=1;
	actualatt=0;
	
	/*Registro le informazioni del messaggio*/
	destinations[0][0]=(int)rcvbuf.myposx;
	destinations[0][1]=(int)rcvbuf.myposy;
	destinations[1][0]=(int)rcvbuf.destx;
	destinations[1][1]=(int)rcvbuf.desty;
	lastdirection=rand() % 2;
	/*Un for permette di arrivare prima alla destinazione 0 e poi alla destinazione 1*/
	for(i=0;i<2;i++){
	        
		do{
		/*prima di di il movimento attraverso la cella*/
		timer.tv_nsec=mains->mat[mx][my].atttime;
		timer.tv_sec=0;
		nanosleep(&timer,NULL);
		/*Incremento le celle attraversate in questa richiesta*/
		actualatt++;
		totatt++;
		/*misurando l'offset tra destinazione e posizione definisco la mia direzione*/
		/*0=sinistra 1=alto 2=destra 3=basso*/
		offsetx=mx-destinations[i][0];
		offsety=my-destinations[i][1];
		if(offsetx!=0 || offsety!=0){
			/*Se sono sulla stessa riga collona vado nella direzione della destinazione*/
			if(offsetx==0){
				if(offsety<0){
					direction=3;
				}else{
					direction=1;
				}
			
			}
			if(offsety==0){
				if(offsetx<0){
					direction=2;
				}else{
					direction=0;
				}
			}
			/*Se non sono sulla stessa riga/colonna evito di andare nella stessa direzione di prima*/
			if(offsetx!=0 && offsety!=0){
				if(lastdirection==0 || lastdirection==2){
					if(offsety<0){
					direction=3;
					}else{
					direction=1;
					}
				}
				if(lastdirection==1 || lastdirection==3){
					if(offsetx<0){
					direction=2;
					}else{
					direction=0;
					}
				}
			}
		}
		
		/*controllo se ci sono buchi nella direzione presa, e se ci sono modifico la direzione
		verso la destra o sinistra del buco a seconda dell'offset*/
		switch(direction){
		case 0:
			if(mains->mat[mx-1][my].info==1){
				if(offsety!=0){
					if(offsety<0){
					direction=3;
					}else{
					direction=1;
					}
				}else{
					/*devo scegliere a caso*/
					if((rand()%2)==0){
						direction=3;
					}else{
						direction=1;   
					}
					/*Evito di andare fuori mappa*/
					if(my+1>(SO_HEIGHT-1))direction=1;
					if(my-1<0)direction=3;
				}
			}
		break;
		case 1:
			if(mains->mat[mx][my-1].info==1){
				if(offsetx!=0){
					if(offsetx<0){
						direction=2;
					}else{
						direction=0;
					}
				}else{
					/*devo scegliere a caso*/
					if((rand()%2)==0){
						direction=2;
					}else{
						direction=0;
					}
					/*Evito di andare fuori mappa*/
					if(mx+1>(SO_WIDTH-1))direction=0;
					if(mx-1<0)direction=2;
				
				}
			}
		break;
		case 2:
			if(mains->mat[mx+1][my].info==1){
				if(offsety!=0){
					if(offsety<0){
					direction=3;
					}else{
					direction=1;
					}
				}else{
					/*devo scegliere a caso*/
					if((rand()%2)==0){
						direction=3;
					}else{
						direction=1;   
					}
					if(my+1>(SO_HEIGHT-1))direction=1;
					if(my-1<0)direction=3;
				}
			}
		break;
		case 3:
			if(mains->mat[mx][my+1].info==1){
				if(offsetx!=0){
					if(offsetx<0){
						direction=2;
					}else{
						direction=0;
					}
				}else{
					/*devo scegliere a caso*/
					if((rand()%2)==0){
						direction=2;
					}else{
						direction=0;
					}
					if(mx+1>(SO_WIDTH-1))direction=0;
					if(mx-1<0)direction=2;
				
				}
			}
		break;
		}
		
		/*Registro la mia scelta per il movimento successivo*/
		lastdirection=direction;
		
		/*definisco la prosima casella da ocupare in base alla direzione*/
		switch(direction){
		case 0:
		targetx=mx-1;
		targety=my;
		break;
		case 1:
		targetx=mx;
		targety=my-1;
		break;
		case 2:
		targetx=mx+1;
		targety=my;
		break;
		case 3:
		targetx=mx;
		targety=my+1;
		break;
		}
		
		
		/*Una volta concesso il permesso modifico il numero totale di attraversamenti della cella*/
		old_mask=maskprocces();
		sem_wait(sem_id,(noholes)+mains->mat[mx][my].semind);
		mains->mat[mx][my].totatt++;
		sem_signal(sem_id,(noholes)+mains->mat[mx][my].semind);
		reset_signals(old_mask);
		
		
		/*Prenoto la prossima cella*/
		sops.sem_flg = 0;
		sops.sem_op = -1;
		sops.sem_num = mains->mat[targetx][targety].semind;
		timer.tv_nsec=0;
		timer.tv_sec=SO_TIMEOUT;
		/*Se entro SO_TIMEOUT non ricevo la cella richiedo di eliminare questo taxi e di crearne un altro*/
		support=semtimedop(sem_id, &sops, 1, &timer);
		if(support==-1 && errno==EAGAIN){
			reset(mx,my,argv,index,1);
		}
		
		/*Se ho ricevuto la nuova cella decremento curtaxi della vecchia cella*/
		/*Incremento la nuova e decremento la vecchio in maniera atomica al print della mappa*/
		old_mask=maskprocces();
		sem_wait(sem_id,(noholes)*2);
		sem_wait(sem_id,(noholes)+mains->mat[mx][my].semind);
		mains->mat[mx][my].curtaxi--;
		if(mains->mat[mx][my].curtaxi==0)mains->mat[mx][my].info=0;
		sem_signal(sem_id,(noholes)+mains->mat[mx][my].semind);

		sem_wait(sem_id,(noholes)+mains->mat[targetx][targety].semind);
		mains->mat[targetx][targety].curtaxi++;
		mains->mat[targetx][targety].info=2;
		sem_signal(sem_id,(noholes)+mains->mat[targetx][targety].semind);
		sem_signal(sem_id,(noholes)*2);
		reset_signals(old_mask);
		
		/*decremento il semaforo della vecchio cella rendendola disponibile a un altro processo*/
		sem_signal(sem_id,mains->mat[mx][my].semind);
		mx=targetx;
		my=targety;
		/*Ripeto il movimento finché non ragiungo la mia destinazione*/
		}while(mx!=destinations[i][0] || my!=destinations[i][1]);
	
	}
	inevaso=0;
	/*Se ho fatto un nuovo record per le celle attraversate, lo memorizzo*/
	if(maxatt<actualatt)maxatt=actualatt;
	
	/*Aumento i successi*/
	old_mask=maskprocces();
	/*Questa parte la devo mascherare perchè il semaforo in questione verrà utilizzato nell'handler*/
	sem_wait(sem_id,(SO_WIDTH*SO_HEIGHT-SO_HOLES)*2+3);
	mains->successes++;
	sem_signal(sem_id,(SO_WIDTH*SO_HEIGHT-SO_HOLES)*2+3);
	reset_signals(old_mask);
	
	}
	
}

void shutdown(){
	/*printf("Sto per morire");*/
	
	/*Registro e confronto le statische del processo taxi con quelle già esistenti*/
	sem_wait(sem_id,(noholes)*2+3);
	if(inevaso)mains->inevasi++;
	if(mains->topattnum<totatt){
	mains->topattnum=totatt;
	mains->topatt=getpid();
	}
	if(mains->topcallnum<richieste){
	mains->topcallnum=richieste;
	mains->topcall=getpid();
	}
	if(mains->toptravelnum<maxatt){
	mains->toptravelnum=maxatt;
	mains->toptravel=getpid();
	}
	sem_signal(sem_id,(noholes)*2+3);
	
	
}

void sigterm_handler(int signum){
	shutdown();
	free(q_id);
	shmdt(mains);
	exit(0);
}


void reset(int mx,int my,char ** passargvs,int index,int addaborted){
	int res[2];
	int lenght;
	int support,supportmsg;
	int a;
	struct msgtaxi sendmsg;
	sigset_t old_mask;
	
	/*Libero la mia cella decrementando curtaxi ed effettuado un signal sul semafore della cella occupata*/
	old_mask=maskprocces();
	sem_wait(sem_id,(noholes)*2);
	sem_wait(sem_id,(noholes)+mains->mat[mx][my].semind);
	mains->mat[mx][my].curtaxi--;
	if(mains->mat[mx][my].curtaxi==0)mains->mat[mx][my].info=0;
	sem_signal(sem_id,(noholes)+mains->mat[mx][my].semind);
	sem_signal(sem_id,(noholes)*2);
	reset_signals(old_mask);
	
	sem_signal(sem_id,mains->mat[mx][my].semind);
		
	/*Nel caso in cui il processo stava effettuando una richiesta incremento il numero di processi aborted*/	
	if(addaborted){
		/*questo semaforo serve agli altri processi in esecuzione, non possiamo interromperci a metà*/
		old_mask=maskprocces();
		sem_wait(sem_id,(noholes)*2+3);
		mains->aborted++;
		sem_signal(sem_id,(noholes)*2+3);
		reset_signals(old_mask);
	}
	
	/*Invio al padre la richiesta di creare un nuovo figlio taxi*/
	sendmsg.mtype=1;
	sendmsg.index=index;
	support=maxsecsize();	
	
	/*Invio un messaggio appena è disponibile un posto nella cosa*/
	/*Posso anche non uttilizare ipc_nowait, devo mandare solo un messaggio da questo processo*/
	/*Non deve usare mask perchè l'unico segnale che può interferire è quello di terminazione*/
	
	supportmsg=msgsnd(q_id[support], &sendmsg, MSGSIZETAXI,0);
	

	
	/*chiudo il processo*/
	free(q_id);
	shmdt(mains);
	exit(0);
}


