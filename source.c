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

mainstructure * mains;

int * q_id;
int mx,my,sem_id;
int noholes;

void sigterm_handler(int signum);
void sigalarm_handler(int signum);
int send(int mask);

int main (int argc, char * argv[]) {

	
	
	int shm_id,SO_HOLES;
	struct timespec timer;
	clock_t before;
	clock_t difference;
	int mutexindex,i;
	sigset_t old_mask;
	
	printf("I am the source child %d\n",getpid());
	
	set_handler(SIGTERM,&sigterm_handler);
	set_handler(SIGALRM,&sigalarm_handler);
	srand ( time(NULL) ^ (getpid()<<16));
	
	q_id=calloc(maxsecsize(),sizeof(int));
	
	timer.tv_sec=MESSAGETIMES;
	timer.tv_nsec=MESSAGETIMEN;
	shm_id=atoi(argv[1]);
	sem_id=atoi(argv[2]);
	mx=atoi(argv[3]);
	my=atoi(argv[4]);
	SO_HOLES=atoi(argv[5]);
	noholes=SO_WIDTH*SO_HEIGHT-SO_HOLES;
	
	for(i=0;i<maxsecsize();i++){
	q_id[i]= atoi(argv[10+i]);
	}
	
	mains= shmat(shm_id,NULL,0);
	
	sem_signal(sem_id,((noholes)*2)+1);

	sem_wait(sem_id,((noholes)*2)+2);

	/*Invio delle richieste*/
	
	before=clock();
	do{
	/*Se la coda è piena riaspetta un secondo*/
	send(1);
	nanosleep(&timer,NULL);
	}while(1);
	
}

void sigterm_handler(int signum){
	/*printf("Source Kill");*/
	shmdt(mains);
	free(q_id);
	exit(0);
}

int send(int mask){
	int res[2];
	int suport;
	int mysec;
	sigset_t old_mask;
	struct msgbuf sendbuf;
	suport=0;
	mysec=whatismysec(mx);
	randomxycell(mains,res);
	sendbuf.myposx=(unsigned char)mx;
	sendbuf.myposy=(unsigned char)my;
	sendbuf.mtype=1;
	sendbuf.destx=(unsigned char)res[0];
	sendbuf.desty=(unsigned char)res[1];
	if(mask)old_mask=maskprocces();
	sem_wait(sem_id,((noholes)*2)+4+mysec);
	suport=msgsnd(q_id[mysec], &sendbuf, MSGSIZE,IPC_NOWAIT);
	sem_signal(sem_id,((noholes)*2)+4+mysec);
	if(mask)reset_signals(old_mask);
	
	if(suport==-1){
	/*printf("In sec:%ld errore %s\n",(long)mysec,strerror(errno));*/
	}else{
	/*printf("Send in sec:%ld \n",(long)mysec);*/
	}
	return suport;
}

void sigalarm_handler(int signum){
        int suport;
        /*Solamente se la coda non è piena*/
        /*il handler è già masked*/
	suport=send(0);
	
}


