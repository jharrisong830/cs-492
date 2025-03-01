#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <sys/wait.h> // added by me
#include <sys/types.h>
#include <pthread.h>

#include "scull.h"

#define CDEV_NAME "/dev/scull"

/* Quantum command line option */
static int g_quantum;

static void usage(const char *cmd)
{
	printf("Usage: %s <command>\n"
	       "Commands:\n"
	       "  R          Reset quantum\n"
	       "  S <int>    Set quantum\n"
	       "  T <int>    Tell quantum\n"
	       "  G          Get quantum\n"
	       "  Q          Query quantum\n"
	       "  X <int>    Exchange quantum\n"
	       "  H <int>    Shift quantum\n"
	       "  h          Print this message\n"
           "  i          Print info\n"
           "  p          Test info with 4 processes\n",
	       cmd);
}

typedef int cmd_t;

static cmd_t parse_arguments(int argc, const char **argv)
{
	cmd_t cmd;

	if (argc < 2) {
		fprintf(stderr, "%s: Invalid number of arguments\n", argv[0]);
		cmd = -1;
		goto ret;
	}

	/* Parse command and optional int argument */
	cmd = argv[1][0];
	switch (cmd) {
	case 'S':
	case 'T':
	case 'H':
	case 'X':
		if (argc < 3) {
			fprintf(stderr, "%s: Missing quantum\n", argv[0]);
			cmd = -1;
			break;
		}
		g_quantum = atoi(argv[2]);
		break;
	case 'R':
	case 'G':
	case 'Q':
	case 'h':
    case 'i': // added! info command
    case 'p': // added! process command
    case 't': // added! thread command
		break;
	default:
		fprintf(stderr, "%s: Invalid command\n", argv[0]);
		cmd = -1;
	}

ret:
	if (cmd < 0 || cmd == 'h') {
		usage(argv[0]);
		exit((cmd == 'h')? EXIT_SUCCESS : EXIT_FAILURE);
	}
	return cmd;
}



/*
 * runner function for created threads, each will run ioctl twice
 * fd (int) is passed as a param in each call to pthread_create
 */
void *thread_runner(void* param) {
    int ret = 0;
    struct task_info return_info;
    for (int i = 0; i < 2; i++) {
        ret = ioctl(*(int*)param, SCULL_IOCIQUANTUM, &return_info); // call ioctl and print result
        printf("state %d, cpu %d, prio %d, pid %d, tgid %d, nv %ld, niv %ld\n", return_info.__state, return_info.cpu, return_info.prio, return_info.pid, return_info.tgid, return_info.nvcsw, return_info.nivcsw);
        if (ret != 0) pthread_exit(&ret); // exit early if error
    }
    pthread_exit(0); // this thread is done!
}






