#include "types.h"
#include "user.h"

void help()
{
    printf(1, "usage: schedule command [arg...]\n");
    printf(1, "Commands and Arguments:\n");
    printf(1, "1) info\n");
    printf(1, "2) change_queue <pid> <new_queue>\n");
    printf(1, "3) set_sjf_param <pid> <burst_time> <confidence>\n");
}

void print_info()
{
    print_processes_info();
}

void change_queue(int pid, int new_queue)
{
    if (pid < 1) {
        printf(1, "Invalid pid\n");
        return;
    }
    if (new_queue < 1 || new_queue > 3) {
        printf(1, "Invalid queue\n");
        return;
    }
    int res = change_scheduling_queue(pid, new_queue);
    if (res < 0)
        printf(1, "Error changing queue\n");
    else
        printf(1, "Queue changed successfully\n");
}

void set_sjf_parameters(int pid, int burst_time, int confidence)
{
    if (pid < 1) {
        printf(1, "Invalid pid\n");
        return;
    }
    if (burst_time < 0 || confidence < 0) {
        printf(1, "Invalid params\n");
        return;
    }
    int res = set_sjf_params(pid, burst_time, confidence);
    
    if (res < 0)
        printf(1, "Error setting SJF params\n");
    else
        printf(1, "SJF params has been set successfully\n");
}


int main(int argc, char *argv[])
{
    if (argc < 2) {
        help();
        exit();
    }
    else if(strcmp(argv[1], "help") == 0) {
        help();
        exit();
    }
    else if(strcmp(argv[1], "info") == 0) {
        print_info();
    }
    else if (strcmp(argv[1], "change_queue") == 0) {
        if (argc < 4) {
            help();
            exit();
        } 
        change_queue(atoi(argv[2]), atoi(argv[3]));
    }
    else if (!strcmp(argv[1], "set_sjf_param") == 0) {
        if (argc < 5) {
            help();
            exit();
        }
        set_sjf_parameters(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
    }
    else {
        help();
        exit();
    }
    exit();
}
