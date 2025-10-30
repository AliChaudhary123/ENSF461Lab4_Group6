// scheduler.c Group 6
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#define min(a,b) (((a)<(b))?(a):(b))

// total jobs
int numofjobs = 0;

struct job {
    // job id is ordered by the arrival; jobs arrived first have smaller job id, always increment by 1
    int id;
    int arrival; // arrival time; safely assume the time unit has the minimal increment of 1
    int length;
    int tickets; // number of tickets for lottery scheduling
    // Added runtime metadata:
    int remaining;
    int start_time; // first time the job runs
    int end_time;   // completion time
    struct job *next;
};

// the workload list
struct job *head = NULL;

// helper functions
//returns 1 when all jobs have end_time set (finished)
static int all_done(void) {
    struct job *j = head;
    while (j) { if (j->end_time < 0) return 0; j = j->next; }
    return 1;
}
// find the smallest arrival time strictly greater than t among unfinsihed jobs
// returns INT_MAX if none found
static int next_arrival_after(int t) {
    int best = INT_MAX;
    for (struct job *j = head; j; j = j->next)
        if (j->end_time < 0 && j->arrival > t && j->arrival < best)
            best = j->arrival;
    return best;
}
// emit one execution segment line with the exact format expected by the tests
// tstart is segment start time
// j is the job that ran
// ran is the duration of this segment
static void print_seg(int tstart, struct job *j, int ran) {
    printf("t=%d: [Job %d] arrived at [%d], ran for: [%d]\n",
           tstart, j->id, j->arrival, ran);
}


// append a new job node with 0 based id to tail of job list
void append_to(struct job **head_pointer, int arrival, int length, int tickets){
    struct job *n = (struct job*)malloc(sizeof(struct job));
    assert(n);
    //initialize fields
    n->arrival = arrival;
    n->length  = length;
    n->tickets = tickets;
    n->remaining = length;
    n->start_time = -1;// means had not started
    n->end_time = -1;// means not yet completed
    n->next = NULL;

    if (!*head_pointer) {
        n->id = 0; // 0-based IDs, first node becomes head
        *head_pointer = n;
    } else {
        // traverse to tail and append
        struct job *t = *head_pointer;
        while (t->next) t = t->next;
        n->id = t->id + 1;
        t->next = n;
    }
    numofjobs++;
}

void read_job_config(const char* filename)
{
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    int tickets  = 0;

    char* delim = ",";
    char *arrival = NULL;
    char *length = NULL;

    fp = fopen(filename, "r");
    if (fp == NULL) {
        // automagtically prints "open: insert system error message"
        perror("open");
        exit(EXIT_FAILURE);
    }
    // falg to check if at least one valid line was read
    int any = 0;
    while ((read = getline(&line, &len, fp)) != -1)
    {
        if (read && line[read-1] == '\n') 
            line[read-1] = 0;
        // skip empty lines if the line is just \n or blank
        if (!*line) 
            continue;
        arrival = strtok(line, delim);
        length  = strtok(NULL, delim);
        // if line doesn't contain both arrival and length, error out
        if (!arrival || !length) {
            fprintf(stderr, "bad line in trace\n"); 
            exit(EXIT_FAILURE); 
        }
        tickets += 100;
        append_to(&head, atoi(arrival), atoi(length), tickets);
        // mark that at least one valid job line was processed
        any = 1;
    }
    // if no lines were read (empty file, err our and terminate
    if (!any) { 
        fprintf(stderr, "empty trace\n"); 
        exit(EXIT_FAILURE); 
    }

    fclose(fp);
    if (line) free(line);
}

