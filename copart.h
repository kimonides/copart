// #include <stdbool.h>
// #include <stdio.h>
// #include <stdlib.h>

#define THETA 3
#define PERIOD 0.5

#include <set>
#include <string>

#include <stdio.h>
#include <stdlib.h>

#include "cpucounters.h"

using namespace std;

enum resource_type
{
    LLC = 0,
    MBA = 1,
    ANY = 2,
    NONE = 3,
};

enum state_type
{
    producers = 0,
    producer = 0,
    consumers = 1,
    consumer = 1,
    maintain = 2
};

struct app
{
    int cpu_core;
    int resource[2];
    enum state_type state[2];
    //enum resource_type preference[2];
    double slowdown;
    double IPCfull;
    double IPC;

    bool operator<(const app &app) const
    {
        return (slowdown < app.slowdown);
    }
};

set<struct app *> appList;

set<struct app *> get_consumer_list()
{
    set<struct app *> consumers;
    for (auto &&app : appList)
    {
        if (app->state[LLC] == consumer || app->state[MBA] == consumer )
            consumers.insert(app);
    }
    return consumers;
}

set<struct app *> get_producers_list()
{
    set<struct app *> producers;
    for (auto &&app : appList)
    {
        if (app->state[LLC] == producer || app->state[MBA] == producer)
            producers.insert(app);
    }
    return producers;
}

void popLowestSlowdown()
{
    return;
}

struct resource
{
    set<struct app *> producers;
    set<struct app *> consumers;
};

void get_next_system_state()
{
    set<struct app *> producers = get_producers_list();
    set<struct app *> consumers = get_consumer_list();

    // one resource for each resource type (LLC,MBA,ANY)
    struct resource resources[3];

    for (auto &&producer : producers)
    {
        if(producer->state[MBA] == producer && producer->state[LLC] == producer)
            resources[ANY].producers.insert(producer);
        else if(producer->state[LLC] == producer)
            resources[LLC].producers.insert(producer);
        else if(producer->state[MBA] == producer)
            resources[MBA].producers.insert(producer);
    }

    for (auto &&consumer : consumers)
    {
        while (true)
        {
            if (consumer->preference == NONE)
                break;

            enum resource_type resource_type;

            if(consumer->state[MBA] == consumer && consumer->state[LLC] == consumer)
                resource_type = ANY;
            else if(consumer->state[LLC] == consumer)
                resource_type = LLC;
            else if(consumer->state[MBA] == consumer)
                resource_type = MBA;

            resources[resource_type].consumers.insert(consumer);

            if (resources[resource_type].consumers.size() > resources[resource_type].producers.size())
            {
                resources[resource_type].consumers.erase(0);
            }
            else
                break;
        }
    }

    for (int resource_type = LLC; resource_type <= ANY; resource_type++)
    {
        for (auto &&consumer : resources[resource_type].consumers)
        {
            enum resource_type rt;
            if (resource_type == ANY)
            {
                double r = ((double)rand() / (RAND_MAX));
                (r > 0.5) ? (rt = MBA) : (rt = LLC);
            }
            struct app *producer = *(resources[rt].producers.begin());
            resources[rt].producers.erase(0);

            //Maybe add some resource changed flag
            producer->resource[rt]--;
            consumer->resource[rt]++;
        }
    }
}

void initialize_state(int number_of_apps)
{
    //initialize appList
    for (int i = 0; i < number_of_apps; i++)
    {
        struct app *app = new struct app();
        appList.insert(app);
    }
}

#define alpha 1500000
#define beta 0.01
#define B 0.03
#define delta 0.05
void update_LLC_FSM(struct app *app, CoreCounterState cstate1, CoreCounterState cstate2)
{
    double L3MissRatio = 1 - getL3CacheHitRatio(cstate1, cstate2);
    float L3AccessesPerSecond = (getL3CacheHits(cstate1, cstate2) + getL3CacheMisses(cstate1, cstate2)) * 1/{PERIOD} ;
    double currentIPC = getIPC(cstate1,cstate2);
    double performanceDegradation = ( app->IPC - currentIPC ) / app->IPC ; 
    double performanceImprovement = ( currentIPC - app->IPC ) / app->IPC ; 

    switch (app->state)
    {
        case maintain:
            if (L3MissRatio >= B)
                app->state[LLC] = consumer;
            else if (L3MissRatio < beta  || L3AccessesPerSecond < alpha)
                app->state[LLC] = producer;
            break;
        case producer:
            if (L3MissRatio >= beta)
                app->state[LLC] = maintain;
            else if ( performanceDegradation >= delta )
                app->state[LLC] = consumer;
            break;
        case consumer:
            if ( performanceImprovement < delta || L3MissRatio < B )
                app->state[LLC] = maintain;
            else if ( L3AccessesPerSecond < alpha || L3MissRatio < beta )
                app->state[LLC] = producer;
            break;

        default:
    }
}


