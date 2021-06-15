#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

int shmid;
int shm_counter;
int shm_reindeer;
int shm_elf;
int shm_christmas;

int *counter;
int *reindeer;
int *christmas;
int *elf;

FILE *f;

typedef struct {
    int shmid;
    sem_t sem_santa_sleep;
    sem_t sem_santa_working;
    sem_t sem_counter;
    sem_t sem_write_log;
    sem_t sem_mutex;
    sem_t elfTex;
    sem_t elfHelp;
    sem_t getHitched;
} Semaphores;

Semaphores *shm;

bool init();

void check_num_in_range(int n, int min, int max);
void check_num_in_range_include(int n, int min, int max);
void check_process(int fd);
void write_log(char* name, int id, char*state);

void santa_process(int NR);
void reindeer_process(int NR, int TR);
void elf_process(int NE, int TE);

void clean();

/***
 * Main function which runs all processes
 * @param argc number of input arguments
 * @param argv input arguments
 * @return 0 if process runs without failure
 */

int main(int argc, char* argv[]) {

    // check whether 5 arguments have been entered
    if (argc != 5){
        perror("Invalid number of arguments\n");
        exit (1);
    }

    //input variables
    int NE, NR;
    int TE, TR;


    char* str; //check if in num is str
    check_num_in_range(NE = (int)strtol(argv[1], &str, 0), 0, 1000);
    check_num_in_range(NR = (int)strtol(argv[2], &str, 0), 0, 20);
    check_num_in_range_include(TE = (int)strtol(argv[3], &str, 0), 0, 1000);
    check_num_in_range_include(TR = (int)strtol(argv[4], &str, 0), 0, 1000);

    if(strlen(str)){
        perror("Invalid argument");
        exit(1);
    }

    // open file
    f = fopen("proj2.out", "w");

    if (f == NULL) {
        perror("Error: Can not open file\n");
        exit(1);
    }

    // initialize semaphores and memory
    if(!init()){
        clean();
        exit(1);
    }

    //set up shared memory
    *counter = 0;
    *elf = 0;
    *reindeer = 0;
    *christmas = 0;


    int fd = fork(); // Initialize auxiliary process
    check_process(fd); // Check process whether is created correctly

    if(fd == 0){
        santa_process(NR);
        reindeer_process(NR, TR);
        elf_process(NE, TE);

        while(wait(NULL) > 0); // Parent waits till children kill themself
        exit(0); // Kill parent
    }

    wait(NULL); // Waits till child dies
    clean(); // Destroy semaphores and release memory

    fclose(f);
    return 0;
}

/***
 * Function checks whether input arguments are correct
 * @param n input number
 * @param min the lowest number of the range
 * @param max the greatest number of the range
 */

void check_num_in_range(int n, int min, int max){
    if(n <= min || max <= n){
        fprintf(stderr, "Value is not in range between (%i, %i)\n", min, max);
        exit(1);
    }
}

/***
 * Function checks whether input arguments are correct
 * @param n input number
 * @param min the lowest number which can be the input number
 * @param max the greatest number which can be the input number
 */

void check_num_in_range_include(int n, int min, int max) {
    if (n < min || max < n) {
        fprintf(stderr, "Value is not in range between <%i, %i>\n", min, max);
        exit(1);
    }
}

/***
 * Check process whether is created correctly
 * @param fd (int) process
 */
void check_process(int fd){
    if (fd == -1){
        perror("Process creation failed");
        clean();
        exit(1);
    }
}

/***
 * Write state of process to the log
 * @param name name of the process
 * @param id ID of the process
 * @param state state of the process
 */
void write_log(char* name, int id, char*state){
    sem_wait(&shm->sem_write_log); // lock to increase correctly the counter

    if (id == -1){ // if process doesn't have a ID
       fprintf(f, "%i: %s: %s\n", ++(*counter), name, state);
    }

    else{
        fprintf(f, "%i: %s %i: %s\n", ++(*counter), name, id, state);
    }
    fflush(NULL);

    sem_post(&shm->sem_write_log);

}

