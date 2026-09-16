#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>   // for fcntl / O_NONBLOCK
#include <signal.h>  // for signals

// real-time signals for each core
#define SIG_CORE1 (SIGRTMIN)
#define SIG_CORE2 (SIGRTMIN + 1)
#define SIG_CORE3 (SIGRTMIN + 2)

//global counters for each core, incremented in signal handler
volatile sig_atomic_t msg_num_core1 = 0;
volatile sig_atomic_t msg_num_core2 = 0;
volatile sig_atomic_t msg_num_core3 = 0;

//signal handler used by main process
void core_signal_handler(int signo)
{
    if (signo == SIG_CORE1)
    {
        msg_num_core1++;
    }
    else if (signo == SIG_CORE2)
    {
        msg_num_core2++;
    }
    else if (signo == SIG_CORE3)
    {
        msg_num_core3++;
    }
}

void no_interrupt_sleep(int sec) //use professors sleep function
{
    // * advanced sleep which will not be interfered by signals
    struct timespec req, rem;

    req.tv_sec = sec;  // The time t sleep in seconds
    req.tv_nsec = 0; // Additional time t sleep in nanoseconds

    while(nanosleep(&req, &rem) == -1)
        if(errno == EINTR)
            req = rem;
}

//want the main function t coordinate processes
//use fork system call t create subprocesses (children processes)
//assume cores = subprocesses= chilren
//every core needs two pipes t send and receive messages t and from the main process.


