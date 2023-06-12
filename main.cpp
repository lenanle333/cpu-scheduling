#include <iostream>
#include <string>
#include <queue>
#include <vector>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <pthread.h>
#include <thread>
#include <atomic>

struct PCB
{
    char name[32];
    int32_t id;
    char status;
    int32_t burst_time;
    int32_t base_register;
    long limit_register;
    char process_type;
    int32_t num_of_files;
    int32_t priority;
    int32_t check_sum;
};

struct Processor
{
    int id;
    float load;
    int scheduling_algo;
    std::queue<PCB> ready_queue;
};

struct ThreadData
{
    Processor processor;
    std::vector<PCB> processes;
    std::vector<Processor> processors;
};

std::vector<Processor> all_processors;
pthread_mutex_t shared_mutex = PTHREAD_MUTEX_INITIALIZER;

FILE *readFile(const char *file)
{
    FILE *in_file = fopen(file, "r");

    if (!in_file)
    {
        std::cout << "Error: Failed to open binary file" << std::endl;
        exit(1);
    }
    return in_file;
}

void handleProcesses(PCB &pcb, FILE *file, std::vector<PCB> &all_processes)
{
    while (!feof(file))
    {
        fread(&pcb.name, 32, 1, file);
        if (!feof(file))
        {
            fread(&pcb.id, 4, 1, file);
            fread(&pcb.status, 1, 1, file);
            fread(&pcb.burst_time, 4, 1, file);
            fread(&pcb.base_register, 4, 1, file);
            fread(&pcb.limit_register, 8, 1, file);
            fread(&pcb.process_type, 1, 1, file);
            fread(&pcb.num_of_files, 4, 1, file);
            fread(&pcb.priority, 1, 1, file);
            fread(&pcb.check_sum, 4, 1, file);
            all_processes.push_back(pcb);
        }
    }
    fclose(file);
}

void handleProcessors(int argc, char *argv[], Processor &processor, std::vector<Processor> &all_processors)
{
    const float TOLERANCE = 0.0001;
    int num_of_processors = (argc - 2) / 2;
    int indx = 2;

    float total_percentage = 1.0;

    for (int i = 0; i < num_of_processors; i++)
    {
        processor.id = i;
        std::stringstream ss1(argv[indx]);
        std::stringstream ss2(argv[indx + 1]);
        if (!(ss1 >> processor.scheduling_algo) || !ss1.eof() || !(ss2 >> processor.load) || !ss2.eof())
        {
            std::cerr << "Error: argument must be an integer" << std::endl;
            std::cout << "Usage: " << argv[0] << " " << argv[1] << " <processor_id> <load>" << std::endl;
            exit(1);
        }
        total_percentage -= processor.load;

        all_processors.push_back(processor);

        indx += 2;
    }

    if (std::abs(total_percentage) > TOLERANCE)
    {
        std::cerr << "Error: Cumulative load needs to be 100%" << std::endl;
        std::cout << "Usage: " << argv[0] << " " << argv[1] << " <processor_id> <load>" << std::endl;
        exit(1);
    }
}

void handleProcessData(int num_of_processes, const std::vector<PCB> &all_processes)
{
    int total_memory_allocated = 0;
    int total_num_of_open_files = 0;

    for (int i = 0; i < num_of_processes; i++)
    {
        const PCB &pcb = all_processes[i];
        int memory_allocated = pcb.limit_register - pcb.base_register;
        total_memory_allocated += memory_allocated;
        total_num_of_open_files += pcb.num_of_files;
    }

    std::cout << "\nNumber of processes available: " << num_of_processes << "\n"
              << "Total number of memory allocated: " << total_memory_allocated << "\n"
              << "Number of open files: " << total_num_of_open_files << std::endl;
}

