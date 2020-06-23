// #include <stdbool.h>
// #include <stdio.h>
// #include <stdlib.h>

#define THETA 3
#define PERIOD 0.5

#include <set>

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
    enum state_type state;
    enum resource_type preference;
    float slowdown;

    bool operator<(const app &app) const
    {
        return (slowdown < app.slowdown);
    }
};

set<struct app *> appList;

set<struct app*> get_consumer_list()
{
    set<struct app *> consumers;
    for(auto &&app : appList)
    {
        if(app->state == consumer)
            consumers.insert(app);
    }
    return consumers;
}

set<struct app*> get_producers_list()
{
    set<struct app *> producers;
    for(auto &&app : appList)
    {
        if(app->state == producer)
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
    //Initialize a ordered set of all the producer apps
    set<struct app *> producers = get_producers_list();

    //Initialize a ordered set of all the consumer apps
    set<struct app *> consumers = get_consumer_list();

    // one resource for each resource type (LLC,MBA,ANY)
    struct resource resources[3];

    for (auto &&producer : producers)
    {
        enum resource_type resource_type = producer->preference;
        resources[resource_type].producers.insert( producer );
    }

    for(auto &&consumer : consumers)
    {
        while(true)
        {
            if(consumer->preference == NONE)
                break;

            enum resource_type preference = consumer->preference;
            consumer->preference = NONE;

            resources[preference].consumers.insert(consumer);
            
            if(resources[preference].consumers.size() > resources[preference].producers.size())
            {
                // todo delete lowest slowdown consumer
                popLowestSlowdown();
            }
            else 
                break;

        }
    }

    for(int resource_type = LLC ; resource_type <=ANY ; resource_type++)
    {
        for(auto &&consumer : resources[resource_type].consumers)
        {
            enum resource_type rt;
            if (resource_type == ANY)
            {
                double r = ((double)rand() / (RAND_MAX));
                (r > 0.5) ? (rt = MBA) : (rt = LLC);
            }
            struct app *producer = *(resources[rt].producers.begin());
            resources[rt].producers.erase(0);

            producer->resource[rt]--;
            consumer->resource[rt]++;
        }
    }
}



void initialize_state(int number_of_apps)
{
    //initialize appList
    for(int i=0;i<number_of_apps;i++)
    {
        struct app *app = new struct app();
        appList.insert(app);
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

void exploreSystemStateSpace()
{
    initialize_state(); //this should also set app states
    int retry_count = 0;

    while (true)
    {
        set_system_state();
        //sleep(PERIOD);
        update_fsm(app_list); //get data and set app states

        struct app *previous_app_list = (struct app *)malloc(sizeof(struct app) * number_of_apps);
        memcpy(&previous_app_list, &app_list, sizeof(app_list));

        get_next_system_state();

        if (compare_system_states(app_list, previous_app_list))
        {
            if (retry_count < THETA)
            {
                get_neighbor_state();
                retry_count++;
            }
            else
                transition_to_idle_phase();
        }
    }
}