/***
 * Create and run santa's process
 * @param NR number of reindeers
 */
 void santa_process(int NR){
    int santa = fork(); // Create santa's process
    check_process(santa); // Check whether process is created correctly

    if (santa == 0){

        while(true){
            write_log("Santa", -1, "going to sleep");
            sem_wait(&shm->sem_santa_sleep); // unlock santa's process

            sem_wait(&shm->sem_mutex);
            if (*reindeer >= NR){

                write_log("Santa", -1, "closing workshop");

                *christmas = 1; // Closing shop
                sem_post(&shm->elfHelp); // Unlock elves
                sem_post(&shm->sem_mutex); // Unlock mutex

                sem_post(&shm->getHitched);
                sem_wait(&shm->sem_santa_working); // Waits till reindeer get hitched

                write_log("Santa", -1, "Christmas started");
                exit(0);
            }

            else if (*elf == 3){
                write_log("Santa", -1, "helping elves");

                sem_post(&shm->elfHelp); // Santa helping elves
                sem_wait(&shm->sem_santa_working);
            }

            sem_post(&shm->sem_mutex);
        }
    }
}

/***
 * Create and run elf's process
 * @param NE numbers of elves
 * @param TE sleeping time
 */
void elf_process(int NE, int TE){
    int fd_elf = fork();
    check_process(fd_elf); // Check whether process is created correctly

    if (fd_elf == 0){

        for (int i = 0; i < NE; i++){
            int elf_fd = fork();
            check_process(elf_fd); // Check whether process is created correctly

            int elfID = i + 1; // Set up elf ID

            if(elf_fd == 0){

                write_log("Elf", elfID, "started");

                while(true){
                    // Set up random sleeping time for sleep
                    sem_wait(&shm->sem_counter);
                    srand(time(0) * getpid());
                    int num_sleep = (rand() % (TE + 1));
                    sem_post(&shm->sem_counter);

                    // Sleep
                    usleep(num_sleep);

                    write_log("Elf", elfID, "need help");

                    // lock if santa is helping elves
                    sem_wait(&shm->elfTex);
                    sem_wait(&shm->sem_mutex);

                    *elf += 1;
                    if (*elf == 3 && *christmas == 0){
                        sem_post(&shm->sem_santa_sleep); // ask santa for help
                    }

                    else{
                        sem_post(&shm->elfTex); // unlock semaphores, santa is not helping elves
                    }
                    sem_post(&shm->sem_mutex);

                    sem_wait(&shm->elfHelp); // wait till santa helps
                    *elf -= 1;

                    if(*christmas == 1){ // Shop is closed
                        write_log("Elf", elfID, "taking holidays");
                        // unlock semaphores
                        sem_post(&shm->elfTex);
                        sem_post(&shm->elfHelp);

                        exit(0);
                    }

                    write_log("Elf", elfID, "get help");


                    if (*elf == 0){
                        // santa helped elves
                        sem_post(&shm->sem_santa_working);
                        sem_post(&shm->elfTex);
                    }

                    else{
                        // help another elf
                        sem_post(&shm->elfHelp);
                    }
                }
            }
        }

        // wait till children kill themself
        while(wait(NULL) > 0);
        exit(0);
    }
}

/***
 * Ceate and run reindeer's process
 * @param NR number of reindeers
 * @param TR sleeping time
 */
