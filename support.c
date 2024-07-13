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
#include <math.h>

void function(){
printf("ha");
}


int sem_wait(int s_id, int sem_num) {
	struct sembuf sops;
	sops.sem_flg = 0;
	sops.sem_op = -1;
	sops.sem_num = sem_num;
	return semop(s_id, &sops, 1);
}

int sem_signal(int s_id, int sem_num) {
	struct sembuf sops;
	sops.sem_flg = 0;
	sops.sem_op = 1;
	sops.sem_num = sem_num;
	return semop(s_id, &sops, 1);
}

int sem_cmd(int s_id, unsigned short sem_num,int nops,int flags) {
	struct sembuf sops;
	sops.sem_flg = flags;
	sops.sem_op = nops;
	sops.sem_num = sem_num;
	return semop(s_id, &sops, 1);
}

int testsem(int sem_id,int index){
	
	struct sembuf sops;
	sops.sem_flg = IPC_NOWAIT;
	sops.sem_op = -1;
	sops.sem_num = index;
	return semop(sem_id, &sops, 1);

}


int whatismysec(int test){
	
	int x;
	if(SO_WIDTH>=SECTIONSIZE){
	x=test/SECTIONSIZE;
	if(test/SECTIONSIZE==SO_WIDTH/SECTIONSIZE && test%SECTIONSIZE>=0){
		x--;
		if(SO_WIDTH%SECTIONSIZE>=3){
			x++;
		}
	}
	
	return x;
	}else{
	return 0;
	}
	
}

int maxsecsize(){

	int x;
	/*from 1 to n-1*/
	if(SO_WIDTH>=SECTIONSIZE){
	x=SO_WIDTH/SECTIONSIZE;
	/*if necesary adding 1*/
	if(SO_WIDTH%SECTIONSIZE>=3){
	x++;
	}
	/*parte da 1,deve partire da 0*/
	return x;
	}else{
	return 1;
	}

}

void randomxycell(mainstructure * mains,int * res){
	
	int onxblock,onyblock;

	
	do{
	onxblock=rand() % SO_WIDTH;
	onyblock=rand() % SO_HEIGHT;
	}while(mains->mat[onxblock][onyblock].info==1);
	res[0]=onxblock;
	res[1]=onyblock;
	

}

sigset_t maskprocces(){
	sigset_t mask, old_mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGINT);
	sigprocmask(SIG_BLOCK, &mask, &old_mask);
	return old_mask;
}


void reset_signals(sigset_t old_mask) {
	sigprocmask(SIG_SETMASK, &old_mask, NULL);
}

struct sigaction set_handler(int sig, void (*func)(int)) {
	struct sigaction sa, sa_old;
	sigset_t mask;
	sigemptyset(&mask);
	sa.sa_handler = func;
	sa.sa_mask = mask;
	sa.sa_flags = 0;
	sigaction(sig, &sa, &sa_old);
	return sa_old;
}


