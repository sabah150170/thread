// compile with " g++ deneme.c -o g -pthread"
// BUSE NUR SABAH 150170002
// 24.04.20

#include<sys/ipc.h> 
#include<sys/shm.h>
#include<sys/types.h> 
#include<sys/sem.h>
#include<sys/errno.h> 
#include<sys/stat.h> // S_IRUSR and S_IWUSR
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<signal.h>
#include<unistd.h>	   // fork
#include<sys/wait.h>  // wait()
#include<math.h>     // round()
#include<pthread.h>

#define GNU_SOURCE
#define SHMKEYPATH "/dev/null"
#define SEMKEYPATH "/dev/null"

typedef struct myStruct2{ // for thread
	int min;
	int max;
}thread_data;

typedef struct myStruct3 { // for shared memory
	int primeN;
	struct myStruct3* down=NULL;
}mainList;
mainList* shared[5];
mainList* thread=NULL;

void* findPrime(void* arg); // for thread

void sem_signal ( int semid , int val ){ 
	  struct sembuf semaphore; 
	  semaphore.sem_num=0; 
	  semaphore.sem_op=val;
	  semaphore.sem_flg =1; // relative sem_op
	  semop( semid , &semaphore , 1);
} 

void sem_wait ( int semid , int val ){ 
	 struct sembuf semaphore; 
	 semaphore.sem_num=0; 
	 semaphore.sem_op=(-1*val); 
	 semaphore.sem_flg =1;  // IPC_NOWAIT
	 semop( semid , &semaphore , 1); 
}

void mysignal(int signum){
	return;
}

void mysigset ( int num){ 
	struct sigaction mysigaction; 
	mysigaction.sa_handler=mysignal; 
	mysigaction.sa_flags =0; 
	sigaction (num,&mysigaction ,NULL);
}