void reindeer_process(int NR, int TR){
    int fd = fork();
    check_process(fd); // Check process whether is created correctly

    if (fd == 0){
        for (int i = 0; i < NR; i++){
            int reindeer_fd = fork();
            check_process(reindeer_fd);  // Check process whether is created correctly
            int idReindeer = i + 1;

            if (reindeer_fd == 0){ // if process is reindeer
                write_log("RD", idReindeer, "rstarted");

                // generate random sleeping time
                sem_wait(&shm->sem_counter);
                srand(time(0) * getpid());
                int rand_num = (rand() % (TR + 1 - TR/2) + TR/2);
                sem_post(&shm->sem_counter);

                // Sleep
                usleep(rand_num);

                write_log("RD", idReindeer, "return home");

                sem_wait(&shm->sem_mutex);
                *reindeer += 1;

                // All reindeers came home
                if (*reindeer == NR){
                    sem_post(&shm->sem_santa_sleep);
                }

                sem_post(&shm->sem_mutex);

                // Wait for santa
                sem_wait(&shm->getHitched);
                sem_wait(&shm->sem_mutex);

                if (*reindeer > 0){
                    sem_post(&shm->getHitched);
                    write_log("RD", idReindeer, "get hitched");
                    *reindeer -= 1;
                }

                if (*reindeer == 0){
                    // if all reindeers are hitched
                    sem_post(&shm->sem_santa_working);
                }

                sem_post(&shm->sem_mutex);
                exit(0);
            }
        }

        while(wait(NULL) > 0);
        exit(0);
    }
}

/***
 * Initialize memory and semaphores
 * @return True if memory and semaphores are created correctly else false
 */
bool init(){

    // make shared memory
    shmid = shmget(IPC_PRIVATE, sizeof(Semaphores), IPC_CREAT | 0666);

    shm_counter = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
    shm_elf = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
    shm_reindeer = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
    shm_christmas = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);


    if (shm_counter < 0 || shm_elf < 0 || shm_reindeer < 0 || shm_christmas < 0 || shmid < 0) {
        perror("Shared memory could not be created.\n");
        return false;
    }

    shm = shmat(shmid, NULL, 0);

    counter = shmat(shm_counter, NULL, 0);
    elf = shmat(shm_elf, NULL, 0);
    reindeer = shmat(shm_reindeer, NULL, 0);
    christmas = shmat(shm_christmas, NULL, 0);


    if (counter == (int *) -1 || elf == (int*) -1 || reindeer == (int *) -1 || christmas == (int *) -1 || shm == (Semaphores *) -1){
        perror("Shared memory could not be created.\n");
        return false;
    }

    shm->shmid = shmid;

    // initialize semaphores
    if (sem_init(&shm->sem_mutex, 1, 1) == -1 || sem_init(&shm->sem_santa_sleep, 1, 0) == -1 || sem_init(&shm->sem_santa_working, 1, 0) == -1 || sem_init(&shm->sem_counter, 1, 1) == -1
        || sem_init(&shm->elfTex, 1, 1) == -1 || sem_init(&shm->elfHelp, 1, 0) == -1 || sem_init(&shm->getHitched, 1, 0) == -1 || sem_init(&shm->sem_write_log, 1, 1) == -1) {
        perror("Initialise Semaphore failed!");
        return false;
    }

    return true;
}

/***
 * Release memory and destroy semaphores
 */
void clean(){
    // destroy semaphores
    if (sem_destroy(&shm->sem_mutex) == -1 || sem_init(&shm->sem_santa_sleep, 1, 0) == -1|| sem_destroy(&shm->sem_santa_working) == -1 || sem_destroy(&shm->sem_counter) == -1
    || sem_destroy(&shm->elfTex) == -1 || sem_destroy(&shm->elfHelp) == -1 || sem_destroy(&shm->getHitched) == -1 || sem_destroy(&shm->sem_write_log) == -1) {
        perror("Destroyes Semaphore failed!");
        exit(1);
    }

    shmid = shm->shmid;

    // remove shared memory
    if (shmdt(counter) == -1 || shmdt(elf) == -1 || shmdt(reindeer) == -1 || shmdt(christmas) == -1 || shmdt(shm) == -1) {
        perror("Remove shared memory failed!");
        exit(1);
    }


    if (shmctl(shm_counter, IPC_RMID, NULL) == -1 || shmctl(shm_elf, IPC_RMID, NULL) == -1 ||
    shmctl(shm_reindeer, IPC_RMID, NULL) == -1 || shmctl(shm_christmas, IPC_RMID, NULL) == -1 || shmctl(shmid, IPC_RMID, NULL) == -1){
        perror("Adress not found");
        exit(1);
    }
}
