/*******************************************************************************
* Filename : edf.c
* Author : John Graham
* Date : 02/28/24
* Description : Earliest Deadline First Scheduling Algorithm
* Pledge : I pledge my honor that I have abided by the Stevens Honor System.
******************************************************************************/



#include <stdio.h>
#include <stdlib.h>
#include <limits.h>



struct process {
    int pid;
    int cpu_time;
    int period;
    int running_time;
    int waiting_time;
    int due_by;
};



struct proc_queue {
    int size; // curr size of the queue, index of the next empty index in the queue
    int max; // max number of items allowed in the queue (equal to number of processes)
    struct process** queue; // array of process pointers, with the first element being the front
};



/**
 * computes the gcd of x and y
 */
int gcd(int x, int y) {
    if (y == 0) {
        return x;
    }
    return gcd(y, x % y);
}



/**
 * computes the lcm of x and y
 */
int lcm(int x, int y) {
    return (x * y) / gcd(x, y);
}



/**
 * computes the lcm of all the processes periods (computes max_time)
 */
int lcm_periods(struct process procs[], int num_proc) {
    int curr_lcm = procs[0].period;
    for (int i = 1; i < num_proc; i++) {
        curr_lcm = lcm(curr_lcm, procs[i].period);
    }
    return curr_lcm;
}



/**
 * allocates space for a new queue, and returns a pointer to that queue
 */
struct proc_queue* make_queue(int num_proc) {
    struct proc_queue* q = malloc(sizeof(struct proc_queue)); // create a queue on the heap
    
    q->size = 0; // set initial size and max capacity
    q->max = num_proc;

    q->queue = malloc(sizeof(struct process*) * num_proc); // create pointer array for process pointers
    return q; // return the new queue!
}



/**
 * allocates space for a new copy of a process (in order to make processes in the queue unique)
 */
struct process* make_process(struct process proc, int t) {
    struct process* p = malloc(sizeof(struct process)); // create a process on the heap

    p->cpu_time = proc.cpu_time;
    p->period = proc.period;
    p->pid = proc.pid;
    p->due_by = t + p->period; // next deadline = current time + period

    p->running_time = 0; // these should always be initialized to 0 when making a new process
    p->waiting_time = 0;

    return p; // return the new process!
}



/**
 * pushes the given process to the end of the queue
 */
void push(struct proc_queue* q, struct process* proc) {
    if (q->size == q->max) { // if the queue is already at max capacity...
        q->max *= 2; // double the max
        q->queue = realloc(q->queue, q->max * sizeof(struct process*)); // reallocate double the size
    }
    q->queue[q->size] = proc; // place new proc at the next empty position
    q->size++; // increment size
}



/**
 * removes and returns the frontmost process in the queue, shifting the remaining elements towards the front
 */
struct process* pop(struct proc_queue* q) {
    struct process* ret = q->queue[0]; // get the process at the front of the queue

    for(int i = 0; i < q->size - 1; i++) {
        q->queue[i] = q->queue[i + 1]; // shift all elements down one
    }

    q->size--; // decrement size
    return ret; // return the popped process
}



/** 
 * removes and returns a process from the queue from the specified index, shifting the remaining elements to fill the empty space
 */
struct process* pop_at(struct proc_queue* q, int index) {
    struct process* ret = q->queue[index]; // get the process at the specified index

    for (int i = index; i < q->size - 1; i++) {
        q->queue[i] = q->queue[i + 1]; // shift all elements down one
    }

    q->size--; // decrement size
    return ret; // return the popped process
}



/**
 * prints a process queue starting with the oldest (frontmost) process, in the form of 'pid (cpuTime ms) '
 */
void print_queue(struct proc_queue* q) {
    for (int i = 0; i < q->size; i++) {
        printf(" %d (%d ms)", q->queue[i]->pid, q->queue[i]->cpu_time - q->queue[i]->running_time);
    }
    printf("\n");
}



/**
 * returns the index of the minimum element in an array
*/
int min_ind(int* arr, int len) {
    int curr_min = INT_MAX;
    int curr_ind = -1;
    for (int i = 0; i < len; i++) {
        if (arr[i] < curr_min) {
            curr_min = arr[i];
            curr_ind = i;
        }
    }
    return curr_ind;
}



/**
 * checks and sets any missed deadlines, and also handles printing the missed deadlines in the proper order (by pid increasing)
*/
void check_set_print_deadlines(struct process** procs, int num_procs, int t) {
    struct process* missed_processes[num_procs];
    int missed_pids[num_procs]; // for printing in order of pid
    for (int i = 0; i < num_procs; i++) { // initialize the array with big numbers, so we can easily get min deadlines later
        missed_pids[i] = INT_MAX; 
    }
    int num_missed = 0;
    
    for (int i = 0; i < num_procs; i++) {
        if (procs[i]->due_by == t) { // if at deadline...
            procs[i]->due_by = t + procs[i]->period; // ...set new deadline

            missed_processes[num_missed] = procs[i];
            missed_pids[num_missed] = procs[i]->pid; // ORIGINALLY was procs[i]->due_by, so that it was sorted by next earliest deadline
            num_missed++;
        }
    }

    // time to print! (this code is messy bc i need to sort by new earliest deadline to print multiple processes)
    while (num_missed != 0) {
        int curr_min_ind = min_ind(missed_pids, num_procs); // get index of minimum deadline time
        printf("%d: process %d missed deadline (%d ms left), new deadline is %d\n", // print!
            t, 
            missed_processes[curr_min_ind]->pid, 
            missed_processes[curr_min_ind]->cpu_time - missed_processes[curr_min_ind]->running_time, 
            t + missed_processes[curr_min_ind]->period);
        missed_pids[curr_min_ind] = INT_MAX; // replace so that this deadline doesn't get counted as the min again
        num_missed--; // decrement the print counter
    }
}