//SJF (non-preemptive)
// pick shortest "arrived" unfinished job at time t
// the tiebreaker is the earlier arrvial time
static struct job* pick_sjf(int t) {
    struct job *best = NULL;
    for (struct job *j = head; j; j = j->next) {
        if (j->end_time >= 0 || j->arrival > t) continue;
        if (!best || j->remaining < best->remaining ||
           (j->remaining == best->remaining && j->arrival < best->arrival))
            best = j;
    }
    return best;
}
//SJF
void policy_SJF()
{
    printf("Execution trace with SJF:\n");

    int t = 0;//curr time
    int remaining = numofjobs;

    while (remaining > 0) {
        struct job *j = pick_sjf(t);
        // if nothing ready, jump to next arrival (tick forward)
        if (!j) {
            int na = next_arrival_after(t);
            t = (na == INT_MAX) ? t + 1 : na;
            continue;
        }
        //first run stamps start_time
        if (j->start_time < 0) j->start_time = t;
        // non-preemptive, run to completion
        print_seg(t, j, j->remaining);
        t += j->remaining;
        j->remaining = 0;
        j->end_time = t;
        remaining--;
    }

    printf("End of execution with SJF.\n");
}

//STCF (preemptive SJF)
// pick the arrived unfinished job with minimum remining at time t
// tie breaker is earlier arrival time
static struct job* pick_stcf(int t) {
    struct job *best = NULL;
    for (struct job *j = head; j; j = j->next) {
        if (j->end_time >= 0 || j->arrival > t) continue;
        if (!best || j->remaining < best->remaining ||
           (j->remaining == best->remaining && j->arrival < best->arrival))
            best = j;
    }
    return best;
}
//STCF
void policy_STCF()
{
    printf("Execution trace with STCF:\n");

    int t = 0;//curr time
    struct job *curr = NULL;//job currently running
    int seg_start = -1;// start time of curr segment

    while (!all_done()) {
        //choose best job for this tick
        struct job *next = pick_stcf(t);
        // if chosen job changed, print the segment that just ended
        if (next != curr) {
            if (curr) {
                int ran = t - seg_start;
                if (ran > 0) print_seg(seg_start, curr, ran);
            }
            curr = next;
            seg_start = t;
            if (curr && curr->start_time < 0) curr->start_time = t;
        }
        //if nothing ready, jump to next arrival/tick
        if (!curr) {
            int na = next_arrival_after(t);
            t = (na == INT_MAX) ? t + 1 : na;
            continue;
        }
        // run exatcly one tick
        curr->remaining--;
        t++;
        // when current finishes , stamp end and close the segment immeditately
        if (curr->remaining == 0) {
            curr->end_time = t;
            int ran = t - seg_start;
            if (ran > 0) print_seg(seg_start, curr, ran);
            curr = NULL;
            seg_start = -1;
        }
    }

    printf("End of execution with STCF.\n");
}





//FIFO
void policy_FIFO(){
    printf("Execution trace with FIFO:\n");

    int t = 0;//curr time
    for (struct job *j = head; j; j = j->next) {
        //if CPU is idle before job arrival, jump to arrival time
        if (t < j->arrival) t = j->arrival;
        // first run stamps start_time
        if (j->start_time < 0) j->start_time = t;
        // run to completion
        print_seg(t, j, j->remaining);
        t += j->remaining;
        j->remaining = 0;
        j->end_time = t;
    }

    printf("End of execution with FIFO.\n");
}
// analysis helper
static void analyze(const char *name) {
    printf("Begin analyzing %s:\n", name);
    int n = 0;
    double sumR=0, sumT=0, sumW=0;

    // calculate response, turnaround and wait times
    for (struct job *j = head; j; j = j->next) {
        int response = j->start_time - j->arrival;
        int turnaround = j->end_time - j->arrival;
        int wait = turnaround - j->length;

        printf("Job %d -- Response time: %d  Turnaround: %d  Wait: %d\n",
               j->id, response, turnaround, wait);

        sumR += response; 
        sumT += turnaround; 
        sumW += wait; 
        n++;
    }
    // avoid div by zero if list empty
    if (n == 0) {
        n = 1;
    }
    //print averages
    printf("Average -- Response: %.2f  Turnaround %.2f  Wait %.2f\n",
           sumR/n, sumT/n, sumW/n);
    printf("End analyzing %s.\n", name);
}

// Round-Robin and Lottery helpers and policies