#define gamma 0.1
#define G 0.3
#define delta 0.05
//Must change the switch and ifs in the MBA FSM
void update_MBA_FSM(struct app *app, CoreCounterState cstate1, CoreCounterState cstate2)
{
    double L3MissRatio = 1 - getL3CacheHitRatio(cstate1, cstate2);
    float L3AccessesPerSecond = (getL3CacheHits(cstate1, cstate2) + getL3CacheMisses(cstate1, cstate2)) * 1/{PERIOD} ;
    double currentIPC = getIPC(cstate1,cstate2);
    double performanceDegradation = ( app->IPC - currentIPC ) / app->IPC ; 
    double performanceImprovement = ( currentIPC - app->IPC ) / app->IPC ; 

    switch (app->state)
    {
        case maintain:
            if (L3MissRatio >= B)
                app->state[LLC] = consumer;
            else if (L3MissRatio < beta  || L3AccessesPerSecond < alpha)
                app->state[LLC] = producer;
            break;
        case producer:
            if (L3MissRatio >= beta)
                app->state[LLC] = maintain;
            else if ( performanceDegradation >= delta )
                app->state[LLC] = consumer;
            break;
        case consumer:
            if ( performanceImprovement < delta || L3MissRatio < B )
                app->state[LLC] = maintain;
            else if ( L3AccessesPerSecond < alpha || L3MissRatio < beta )
                app->state[LLC] = producer;
            break;

        default:
    }
}

void update_FSM( std::set<struct app *> appList , std::vector<CoreCounterState> cstates1, std::vector<CoreCounterState> cstates2)
{
    for (auto &&app : appList)
    {
        update_LLC_FSM(app, cstates1[app->cpu_core], cstates2[app->cpu_core]);
        update_MBA_FSM(app, cstates1[app->cpu_core], cstates2[app->cpu_core]);
    }
}

// bool compare_system_states(struct app *a, struct app *b)
// {
//     for (int i = 0; i < number_of_apps; i++)
//     {
//         if ((a[i].LLC != b[i].LLC) || (a[i].MBA != b[i].MBA))
//             return false;
//     }
//     return true;
// }

set_system_state();

void exploreSystemStateSpace(PCM *m)
{
    int retry_count = 0;
    std::vector<CoreCounterState> cstates1, cstates2;

    m->getCoreCounterStates(cstates1);

    //initialize_state(); //this should also set app states

    while (true)
    {
        set_system_state();
        MySleepMs(PERIOD*1000);
        m->getCoreCounterStates(cstates2);
        update_FSM(cstates1, cstates2); //get data and set app states

        // struct app *previous_app_list = (struct app *)malloc(sizeof(struct app) * number_of_apps);
        // memcpy(&previous_app_list, &app_list, sizeof(app_list));

        get_next_system_state();

        // if (compare_system_states(app_list, previous_app_list))
        // {
        //     if (retry_count < THETA)
        //     {
        //         get_neighbor_state();
        //         retry_count++;
        //     }
        //     else
        //         transition_to_idle_phase();
        // }

        std::swap(cstates1,cstates2);
    }
}

#define PROFILING_WAIT_TIME 10000
/*
    Profiles each application by running it with all resources and with reduced LLC Ways and Memory Bandwidth respectively to see how sensitive it is to each resource. 
    It waits at each resource allocation for PROFILING_WAIT_TIME defined in copart.h.
*/
void application_profiling_phase(PCM *m)
{
    CoreCounterState cstates1, cstates2;

    for (auto &&app : appList)
    {
        double IPCfull, IPC_low_ways, IPC_low_bw;

        string CLOS = to_string(app->cpu_core) + 1;
        string core = to_string(app->cpu_core);
        string cmd = "pqos -a \"llc:" + CLOS + "=" + core + ";\"";

        system(cmd.c_str());

        cstates1 = m->getCoreCounterState(app->cpu_core);

        //Give all ways and all memory bandwidth to the application
        cmd = "pqos -e \"llc:" + CLOS + "=0xfffff;\" > nul";
        system(cmd.c_str());
        cmd = "pqos -e \"mba:" + CLOS + "=100;\"";
        system(cmd.c_str());
        MySleepMs(PROFILING_WAIT_TIME);

        cstates2 = m->getCoreCounterState(app->cpu_core);
        IPCfull = getIPC(cstates1, cstates2);

        std::swap(cstates1, cstates2);

        //Give only 2 ways and all the memory bandwidth
        cmd = "pqos -e \"llc:" + CLOS + "=0x00003;\" > nul";
        system(cmd.c_str());
        cmd = "pqos -e \"mba:" + CLOS + "=100;\"";
        system(cmd.c_str());
        MySleepMs(PROFILING_WAIT_TIME);

        cstates2 = m->getCoreCounterState(app->cpu_core);
        IPC_low_ways = getIPC(cstates1, cstates2);

        std::swap(cstates1, cstates2);

        //Give all ways and 20% memory bandwidth
        cmd = "pqos -e \"llc:" + CLOS + "=0xfffff;\" > nul";
        system(cmd.c_str());
        cmd = "pqos -e \"mba:" + CLOS + "=20;\"";
        system(cmd.c_str());
        MySleepMs(PROFILING_WAIT_TIME);

        cstates2 = m->getCoreCounterState(app->cpu_core);
        double IPC_low_bw = getIPC(cstates1, cstates2);

        app->IPCfull = IPCfull;

        if(  (IPCfull-IPC_low_ways)/IPCfull > 0.1 )
        {
            app->state[LLC] = consumer;
        }
        if(  (IPCfull-IPC_low_bw)/IPCfull > 0.1 )
        {
            app->state[MBA] = consumer;
        }
    }
}

//Here I will start and map all the apps to cpu cores.
//IDEA create a file with all the docker initializations and take
// input from there on which apps and how to initialize them
void initiate_copart_apps()
{
    system("docker start dc-server");
    struct app *app = (struct app *)malloc(sizeof(struct app));
    app->cpu_core = 0;
    appList.insert(app);
    cout << "Current app count: " << appList.size() << endl;
}