/**
 * gets the index of the process with the earliest deadline (returns -1 if current process deadline is already the earliest)
 */
int get_earliest_deadline(struct process** procs, int num_procs, int t, int curr_deadline) {
    int earliest_dl = curr_deadline;
    int earliest_ind = -1;

    for (int i = 0; i < num_procs; i++) { // FOR SORTING: going in order of the queue will fall back to selecting oldest process if some procs have same deadline
        if (procs[i]->due_by < earliest_dl) {
            earliest_dl = procs[i]->due_by; // set the new earliest deadline and ind
            earliest_ind = i;
        }
    }

    return earliest_ind;
}



/**
 * main loop for edf scheduling of processes
 */
void scheduling_loop(struct process procs[], int num_proc, int max_time) {
    struct proc_queue* ready_queue = make_queue(num_proc); // make a new process queue
    struct process* curr_proc = NULL;
    int curr_ind = -1;

    int total_scheduled = 0; // counters for end stats
    int total_wait = 0;

    for (int t = 0; t < max_time; t++) {
        int should_print = 0;

        check_set_print_deadlines(ready_queue->queue, ready_queue->size, t); // handle any missed deadlines, so we don't have to worry about them this period

        // FOR SORTING: going in order of the pids will ensure that for processes added at the same time, lower pids are considered older 
        for (int i = 0; i < num_proc; i++) { // check if each process should be scheduled
            if (t % procs[i].period == 0) { // schedule if current time is multiple of the period
                struct process* new_proc = make_process(procs[i], t); // create a new copy of process i
                push(ready_queue, new_proc); // add to queue
                total_scheduled++; // push to queue -> scheduled!
                should_print = 1; // queue has changed, we should print!
            }
        }


        if (should_print) { // prints the queue if needed
            printf("%d: processes (oldest first):", t);
            print_queue(ready_queue);
        }


        if (ready_queue->size != 0) { // if there are processes ready to be scheduled...
            if (curr_proc == NULL) { // if no current proc, then just get the earliest deadline in the queue and make that the current process
                int earliest_dl = get_earliest_deadline(ready_queue->queue, ready_queue->size, t, INT_MAX); // get the item in the queue with the earliest deadline
                curr_proc = ready_queue->queue[earliest_dl];
                curr_ind = earliest_dl;
                printf("%d: process %d starts\n", t, curr_proc->pid);
            }
            else { // if there already is a current process, we need to check if another deadline is earlier
                int earliest_dl = get_earliest_deadline(ready_queue->queue, ready_queue->size, t, curr_proc->due_by); // get the item in the queue with the earliest deadline
                if (earliest_dl != -1) { // preemption! (a process that is not the current one has an earlier deadline)
                    printf("%d: process %d preempted!\n", t, curr_proc->pid);
                    // push(ready_queue, curr_proc); // put the current queue back on the queue, don't reset the current running time
                    curr_proc = ready_queue->queue[earliest_dl]; // get the new process
                    curr_ind = earliest_dl;
                    printf("%d: process %d starts\n", t, curr_proc->pid);
                }
            }
        }


        if (curr_proc != NULL) { // if a process is running...
            curr_proc->running_time++; // increment its running time
            if (curr_proc->running_time == curr_proc->cpu_time) { // process ended!
                // curr_proc->running_time = 0;
                printf("%d: process %d ends\n", t + 1, curr_proc->pid); // t + 1 -> runs in t, finishes by t + 1
                total_wait += curr_proc->waiting_time; // add the total waiting time of this process to the total
                pop_at(ready_queue, curr_ind);
                free(curr_proc); // free this process
                curr_proc = NULL; // this process is done!
                curr_ind = -1;
            }
        }


        if (ready_queue->size != 0) { //if there are processes waiting in the queue, we increment wait time
            for (int i = 0; i < ready_queue->size; i++) {
                if (i == curr_ind) continue; // ignore curr process
                ready_queue->queue[i]->waiting_time++;
            }
        }
    }
    printf("%d: Max Time reached\n", max_time);

    for (int i = 0; i < ready_queue->size; i++) { // for any remaining processes
        total_wait += ready_queue->queue[i]->waiting_time; // add each processes waiting time to the total
        free(ready_queue->queue[i]); // free the process
    }

    printf("Sum of all waiting times: %d\n", total_wait);
    printf("Number of processes created: %d\n", total_scheduled);
    printf("Average Waiting Time: %.2lf\n", (double)total_wait / (double)total_scheduled);

    free(ready_queue->queue); // free the queue array and the queue object
    free(ready_queue);
}








int main(int argc, char** argv) {
    int num_proc;

    printf("Enter the number of processes to schedule: "); // get number of processes from the user
    scanf("%d", &num_proc);

    struct process processes[num_proc]; // reference structs (will make copies of these/malloc later)

    for (int i = 0; i < num_proc; i++) { // set the properties of each process
        struct process p;
        p.pid = i + 1;

        printf("Enter the CPU time of process %d: ", p.pid);
        scanf("%d", &(p.cpu_time));

        printf("Enter the period of process %d: ", p.pid);
        scanf("%d", &(p.period));

        p.running_time = 0; // set default values (will be changed as processes are created)
        p.waiting_time = 0;
        p.due_by = 0;

        processes[i] = p;
    }

    scheduling_loop(processes, num_proc, lcm_periods(processes, num_proc)); // lets start scheduling!

    return 0;
}