void handleLoadBalancing(ThreadData *processor_thread)
{
    int num_of_processors = processor_thread->processors.size();
    int next_processor_index = 0;

    int process_count = 0;
    for (int i = 0; i < num_of_processors; i++)
    {
        Processor &source = processor_thread->processors[i];
        Processor &target = processor_thread->processors[next_processor_index];

        if (source.ready_queue.size() > 1 && target.ready_queue.size() < 1)
        {
            PCB process = source.ready_queue.front();
            source.ready_queue.pop();
            target.ready_queue.push(process);
            process_count++;

            std::cout << "Transferred " << process_count << " processes from Processor " << source.id << " to Processor " << target.id << std::endl;

            next_processor_index = (next_processor_index + 1) % num_of_processors;
        }
    }
}
bool are_processors_terminated(ThreadData processor_thread)
{
    for (int i = 0; i < processor_thread.processors.size(); i++)
    {
        if (!processor_thread.processors[i].ready_queue.empty())
        {
            return false;
        }
    }
    return true;
}
void *startLoadBalancing(void *args)
{
    ThreadData *processor_thread = static_cast<ThreadData *>(args);
    while (!are_processors_terminated(*processor_thread))
    {
        handleLoadBalancing(processor_thread);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return (void *)0;
}

std::vector<PCB> sort_burst_time(std::vector<PCB> &all_processes)
{
    bool swapped;
    int size = all_processes.size();
    for (int i = 0; i < size - 1; i++)
    {
        swapped = false;
        for (int j = 0; j < size - i - 1; j++)
        {
            if (all_processes[j].burst_time > all_processes[j + 1].burst_time)
            {
                std::swap(all_processes[j], all_processes[j + 1]);
                swapped = true;
            }
        }
        if (!swapped)
        {
            break;
        }
    }
    return all_processes;
}

std::vector<PCB> sort_priority(std::vector<PCB> &all_processes)
{
    bool swapped;
    int size = all_processes.size();
    for (int i = 0; i < size - 1; i++)
    {
        swapped = false;
        for (int j = 0; j < size - i - 1; j++)
        {
            if (all_processes[j].priority < all_processes[j + 1].priority)
            {
                std::swap(all_processes[j], all_processes[j + 1]);
                swapped = true;
            }
        }
        if (!swapped)
        {
            break;
        }
    }
    return all_processes;
}

void *priorityScheduler(void *args)
{
    ThreadData *processor_t = static_cast<ThreadData *>(args);
    Processor processor = processor_t->processor;

    const int quantum = 2;
    int seconds = 0;
    int completed_processes = 0;
    int num_of_processes = processor.ready_queue.size();

    pthread_mutex_lock(&shared_mutex);
    std::cout << "\n---Running Priority Scheduler---"
              << std::endl;
    std::cout << "Processor " << processor.id << " assigned processes: " << processor.ready_queue.size() << "\n"
              << std::endl;
    pthread_mutex_unlock(&shared_mutex);

    while (!processor.ready_queue.empty())
    {
        pthread_mutex_lock(&shared_mutex);
        PCB &curr_process = processor.ready_queue.front();
        curr_process.status = 'r';
        processor.ready_queue.pop();
        pthread_mutex_unlock(&shared_mutex);

        pthread_mutex_lock(&shared_mutex);
        std::cout << "\nAlgorithm: Priority Scheduler"
                  << "\n"
                  << "Processor " << processor.id
                  << " running process " << curr_process.id << ":"
                  << std::endl;
        pthread_mutex_unlock(&shared_mutex);

        bool process_completed = false;
        if (curr_process.burst_time > quantum)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            pthread_mutex_lock(&shared_mutex);
            curr_process.burst_time -= quantum;
            std::cout << "PRIORITY: " << curr_process.priority << "\n"
                      << "Burst time: " << curr_process.burst_time << std::endl;
            pthread_mutex_unlock(&shared_mutex);
        }
        else
        {
            curr_process.burst_time = 0;
            completed_processes++;
            process_completed = true;

            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            pthread_mutex_lock(&shared_mutex);
            std::cout << "Process " << curr_process.id << " terminating..." << std::endl;
            std::cout << "Processor " << processor.id << " - Priority Scheduler processes completed: " << completed_processes << "/" << num_of_processes << std::endl;
            pthread_mutex_unlock(&shared_mutex);
            curr_process.status = 't';
        }

        seconds++;
        if (seconds == 10)
        {
            std::cout << "\nAging priority in progress..."
                      << std::endl;
            std::vector<PCB> temp_processes;
            temp_processes.clear();
            while (!processor.ready_queue.empty())
            {
                pthread_mutex_lock(&shared_mutex);
                temp_processes.push_back(processor.ready_queue.front());
                processor.ready_queue.pop();
                pthread_mutex_unlock(&shared_mutex);
            }

            for (int i = 0; i < temp_processes.size(); i++)
            {
                pthread_mutex_lock(&shared_mutex);
                temp_processes[i].priority += 1;
                processor.ready_queue.push(temp_processes[i]);
                pthread_mutex_unlock(&shared_mutex);
            }
            seconds = 0;
        }
        if (!process_completed)
        {
            pthread_mutex_lock(&shared_mutex);
            processor.ready_queue.push(curr_process);
            pthread_mutex_unlock(&shared_mutex);
        }
    }
    return (void *)0;
}

