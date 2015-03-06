
#include "VehicleWarmup.h"

namespace VENTOS {

Define_Module(VENTOS::Warmup);


Warmup::~Warmup()
{

}


void Warmup::initialize(int stage)
{
    if(stage ==0)
    {
        // get the ptr of the current module
        nodePtr = FindModule<>::findHost(this);
        if(nodePtr == NULL)
            error("can not get a pointer to the module.");

        // get a pointer to the TraCI module
        cModule *module = simulation.getSystemModule()->getSubmodule("TraCI");
        TraCI = static_cast<TraCI_Extend *>(module);

        // get a pointer to the VehicleSpeedProfile module
        module = simulation.getSystemModule()->getSubmodule("speedprofile");
        SpeedProfilePtr = static_cast<SpeedProfile *>(module);

        // get totoal vehicles from AddVehicle module
        totalVehicles = simulation.getSystemModule()->getSubmodule("addVehicle")->par("totalVehicles").longValue();

        Signal_executeEachTS = registerSignal("executeEachTS");
        simulation.getSystemModule()->subscribe("executeEachTS", this);

        on = par("on").boolValue();
        laneId = par("laneId").stringValue();
        stopPosition = totalVehicles * par("stopPosition").doubleValue();
        warmUpSpeed = par("warmUpSpeed").doubleValue();
        waitingTime = par("waitingTime").doubleValue();

        startTime = -1;
        IsWarmUpFinished = false;

        if(on)
        {
            warmupFinish = new cMessage("warmupFinish", 1);
        }
    }
}


void Warmup::finish()
{


}


void Warmup::handleMessage(cMessage *msg)
{
    if (msg == warmupFinish)
    {
        IsWarmUpFinished = true;
    }
}


void Warmup::receiveSignal(cComponent *source, simsignal_t signalID, long i)
{
    Enter_Method_Silent();

    if(signalID == Signal_executeEachTS)
    {
        // check if warm-up phase is finished
        bool finished = Warmup::DoWarmup();
        if (finished)
        {
            // we can start speed profiling
            SpeedProfilePtr->Change();
        }
    }
}


bool Warmup::DoWarmup()
{
    // if warmup is not on, return finished == true
    if (!on)
        return true;

    if(IsWarmUpFinished)
        return true;

    if( warmupFinish->isScheduled() )
        return false;

    // who is leading?
    list<string> veh = TraCI->commandGetLaneVehicleList(laneId);

    if(veh.empty())
        return false;

    // if startTime is not specified, then store the current simulation time as startTime
    if(startTime == -1)
    {
        startTime = simTime().dbl();
    }
    // if user specifies a startTime, but it is negative
    else if(startTime < 0)
    {
        error("startTime is less than 0 in Warmup.");
    }
    // if user specifies a startTime, but we should wait for it
    else if(startTime > simTime().dbl())
        return false;

    // get the first leading vehicle
    string leadingVehicle = veh.back();

    double pos = TraCI->commandGetVehicleLanePosition(leadingVehicle);

    // we are at stop position
    if(pos >= stopPosition)
    {
        // start breaking and wait for other vehicles
        TraCI->commandChangeVehicleSpeed(leadingVehicle, warmUpSpeed);

        // get # of vehicles that have entered simulation so far
        int n = TraCI->commandGetVehicleCount();

        // if all vehicles are in the simulation
        if( n == totalVehicles )
        {
            scheduleAt(simTime() + waitingTime, warmupFinish);
        }
    }

    return false;
}

}