// find the first ready and unfinished job at/after node 'from' in list order
static struct job* find_next_ready_from(struct job *from, int t) {
    if (!head) return NULL;
    struct job *p = from ? from : head;
    struct job *start = p;
    do {
        if (p->end_time < 0 && p->arrival <= t && p->remaining > 0) return p;
        p = (p->next) ? p->next : head;
    } while (p != start);
    return NULL;
}

// sum tickets among arrived and unfinished jobs at time t
static int total_ready_tickets(int t) {
    int tot = 0;
    for (struct job *j = head; j; j = j->next)
        if (j->end_time < 0 && j->arrival <= t && j->remaining > 0)
            tot += j->tickets;
    return tot;
}

// pick the winning job by ticket among arrived and unfinished jobs at time t
static struct job* pick_lottery(int t, int winning) {
    int acc = 0;
    for (struct job *j = head; j; j = j->next) {
        if (j->end_time < 0 && j->arrival <= t && j->remaining > 0) {
            acc += j->tickets;
            if (acc > winning) return j;
        }
    }
    return NULL;
}

// Round-Robin policy
static void policy_RR(int slice)
{
    printf("Execution trace with RR:\n");

    int t = 0;
    struct job *curr = NULL;

    while (!all_done()) {
        curr = find_next_ready_from(curr ? curr : head, t);
        if (!curr) {
            int na = next_arrival_after(t);
            t = (na == INT_MAX) ? t + 1 : na;
            curr = NULL;
            continue;
        }

        if (curr->start_time < 0) curr->start_time = t;

        int ran = min(slice, curr->remaining);
        print_seg(t, curr, ran);
        t += ran;
        curr->remaining -= ran;

        if (curr->remaining == 0) {
            curr->end_time = t;
            curr = curr->next ? curr->next : head;
        } else {
            curr = curr->next ? curr->next : head;
        }
    }

    printf("End of execution with RR.\n");
}

// Lottery policy
static void policy_LT(int slice)
{
    printf("Execution trace with LT:\n");

    // fixed seed for deterministic tests
    srand(42);

    int t = 0;

    while (!all_done()) {
        int tot = total_ready_tickets(t);
        if (tot == 0) {
            int na = next_arrival_after(t);
            t = (na == INT_MAX) ? t + 1 : na;
            continue;
        }

        int winning = rand() % tot;               // 0..tot-1
        struct job *j = pick_lottery(t, winning); // should not be NULL
        if (j->start_time < 0) j->start_time = t;

        int ran = min(slice, j->remaining);
        print_seg(t, j, ran);
        t += ran;
        j->remaining -= ran;
        if (j->remaining == 0) j->end_time = t;
    }

    printf("End of execution with LT.\n");
}

int main(int argc, char **argv){

    static char usage[] = "usage: %s analysis policy slice trace\n";

    int analysis;
    char *pname;
    char *tname;
    int slice;

    if (argc < 5)
    {
        fprintf(stderr, "missing variables\n");
        fprintf(stderr, usage, argv[0]);
        exit(1);
    }

    // if 0, we don't analysis the performance
    analysis = atoi(argv[1]);

    // policy name
    pname = argv[2];

    // time slice, only valid for RR 
    slice = atoi(argv[3]);

    // workload trace
    tname = argv[4];

    read_job_config(tname);

    if (strcmp(pname, "FIFO") == 0){
        policy_FIFO();
        if (analysis == 1) {
            analyze("FIFO");
        }
    }
    else if (strcmp(pname, "SJF") == 0)
    {
        policy_SJF();
        if (analysis == 1) {
            analyze("SJF");
        }
    }
    else if (strcmp(pname, "STCF") == 0)
    {
        policy_STCF();
        if (analysis == 1) {
            analyze("STCF");
        }
    }
    else if (strcmp(pname, "RR") == 0)
    {
        policy_RR(slice);
        if (analysis == 1) {
            analyze("RR");
        }
    }
    else if (strcmp(pname, "LT") == 0)
    {
        policy_LT(slice);
        if (analysis == 1) {
            analyze("LT");
        }
    }
    
    // included case when policy name is unknown
    else {
        fprintf(stderr, "Unknown policy: %s\n", pname);
        fprintf(stderr, usage, argv[0]);
        exit(1);
    }

    exit(0);
}