void *shortestJobFirstScheduler(void *args)
{
    ThreadData *processor_t = static_cast<ThreadData *>(args);
    Processor processor = processor_t->processor;

    int completed_processes = 0;
    int num_of_processes = processor.ready_queue.size();

    pthread_mutex_lock(&shared_mutex);
    std::cout << "\n---Running Shortest Job First scheduler---"
              << std::endl;
    std::cout << "Processor " << processor.id << " assigned processes: " << processor.ready_queue.size() << "\n"
              << std::endl;
    pthread_mutex_unlock(&shared_mutex);

    while (!processor.ready_queue.empty())
    {
        pthread_mutex_lock(&shared_mutex);
        PCB &curr_process = processor.ready_queue.front();
        curr_process.status = 'r';
        processor.ready_queue.pop();
        pthread_mutex_unlock(&shared_mutex);

        while (curr_process.burst_time > 0)
        {
            int quantum = std::min(curr_process.burst_time, 2);
            curr_process.burst_time -= quantum;

            pthread_mutex_lock(&shared_mutex);
            if (curr_process.burst_time != 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                std::cout << "\nAlgorithm: Shortest Job First"
                          << "\n"
                          << "Processor " << processor.id
                          << " running process " << curr_process.id << ":\n"
                          << "BURST TIME: " << curr_process.burst_time << std::endl;
                pthread_mutex_unlock(&shared_mutex);
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                std::cout << "\nAlgorithm: Shortest Job First"
                          << "\n"
                          << "Processor " << processor.id
                          << " running process " << curr_process.id << ":"
                          << std::endl;
                pthread_mutex_unlock(&shared_mutex);
            }
        }

        completed_processes++;

        pthread_mutex_lock(&shared_mutex);
        std::cout << "Process " << curr_process.id << " terminating..." << std::endl;
        std::cout << "Processor " << processor.id << " - Shortest Job First processes completed: " << completed_processes << "/" << num_of_processes << std::endl;
        pthread_mutex_unlock(&shared_mutex);
        curr_process.status = 't';
    }

    return (void *)0;
}

void *roundRobinScheduler(void *args)
{
    ThreadData *processor_t = static_cast<ThreadData *>(args);
    Processor processor = processor_t->processor;

    const int quantum = 2;
    int completed_processes = 0;
    int num_of_processes = processor.ready_queue.size();

    pthread_mutex_lock(&shared_mutex);
    std::cout << "\n---Running Round Robin scheduler---"
              << std::endl;
    std::cout << "Processor " << processor.id << " assigned processes: " << processor.ready_queue.size() << "\n"
              << std::endl;
    pthread_mutex_unlock(&shared_mutex);

    while (!processor.ready_queue.empty())
    {
        pthread_mutex_lock(&shared_mutex);
        PCB &curr_process = processor.ready_queue.front();
        curr_process.status = 'r';
        processor.ready_queue.pop();
        pthread_mutex_unlock(&shared_mutex);

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        pthread_mutex_lock(&shared_mutex);
        std::cout << "\nAlgorithm: Round Robin"
                  << "\n"
                  << "Processor " << processor.id
                  << " running process " << curr_process.id << ":"
                  << std::endl;
        pthread_mutex_unlock(&shared_mutex);

        pthread_mutex_lock(&shared_mutex);
        if (curr_process.burst_time > quantum)
        {
            curr_process.burst_time -= quantum;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            std::cout << "Burst time: " << curr_process.burst_time << "\n"
                      << std::endl;
            pthread_mutex_unlock(&shared_mutex);
        }
        else
        {
            completed_processes++;
            curr_process.burst_time = 0;

            std::cout << "Process " << curr_process.id << " terminating..."
                      << std::endl;
            std::cout << "Processor " << processor.id << " - Round Robin processes completed: " << completed_processes << "/" << num_of_processes << "\n"
                      << std::endl;
            pthread_mutex_unlock(&shared_mutex);
            curr_process.status = 't';
        }

        if (curr_process.burst_time > 0)
        {
            pthread_mutex_lock(&shared_mutex);
            processor.ready_queue.push(curr_process);
            pthread_mutex_unlock(&shared_mutex);
        }
    }

    return (void *)(0);
}