static int do_op(int fd, cmd_t cmd)
{
	int ret, q;
    struct task_info ret_task_info; // initialize struct

	pid_t ch1, ch2, ch3, ch4; // for p command
    
    pthread_t tid1, tid2, tid3, tid4; // for t command

	switch (cmd) {
	case 'R':
		ret = ioctl(fd, SCULL_IOCRESET);
		if (ret == 0)
			printf("Quantum reset\n");
		break;
	case 'Q':
		q = ioctl(fd, SCULL_IOCQQUANTUM);
		printf("Quantum: %d\n", q);
		ret = 0;
		break;
	case 'G':
		ret = ioctl(fd, SCULL_IOCGQUANTUM, &q);
		if (ret == 0)
			printf("Quantum: %d\n", q);
		break;
	case 'T':
		ret = ioctl(fd, SCULL_IOCTQUANTUM, g_quantum);
		if (ret == 0)
			printf("Quantum set\n");
		break;
	case 'S':
		q = g_quantum;
		ret = ioctl(fd, SCULL_IOCSQUANTUM, &q);
		if (ret == 0)
			printf("Quantum set\n");
		break;
	case 'X':
		q = g_quantum;
		ret = ioctl(fd, SCULL_IOCXQUANTUM, &q);
		if (ret == 0)
			printf("Quantum exchanged, old quantum: %d\n", q);
		break;
	case 'H':
		q = ioctl(fd, SCULL_IOCHQUANTUM, g_quantum);
		printf("Quantum shifted, old quantum: %d\n", q);
		ret = 0;
		break;
    case 'i':
        ret = ioctl(fd, SCULL_IOCIQUANTUM, &ret_task_info); // call ioctl func to get info (will be copied to ret_task_info)
        printf("state %d, cpu %d, prio %d, pid %d, tgid %d, nv %ld, niv %ld\n", ret_task_info.__state, ret_task_info.cpu, ret_task_info.prio, ret_task_info.pid, ret_task_info.tgid, ret_task_info.nvcsw, ret_task_info.nivcsw); // print the info from the struct
		break;
    case 'p':
        ch1 = fork();
        if (ch1 == 0) {
            // CHILD 1
            struct task_info t1; // in each child, make a new task struct...
			for (int i = 0; i < 2; i++) {
				ret = ioctl(fd, SCULL_IOCIQUANTUM, &t1); // call ioctl func twice and print!
        		if (ret != 0) exit(ret); // exit in case of error
				printf("state %d, cpu %d, prio %d, pid %d, tgid %d, nv %ld, niv %ld\n", t1.__state, t1.cpu, t1.prio, t1.pid, t1.tgid, t1.nvcsw, t1.nivcsw);
			}
			exit(EXIT_SUCCESS); // exit -> back to parent!
        }
        else {
            ch2 = fork();
            if (ch2 == 0) {
                // CHILD 2
				struct task_info t2; 
				for (int i = 0; i < 2; i++) {
					ret = ioctl(fd, SCULL_IOCIQUANTUM, &t2); 
					if (ret != 0) exit(ret); 
					printf("state %d, cpu %d, prio %d, pid %d, tgid %d, nv %ld, niv %ld\n", t2.__state, t2.cpu, t2.prio, t2.pid, t2.tgid, t2.nvcsw, t2.nivcsw); 
				}
				exit(EXIT_SUCCESS);;
            }
            else {
                ch3 = fork();
                if (ch3 == 0) {
                    // CHILD 3
					struct task_info t3; 
					for (int i = 0; i < 2; i++) {
						ret = ioctl(fd, SCULL_IOCIQUANTUM, &t3); 
						if (ret != 0) exit(ret); 
						printf("state %d, cpu %d, prio %d, pid %d, tgid %d, nv %ld, niv %ld\n", t3.__state, t3.cpu, t3.prio, t3.pid, t3.tgid, t3.nvcsw, t3.nivcsw); 
					}
					exit(EXIT_SUCCESS);;
                }
                else {
                    ch4 = fork();
                    if (ch4 == 0) {
                        // CHILD 4
						struct task_info t4; 
						for (int i = 0; i < 2; i++) {
							ret = ioctl(fd, SCULL_IOCIQUANTUM, &t4); 
							if (ret != 0) exit(ret); 
							printf("state %d, cpu %d, prio %d, pid %d, tgid %d, nv %ld, niv %ld\n", t4.__state, t4.cpu, t4.prio, t4.pid, t4.tgid, t4.nvcsw, t4.nivcsw); 
						}
						exit(EXIT_SUCCESS);;
                    }
                    else {
                        // MAIN PARENT
                        int ch1_stat, ch2_stat, ch3_stat, ch4_stat;
                        waitpid(ch1, &ch1_stat, 0); // wait for all children to finish...
                        waitpid(ch2, &ch2_stat, 0);
                        waitpid(ch3, &ch3_stat, 0);
                        waitpid(ch4, &ch4_stat, 0);
						ret = 0; // set this otherwise it prints "success" error message lol
                    }
                }
            }
        }
        break;
    case 't':
        pthread_create(&tid1, NULL, thread_runner, &fd); // start 4 threads, passing in address to tid, NULL, func pointer to runner, and pointer to argument (fd)
        pthread_create(&tid2, NULL, thread_runner, &fd);
        pthread_create(&tid3, NULL, thread_runner, &fd);
        pthread_create(&tid4, NULL, thread_runner, &fd);

        pthread_join(tid1, NULL); // join -> wait for threads to finish
        pthread_join(tid2, NULL);
        pthread_join(tid3, NULL); 
        pthread_join(tid4, NULL);
        ret = 0;
        break;
	default:
		/* Should never occur */
		abort();
		ret = -1; /* Keep the compiler happy */
	}

	if (ret != 0)
		perror("ioctl");
	return ret;
}

int main(int argc, const char **argv)
{
	int fd, ret;
	cmd_t cmd;

	cmd = parse_arguments(argc, argv);

	fd = open(CDEV_NAME, O_RDONLY);
	if (fd < 0) {
		perror("cdev open");
		return EXIT_FAILURE;
	}

	printf("Device (%s) opened\n", CDEV_NAME);

	ret = do_op(fd, cmd);

	if (close(fd) != 0) {
		perror("cdev close");
		return EXIT_FAILURE;
	}

	printf("Device (%s) closed\n", CDEV_NAME);

	return (ret != 0)? EXIT_FAILURE : EXIT_SUCCESS;
}
