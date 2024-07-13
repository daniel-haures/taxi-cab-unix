

#ifndef DENSE
#define SO_WIDTH 60
#define SO_HEIGHT 20
#else
#define SO_WIDTH 20
#define SO_HEIGHT 10
#endif

#define MSGSIZE 4

#define MSGSIZETAXI 2

#define SECTIONSIZE 10

#define MESSAGETIMEN 200000000
#define MESSAGETIMES 0

#define INSERTTENTATIVES 20

#define KILLIFNOREQUEST 1

/*Struttura messagio richiesta servizio taxi*/
struct msgbuf{
long  mtype;
unsigned char myposx;
unsigned char myposy;
unsigned char destx;
unsigned char desty;
};

/*Struttura messagio richiesta nuovo taxi*/
struct msgtaxi{
long  mtype;
short int index;
};

/*Struttura della cella*/
typedef struct {
int semind;
int info; 
int atttime;
int curtaxi;
int issource;
int totatt;
/*capacity detterminata dal semaforo*/
}cell;


typedef struct {
cell mat[SO_WIDTH][SO_HEIGHT];
int aborted;
int successes;
int inevasi;
pid_t topatt;
int topattnum;
pid_t toptravel;
int toptravelnum;
pid_t topcall;
int topcallnum;
}mainstructure;


void function();
int sem_wait(int s_id, int sem_num);
int sem_signal(int s_id, int sem_num);
int sem_cmd(int s_id, unsigned short sem_num,int nops,int flags);
void randomxycell(mainstructure * mains,int * res);
int testsem(int sem_id,int index);
int maxsecsize();
int whatismysec(int test);
struct sigaction set_handler(int sig, void (*func)(int));
void reset_signals(sigset_t old_mask);
sigset_t maskprocces();