void *firstComeFirstServeScheduler(void *args)
{
    ThreadData *processor_t = static_cast<ThreadData *>(args);
    Processor processor = processor_t->processor;

    int completed_processes = 0;
    int num_of_processes = processor.ready_queue.size();

    pthread_mutex_lock(&shared_mutex);
    std::cout << "\n---Running First Come First Serve scheduler---"
              << std::endl;
    std::cout << "Processor " << processor.id << " assigned processes: " << processor.ready_queue.size() << "\n"
              << std::endl;
    pthread_mutex_unlock(&shared_mutex);

    while (!processor.ready_queue.empty())
    {
        pthread_mutex_lock(&shared_mutex);
        PCB &curr_process = processor.ready_queue.front();
        curr_process.status = 'r';
        processor.ready_queue.pop();
        pthread_mutex_unlock(&shared_mutex);

        while (curr_process.burst_time > 0)
        {
            int quantum = std::min(curr_process.burst_time, 2);
            curr_process.burst_time -= quantum;

            pthread_mutex_lock(&shared_mutex);
            if (curr_process.burst_time != 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                std::cout << "\nAlgorithm: First Come First Serve"
                          << "\n"
                          << "Processor " << processor.id
                          << " running PROCESS " << curr_process.id << ":\n"
                          << "Burst time: " << curr_process.burst_time << std::endl;
                pthread_mutex_unlock(&shared_mutex);
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                std::cout << "\nAlgorithm: First Come First Serve"
                          << "\n"
                          << "Processor " << processor.id
                          << " running PROCESS " << curr_process.id << ":"
                          << std::endl;
                pthread_mutex_unlock(&shared_mutex);
            }
        }

        completed_processes++;

        pthread_mutex_lock(&shared_mutex);
        std::cout << "Process " << curr_process.id << " terminating..." << std::endl;
        std::cout << "Processor " << processor.id << " - First Come First Serve processes completed " << completed_processes << "/" << num_of_processes << std::endl;
        pthread_mutex_unlock(&shared_mutex);
        curr_process.status = 't';
    }
    return (void *)0;
}

int main(int argc, char *argv[])
{
    if (argc < 4 || argc % 2 != 0)
    {
        std::cerr << "Error: Invalid number of arguments" << std::endl;
        std::cout << "Usage: " << argv[0] << " " << argv[1] << " <processor_id> <load>" << std::endl;
        return 1;
    }
    ThreadData *processor_thread = new ThreadData();
    PCB pcb = {};
    Processor p = {};
    std::vector<PCB> all_processes;
    std::vector<pthread_t> threads;

    char *binary_file = argv[1];
    FILE *in_file = readFile(binary_file);

    int total_memory_allocated = 0;
    int total_num_of_open_files = 0;
    int memory_allocated = 0;

    handleProcesses(pcb, in_file, all_processes);
    int num_of_processes = all_processes.size();

    handleProcessData(num_of_processes, all_processes);

    handleProcessors(argc, argv, p, all_processors);

    int num_of_processors = all_processors.size();

    pthread_t load_balancing_thread;
    pthread_create(&load_balancing_thread, NULL, startLoadBalancing, processor_thread);

    for (int i = 0; i < num_of_processors; i++)
    {
        pthread_mutex_lock(&shared_mutex);
        processor_thread->processor = all_processors[i];
        processor_thread->processors = all_processors;
        processor_thread->processes = all_processes;
        pthread_mutex_unlock(&shared_mutex);

        int algorithm = processor_thread->processor.scheduling_algo % 4;
        processor_thread->processor.scheduling_algo = algorithm;

        int assigned_processes = processor_thread->processor.load * num_of_processes;

        if (algorithm == 2)
        {
            processor_thread->processes = sort_burst_time(processor_thread->processes);
        }
        else if (algorithm == 3)
        {
            processor_thread->processes = sort_priority(processor_thread->processes);
        }

        for (int j = 0; j < assigned_processes; j++)
        {
            pthread_mutex_lock(&shared_mutex);
            processor_thread->processor.ready_queue.push(processor_thread->processes[j]);
            pthread_mutex_unlock(&shared_mutex);
        }

        pthread_mutex_lock(&shared_mutex);
        all_processes.erase(all_processes.begin(), all_processes.begin() + assigned_processes);
        pthread_mutex_unlock(&shared_mutex);
        all_processors = processor_thread->processors;
        switch (algorithm)
        {
        case 0:
            /* First Come First Serve */
            pthread_t fcfs;
            pthread_create(&fcfs, NULL, firstComeFirstServeScheduler, processor_thread);
            threads.push_back(fcfs);
            break;
        case 1:
            /* Round Robin */
            pthread_t rr;
            pthread_create(&rr, NULL, roundRobinScheduler, processor_thread);
            threads.push_back(rr);
            break;
        case 2:
            /* Shortest Job First */
            pthread_t sjf;
            pthread_create(&sjf, NULL, shortestJobFirstScheduler, processor_thread);
            threads.push_back(sjf);
            break;
        case 3:
            /* Priority Scheduling */
            pthread_t ps;
            pthread_create(&ps, NULL, priorityScheduler, processor_thread);
            threads.push_back(ps);
            break;
        default:
            break;
        }
    }

    for (int i = 0; i < threads.size(); i++)
    {
        pthread_join(threads[i], NULL);
    }

    pthread_join(load_balancing_thread, NULL);
    pthread_mutex_destroy(&shared_mutex);
    threads.clear();
    delete processor_thread;
    return 0;
}