int main(int argc, char *argv[])  {
	printf("Master: Started.\n");
	mysigset (12); perror("-------sigset:");
	int cid, i, termSem, termSem2, processMin, processMax, threadMax, threadMin;
	pid_t f, child[atoi(argv[3])];
	bool test;
	int interval=int(round((atof(argv[2])-atof(argv[1]))/atof(argv[3]))); // find (max-min)/np and round it
	key_t KEYSHM=ftok(SEMKEYPATH,111);	if ( KEYSHM == (key_t)-1 ) 	printf("-------fail keyshm\n"); // if you see invalid arg change number
	key_t KEYSEM2=ftok(SEMKEYPATH,222);	if ( KEYSEM2 == (key_t)-1 )	printf("-------fail keysem2\n");
	
	for(int k=0; k<5; k++) shared[k]=NULL;

	for(i=0; i<atoi(argv[3]); i++){ // generate forks
		processMin=atoi(argv[1]);
		processMin+=(i*interval);
		processMax=processMin+interval-1;
		if(i==atoi(argv[3])-1) processMax=atoi(argv[2]);
		f=fork();
		if(f==-1) printf("UNSUCCESS\n");
		else if (f>0) child[i]=f;
		else if(f==0) break;
	}
	
	if(f>0){ // main process, do not create semaphore before fork()
		termSem = semget (KEYSEM2, 1 , IPC_CREAT|0666);  perror("-------semget parent:"); // create new semaphore, IPC_EXCL: file, middle parameter is how many semaphores are there
		//termSem = semget (KEYSEM2, 1 , IPC_CREAT);  perror("-------semget parent:"); // if you see permission denied, run this
		termSem2= semctl (termSem , 0 , SETVAL, 0); perror("-------semctl parent:"); // second parameter is which semaphore is setting
		printf("-------semaphore value of parent: %d\n",termSem2); 
		printf("-------semafor id of parent: %d\n",termSem);

		cid=shmget(KEYSHM, 5*sizeof(mainList), IPC_CREAT|0666| S_IRUSR | S_IWUSR);	perror("-------shmget parent:"); // shared memory is separated with above code, IPC_EXCL: alan zaten varsa hata
		//cid=shmget(KEYSHM, sizeof(mainList), IPC_CREAT| S_IRUSR | S_IWUSR);	perror("-------shmget parent:"); // if you see permission denied, run this
		shared[5]=(mainList*)shmat(cid,NULL,0);  perror("-------shmat parent:"); // shared memory is linked to virtual memory of procces
		int size;
		struct shmid_ds buffer;
		shmctl(cid, IPC_STAT, &buffer);
		size=buffer.shm_segsz;
		printf("-------size of shared: %d\n", size);

		printf("-------memory id of parent: %d\n",cid);
		
		/*sleep(1);
		for ( int k =0; k <atoi(argv[3]); k++) {
			kill( child[k] ,12);  perror("-------kill:");// send signal to pause
			printf("-------sended signal to Slave:%d\n", k);
		}*/

		sem_wait(termSem ,atoi(argv[3])); perror("-------sem_wait:");// ? it should wait to sem_signal, bu it did not.
		printf("Master: Done. Prime numbers are:");

		for(int k=0; shared[k]!=NULL; k++){ // print prime numbers
			while(shared[k]!=NULL){
				printf("%d, ",shared[k]->primeN);
				shared[k]=shared[k]->down;
			}
		}

		for(int k=0; k<5; k++) shmdt(shared[k]); // let it go
		semctl (termSem ,0 ,IPC_RMID ,0); perror("-------semctl parent LAST:");
		shmctl (cid , IPC_RMID ,0); perror("-------shmctl parent LAST:");// manage shared memory, deallocate the shared memory segment
		exit(0);
	}

	else if(f==0) { // child processes
			sleep(4);
			//pause(); perror("-------pause:"); // wait for signal
			int intervalThread =int(round((processMax-processMin)/atof(argv[4]))); // (max-min)/nt
			termSem = semget(KEYSEM2, 1 , 0); perror("-------semget child:"); // to attach to an existing semaphore
			termSem2=semctl( termSem ,0 ,GETVAL,0); perror("-------semctl child:"); 

			printf("Slave %d: Started. Interval %d-%d\n", i+1, processMin, processMax);
			printf("-------semaphore id of child%d: %d\n",i+1,termSem);
			printf("-------semephore value of child%d: %d\n",i+1,termSem2);  // semaphore value, start 0
			
			cid=shmget(KEYSHM, sizeof(mainList), 0);  perror("-------shmget child:"); // shared memory is created with above code
			shared[i]=(mainList*)shmat(cid,0,0); perror("-------shmat child:");  // every child is added segment
			printf("-------memory id of child%d: %d\n",i+1,cid);
			
			pthread_t thread_id[atoi(argv[4])];
			for(int j=0; j<atoi(argv[4]); j++){ // generate threads
				threadMin=processMin;
				threadMin+=(j*intervalThread);
				threadMax=threadMin+intervalThread-1;
				if(j==atoi(argv[4])-1) threadMax=processMax;
				sleep(1);
				thread_data prime;
				prime.min=threadMin;
				prime.max=threadMax;
				printf("-------thread id of %d.%d: %d\n", i+1,j+1, thread_id[j]);
				pthread_create(&thread_id[j], NULL, findPrime, (void*)&prime); perror("-------pthread_create"); 
				printf("Thread %d.%d: searching in %d-%d\n", i+1, j+1, threadMin, threadMax);
				pthread_join(thread_id[j], NULL); perror("-------pthread_join:"); // wait to working thread	
				printf("Thread %d.%d: Done\n", i+1, j+1);
			}
			/*for(int r=0; r<atoi(argv[4]); r++)	{ // it prevent threads work properly
				if(pthread_join(thread_id[r], NULL)) printf("hata2"); // wait to working thread	
				printf("Thread %d.%d: Done\n", i+1, r+1);
				
			}*/
			for(int k=0; thread!=NULL; k++){
				shared[i]=thread;
				shared[i]=shared[i]->down;
				thread=thread->down;
			}

			printf("Slave %d: Done\n ", i+1);
			shmdt(shared[i]);  // cut connection
			sem_signal(termSem, 1); perror("-------sem_signal:");	
			exit(0);
	} 
	return 0;
}


void *findPrime(void* arg){
	thread_data *prime= (thread_data*) arg;
	int test=1;
	while(prime->min <= prime->max){
		test=1;
		for(int m=2; m<=prime->max/2; m++){
			if(prime->min%m==0) {
				test=0;
				break;
			}
		}						
		if(test) {
			mainList* Node=(mainList*)malloc(sizeof(mainList));
			Node->primeN=prime->min;

			if(thread==NULL) thread=Node; 

			else if(thread->primeN>Node->primeN) { // add top
				mainList* temp;
				temp=thread;
				thread=Node;
				thread->down=temp;
			}
			else{ // add middle or last
				mainList* temp;
				temp=thread;
				test=1;
				while(temp->down!=NULL){
					if(temp->down->primeN>Node->primeN) break;
					temp=temp->down;
				}
				mainList* temp2;
				temp2=temp->down;
				temp->down=Node;
				Node->down=temp2;
			}
		}
		prime->min++;
	}
	//pthread_exit(NULL); perror("-------pthread_exit");
}
