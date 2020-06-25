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
        update_LLC_FSM(app, cstates1, cstates2);
        update_MBA_FSM(app, cstates1, cstates2);
    }
}