//PART 2 MODIFICATIONS:________________________________________________________________________________________________________________________________
int main(int argc, char *argv[]) //argument count, array of strings
{   
    //illegal input handling:
    if (argc != 3)
    {   
        perror("You need exactly 3 inputs");
        exit(EXIT_FAILURE);
    }

    int total_tasks = atoi(argv[1]);
    int max_n  = atoi(argv[2]); 

    if (total_tasks <= 0)
    {
        perror("Total tasks must be positive.\n");
        exit(EXIT_FAILURE);
    }

    if (max_n <= 0)
    {
        perror("Max bits must be positive.\n");
        exit(EXIT_FAILURE);
    }

    //random number generator:
    srand(time(NULL));

//PART 1:_________________________________________________________________________________________________________________________________________

    //define variables for pipes, declare arrays of length 2 [0, 1] which repreesent read/write
    int mt_core1[2], core1_tm[2]; //R -> W, two entries arrays that have integers 0 (R), 1 (W)
    int mt_core2[2], core2_tm[2];
    int mt_core3[2], core3_tm[2];

    //create pipes using the variables (pipe() only takes one arg unfortunately)
    
//call pipe only once**
    if (pipe(mt_core1) == -1 || pipe(core1_tm) == -1 ||
        pipe(mt_core2) == -1 || pipe(core2_tm) == -1 ||
        pipe(mt_core3) == -1 || pipe(core3_tm) == -1) 
    {
        perror("ERROR SYSTEM CALL: pipes");
        exit(EXIT_FAILURE);

    }
    //NEED TO CALL FORK ONE AT A TIME TO AVOID 2^N PROCESSES, need to close 2 ends for both children and parent

    //UNUSED PIPE CHECK: 
    //1) coreK (child pipe) unused pipes 
    //2) coreX unused pipes where X != K 
    //3)unused mainK pipes (parent pipes communicating with coreK)

    //creating forks and closing unused pipes
    pid_t core1 = fork();

    if (core1 == -1) 
    {
        perror("ERROR SYSTEM CALL: fork core1");
        exit(EXIT_FAILURE);
    }   

    if (core1 == 0)
    {
        close((mt_core1[1]));
        close(core1_tm[0]);
        
        close(mt_core2[0]); close(mt_core2[1]);
        close(core2_tm[0]); close(core2_tm[1]);
        close(mt_core3[0]); close(mt_core3[1]);
        close(core3_tm[0]); close(core3_tm[1]);

        printf("unused ends closed for core 1.\n");

        // core1: read tasks, process, send results
        char buf[64];
        while (1)
        {
            ssize_t r = read(mt_core1[0], buf, sizeof(buf));
            if (r <= 0)
            {
                break; // no more tasks or error
            }

            int task_id, n;
            if (sscanf(buf, "%d_%d", &task_id, &n) != 2)
            {
                continue; // bad message
            }

            no_interrupt_sleep(1); // simulate work

            int mask = (1 << n) - 1;
            int result = task_id & mask;

            char out[64];
            int len = snprintf(out, sizeof(out),
                            "core1: task %d, n = %d, result = %d\n",
                            task_id, n, result);
            if (len > 0)
            {
                if (write(core1_tm[1], out, len) == -1)
                {
                    perror("ERROR SYSTEM CALL: write core1_tm");
                    break;
                }
                //send real-time signal t main after writing result
                if (kill(getppid(), SIG_CORE1) == -1)
                {
                    perror("ERROR SYSTEM CALL: kill core1");
                    break;
                }
            }
        }

        close(mt_core1[0]);
        close(core1_tm[1]);
        _exit(0); //use to exit core
    }   

    close(mt_core1[0]);
    close(core1_tm[1]);

    pid_t core2 = fork();
    if (core2 == -1) 
    {
        perror("ERROR SYSTEM CALL: fork core2");
        exit(EXIT_FAILURE);
    }   

    if (core2 == 0)
    {
        close((mt_core2[1]));
        close(core2_tm[0]);

        close(mt_core1[0]); close(mt_core1[1]);
        close(core1_tm[0]); close(core1_tm[1]);
        close(mt_core3[0]); close(mt_core3[1]);
        close(core3_tm[0]); close(core3_tm[1]);

        printf("unused ends closed for core 2.\n");

        char buf[64];
        while (1)
        {
            ssize_t r = read(mt_core2[0], buf, sizeof(buf));
            if (r <= 0)
            {
                break;
            }

            int task_id, n;
            if (sscanf(buf, "%d_%d", &task_id, &n) != 2)
            {
                continue;
            }

            no_interrupt_sleep(1);

            int mask = (1 << n) - 1;
            int result = task_id & mask;

            char out[64];
            int len = snprintf(out, sizeof(out),
                            "core2: task %d, n = %d, result = %d\n",
                            task_id, n, result);
            if (len > 0)
            {
                if (write(core2_tm[1], out, len) == -1)
                {
                    perror("ERROR SYSTEM CALL: write core2_tm");
                    break;
                }
                //send real-time signal t main after writing result
                if (kill(getppid(), SIG_CORE2) == -1)
                {
                    perror("ERROR SYSTEM CALL: kill core2");
                    break;
                }
            }
        }

        close(mt_core2[0]);
        close(core2_tm[1]);
        _exit(0); 
    }

    close(mt_core2[0]);
    close(core2_tm[1]);

    pid_t core3 = fork();
    if (core3 == -1) 
    {
        perror("ERROR SYSTEM CALL: fork core3");
        exit(EXIT_FAILURE);
    }   

    if (core3 == 0)
    {
        close((mt_core3[1]));
        close(core3_tm[0]);

        close(mt_core1[0]); close(mt_core1[1]);
        close(core1_tm[0]); close(core1_tm[1]);
        close(mt_core2[0]); close(mt_core2[1]);
        close(core2_tm[0]); close(core2_tm[1]);

        printf("unused ends closed for core 3.\n");

        char buf[64];
        while (1)
        {
            ssize_t r = read(mt_core3[0], buf, sizeof(buf));
            if (r <= 0)
            {
                break;
            }

            int task_id, n;
            if (sscanf(buf, "%d_%d", &task_id, &n) != 2)
            {
                continue;
            }

            no_interrupt_sleep(1);

            int mask = (1 << n) - 1;
            int result = task_id & mask;

            char out[64];
            int len = snprintf(out, sizeof(out),
                            "core3: task %d, n = %d, result = %d\n",
                            task_id, n, result);
            if (len > 0)
            {
                if (write(core3_tm[1], out, len) == -1)
                {
                    perror("ERROR SYSTEM CALL: write core3_tm");
                    break;
                }
                //send real-time signal t main after writing result
                if (kill(getppid(), SIG_CORE3) == -1)
                {
                    perror("ERROR SYSTEM CALL: kill core3");
                    break;
                }
            }
        }

        close(mt_core3[0]);
        close(core3_tm[1]);
        _exit(0); 
    }   

    close(mt_core3[0]);
    close(core3_tm[1]);

//PART 3________________________________________________________________________________________________________________________________________________________________________

    // arrays for storing final results for each core
    int core1_results[total_tasks];
    int core2_results[total_tasks];
    int core3_results[total_tasks];

    int core1_count = 0;
    int core2_count = 0;
    int core3_count = 0;

    // track whether each core is busy (1) or idle (0)
    int core_busy[3] = {0, 0, 0};

    // current result for the task that each core is working on
    int current_result_core1 = 0;
    int current_result_core2 = 0;
    int current_result_core3 = 0;

    int tasks_assigned = 0; // how many tasks main has assigned so far
    int tasks_done = 0;     // how many tasks have finished and returned results

    // set core result pipes to non-blocking to poll
    int flags;

    flags = fcntl(core1_tm[0], F_GETFL);
    if (flags != -1) fcntl(core1_tm[0], F_SETFL, flags | O_NONBLOCK);

    flags = fcntl(core2_tm[0], F_GETFL);
    if (flags != -1) fcntl(core2_tm[0], F_SETFL, flags | O_NONBLOCK);

    flags = fcntl(core3_tm[0], F_GETFL);
    if (flags != -1) fcntl(core3_tm[0], F_SETFL, flags | O_NONBLOCK);

    // set up signal handler for each core real-time signal (part 4 requirement)
    struct sigaction sa;
    sa.sa_handler = core_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIG_CORE1, &sa, NULL) == -1 ||
        sigaction(SIG_CORE2, &sa, NULL) == -1 ||
        sigaction(SIG_CORE3, &sa, NULL) == -1)
    {
        perror("ERROR SYSTEM CALL: sigaction");
        exit(EXIT_FAILURE);
    }

    printf("main: starting task assignment, total tasks = %d, max bits = %d.\n", total_tasks, max_n);

    // main loop: keep assigning tasks while cores are idle and poll for inbound results
    while (tasks_done < total_tasks)
    {
        // assign tasks to idle cores (linear search through cores)
        for (int k = 0; k < 3 && tasks_assigned < total_tasks; k++)
        {
            if (core_busy[k] == 0)
            {
                int n = rand() % max_n + 1; // random n between 1 and max_n
                int task_id = tasks_assigned;

                char msg[64];
                snprintf(msg, sizeof(msg), "%d_%d", task_id, n); // "taskid_n"

                int mask = (1 << n) - 1;
                int result = task_id & mask; // precompute result for aggregation

                if (k == 0)
                {
                    if (write(mt_core1[1], msg, strlen(msg) + 1) == -1)
                    {
                        perror("ERROR SYSTEM CALL: write to core1");
                    }
                    else
                    {
                        core_busy[0] = 1;
                        current_result_core1 = result;
                        printf("main: assigned task %d to core1 with n = %d.\n", task_id, n);
                    }
                }
                else if (k == 1)
                {
                    if (write(mt_core2[1], msg, strlen(msg) + 1) == -1)
                    {
                        perror("ERROR SYSTEM CALL: write to core2");
                    }
                    else
                    {
                        core_busy[1] = 1;
                        current_result_core2 = result;
                        printf("main: assigned task %d to core2 with n = %d.\n", task_id, n);
                    }
                }
                else // k == 2
                {
                    if (write(mt_core3[1], msg, strlen(msg) + 1) == -1)
                    {
                        perror("ERROR SYSTEM CALL: write to core3");
                    }
                    else
                    {
                        core_busy[2] = 1;
                        current_result_core3 = result;
                        printf("main: assigned task %d to core3 with n = %d.\n", task_id, n);
                    }
                }

                ++tasks_assigned;
            }
        }
    
    //PART 4________________________________________________________________________________________________________________________________________________________________________

        char buffer[128];
        ssize_t bytes_read;

        // core1: only read if the signal counter says a message arrived
        if (core_busy[0] && msg_num_core1 > 0)
        {
            msg_num_core1--; // consume one message notification

            bytes_read = read(core1_tm[0], buffer, sizeof(buffer) - 1);
            if (bytes_read > 0)
            {
                buffer[bytes_read] = '\0';
                printf("%s", buffer); 

                core1_results[core1_count++] = current_result_core1;
                core_busy[0] = 0; 
                ++tasks_done;
            }
            else if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
            {
                perror("ERROR SYSTEM CALL: read from core1_tm");
            }
        }

        // core2
        if (core_busy[1] && msg_num_core2 > 0)
        {
            msg_num_core2--; // consume one message notification

            bytes_read = read(core2_tm[0], buffer, sizeof(buffer) - 1);
            if (bytes_read > 0)
            {
                buffer[bytes_read] = '\0';
                printf("%s", buffer); 

                core2_results[core2_count++] = current_result_core2;
                core_busy[1] = 0; 
                ++tasks_done;
            }
            else if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
            {
                perror("ERROR SYSTEM CALL: read from core2_tm");
            }
        }

        // core3
        if (core_busy[2] && msg_num_core3 > 0)
        {
            msg_num_core3--; // consume one message notification

            bytes_read = read(core3_tm[0], buffer, sizeof(buffer) - 1);
            if (bytes_read > 0)
            {
                buffer[bytes_read] = '\0';
                printf("%s", buffer); 

                core3_results[core3_count++] = current_result_core3;
                core_busy[2] = 0;
                ++tasks_done;
            }
            else if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
            {
                perror("ERROR SYSTEM CALL: read from core3_tm");
            }
        }

        // simple throttle to avoid burning CPU in busy loop
        no_interrupt_sleep(1);
    }

    printf("main: all tasks finished, starting post-processing.\n");

    // close write ends
    close(mt_core1[1]);
    close(mt_core2[1]);
    close(mt_core3[1]);

    //close read ends after finishing reading results
    close(core1_tm[0]);
    close(core2_tm[0]);
    close(core3_tm[0]);

    int status;

    if (waitpid(core1, &status, 0) == -1)
    {
        perror("waitpid core1 call error");
    }

    if (waitpid(core2, &status, 0) == -1)
    {
        perror("waitpid core2 call error");
    }

    if (waitpid(core3, &status, 0) == -1)
    {
        perror("waitpid core3 call error");
    }
//PART 5________________________________________________________________________________________________________________________________________________________________________
    // printing results for each core
    printf("core 1 results: ");
    for (int i = 0; i < core1_count; i++)
    {
        printf("%d", core1_results[i]);
        if (i != core1_count - 1)
        {
            printf(", ");
        }
    }
    printf("\n");

    printf("core 2 results: ");
    for (int i = 0; i < core2_count; i++)
    {
        printf("%d", core2_results[i]);
        if (i != core2_count - 1)
        {
            printf(", ");
        }
    }
    printf("\n");

    printf("core 3 results: ");
    for (int i = 0; i < core3_count; i++)
    {
        printf("%d", core3_results[i]);
        if (i != core3_count - 1)
        {
            printf(", ");
        }
    }
    printf("\n");

    return 0;
}
