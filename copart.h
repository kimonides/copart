// #include <stdbool.h>
// #include <stdio.h>
// #include <stdlib.h>

#define THETA 3
#define PERIOD 0.5

#include <set>
#include <string>
#include <algorithm>

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

    bool operator=(const app &app) const
    {
        bool ret = cpu_core == app.cpu_core &&
                   resource[0] == app.resource[0] &&
                   resource[1] == app.resource[1] &&
                   state[0] == app.state[0] &&
                   state[1] == app.state[1];

        return ret;
    }
};

vector<struct app> appList;

vector<struct app *> getConsumersList(vector<struct app> appList)
{
    vector<struct app *> consumers;
    for (auto &&app : appList)
    {
        if (app.state[LLC] == consumer || app.state[MBA] == consumer)
            consumers.push_back(&app);
    }
    return consumers;
}

vector<struct app *> getProducersList(vector<struct app> appList)
{
    vector<struct app *> producers;
    for (auto &&app : appList)
    {
        if (app.state[LLC] == producer || app.state[MBA] == producer)
            producers.push_back(&app);
    }
    return producers;
}

struct resource
{
    vector<struct app *> producers;
    vector<struct app *> consumers;
};

enum resource_type getResourceType(app *app)
{
    if (app->state[MBA] == consumer && app->state[LLC] == consumer)
        return ANY;
    else if (app->state[LLC] == consumer)
        return LLC;
    else if (app->state[MBA] == consumer)
        return MBA;
}

void getNextSystemState(vector<struct app> appList)
{
    set<struct app *> producers = getProducersList(appList);
    set<struct app *> consumers = getConsumersList(appList);

    // one struct resource for each resource type (LLC,MBA,ANY)
    struct resource resources[3];

    for (auto &&producer : producers)
    {
        enum resource_type resource_type = getResourceType(producer);
        resources[resource_type].producers.push_back(producer);
    }

    for (auto &&consumer : consumers)
    {
        while (true)
        {
            enum resource_type resource_type = getResourceType(consumer);

            resources[resource_type].consumers.push_back(consumer);

            if (resources[resource_type].consumers.size() > resources[resource_type].producers.size())
                resources[resource_type].consumers.erase(resources[resource_type].consumers.begin());
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
            resources[rt].producers.erase(resources[rt].producers.begin());

            //Maybe add some resource changed flag
            producer->resource[rt]--;
            consumer->resource[rt]++;
        }
    }
}

#define alpha 1500000
#define beta 0.01
#define B 0.03
#define delta 0.05
void update_LLC_FSM(struct app *app, CoreCounterState cstate1, CoreCounterState cstate2)
{
    double L3MissRatio = 1 - getL3CacheHitRatio(cstate1, cstate2);
    float L3AccessesPerSecond = (getL3CacheHits(cstate1, cstate2) + getL3CacheMisses(cstate1, cstate2)) * 1 / {PERIOD};
    double currentIPC = getIPC(cstate1, cstate2);
    double performanceDegradation = (app->IPC - currentIPC) / app->IPC;
    double performanceImprovement = (currentIPC - app->IPC) / app->IPC;

    switch (app->state)
    {
    case maintain:
        if (L3MissRatio >= B)
            app->state[LLC] = consumer;
        else if (L3MissRatio < beta || L3AccessesPerSecond < alpha)
            app->state[LLC] = producer;
        break;
    case producer:
        if (L3MissRatio >= beta)
            app->state[LLC] = maintain;
        else if (performanceDegradation >= delta)
            app->state[LLC] = consumer;
        break;
    case consumer:
        if (performanceImprovement < delta || L3MissRatio < B)
            app->state[LLC] = maintain;
        else if (L3AccessesPerSecond < alpha || L3MissRatio < beta)
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
    float L3AccessesPerSecond = (getL3CacheHits(cstate1, cstate2) + getL3CacheMisses(cstate1, cstate2)) * 1 / {PERIOD};
    double currentIPC = getIPC(cstate1, cstate2);
    double performanceDegradation = (app->IPC - currentIPC) / app->IPC;
    double performanceImprovement = (currentIPC - app->IPC) / app->IPC;

    switch (app->state)
    {
    case maintain:
        if (L3MissRatio >= B)
            app->state[LLC] = consumer;
        else if (L3MissRatio < beta || L3AccessesPerSecond < alpha)
            app->state[LLC] = producer;
        break;
    case producer:
        if (L3MissRatio >= beta)
            app->state[LLC] = maintain;
        else if (performanceDegradation >= delta)
            app->state[LLC] = consumer;
        break;
    case consumer:
        if (performanceImprovement < delta || L3MissRatio < B)
            app->state[LLC] = maintain;
        else if (L3AccessesPerSecond < alpha || L3MissRatio < beta)
            app->state[LLC] = producer;
        break;

    default:
    }
}

void update_FSM(std::vector<struct app> appList, std::vector<CoreCounterState> cstates1, std::vector<CoreCounterState> cstates2)
{
    for (auto &&app : appList)
    {
        update_LLC_FSM(&app, cstates1[app->cpu_core], cstates2[app->cpu_core]);
        update_MBA_FSM(&app, cstates1[app->cpu_core], cstates2[app->cpu_core]);
    }
}

// TODO 
// loop through each app and change it's CLOS MBA and Way Allocation if needed
void setSystemState(vector<sturct app> appList)
{
}

void initiate_copart_apps()
{
    const int appCount = 1;
    appList.reserve(appCount);

    system("docker start dc-server");
    struct app app;
    app.cpu_core = 0;
    appList.push_back(app);

    std::sort(appList.begin(), appList.end());
}

// TODO
// create a function to get a neighbor state
void getNeighborState(vector<struct app> appList)
{

}

/*
    Explores system states until it converges by using the Hospitals/Residents solution
*/
void exploreSystemStateSpace(PCM *m)
{
    int retry_count = 0;
    std::vector<CoreCounterState> cstates1, cstates2;

    m->getCoreCounterStates(cstates1);

    while (true)
    {
        setSystemState(appList);
        MySleepMs(PERIOD * 1000);
        m->getCoreCounterStates(cstates2);

        update_FSM(appList, cstates1, cstates2);

        vector<struct app> previousAppList;
        std::swap(previousAppList, appList);

        getNextSystemState(appList);

        if (appList == previousAppList)
        {
            if (retry_count < THETA)
            {
                getNeighborState(appList);
                retry_count++;
            }
            else
                //Transitioning to idle phase
                break;
        }

        std::swap(cstates1, cstates2);
    }
}

// TODO
// Make so the IPC that we get is the average of a few measurements
#define PROFILING_WAIT_TIME 10000
/*
    Profiles each application by running it with all the resources and with reduced LLC Ways and Memory Bandwidth respectively to see how sensitive it is to each resource. 
    It waits at each resource allocation for PROFILING_WAIT_TIME defined in copart.h.
*/
void applicationProfilingPhase(PCM *m)
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

        if ((IPCfull - IPC_low_ways) / IPCfull > 0.1)
        {
            app->state[LLC] = consumer;
        }
        if ((IPCfull - IPC_low_bw) / IPCfull > 0.1)
        {
            app->state[MBA] = consumer;
        }
    }
}

//Waits and monitors system until a change happens
void idlePhase(PCM *m)
{
    std::vector<CoreCounterState> cstates1, cstates2;
    m->getCoreCounterStates(cstates1);

    while (true)
    {
        MySleepMs(PERIOD * 1000);
        m->getCoreCounterStates(cstates2);

        // TODO
        // check if we must exit the idle phase

        std::swap(cstates1, cstates2);
    }
}
