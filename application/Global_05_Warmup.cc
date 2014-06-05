
#include "Global_05_Warmup.h"


Define_Module(Warmup);


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

        // get the ptr of the AddVehicle module
        module = simulation.getSystemModule()->getSubmodule("vehicleAdd");
        AddVehiclePtr = static_cast<VehicleAdd *>(module);
        if(AddVehiclePtr == NULL)
            error("can not get a pointer to the AddVehicle module.");

        totalVehicles = AddVehiclePtr->par("totalVehicles").longValue();

        on = par("on").boolValue();
        laneId = par("laneId").stringValue();
        stopPosition = par("stopPosition").doubleValue();
        waitingTime = par("waitingTime").doubleValue();

        startTime = -1;
        IsWarmUpFinished = false;
        warmupFinish = new cMessage("warmupFinish", 1);
    }
}


void Warmup::handleMessage(cMessage *msg)
{
    if (msg == warmupFinish)
    {
        IsWarmUpFinished = true;
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

    // upon first call, store current simulation time as startTime
    if(startTime == -1)
    {
        startTime = simTime().dbl();
    }

    if(startTime < 0)
        error("startTime is less than 0 in Warmup.");

    // who is leading?
    std::list<std::string> veh = TraCI->commandGetVehiclesOnLane(laneId);

    if(veh.empty())
        return false;

    std::string leadingVehicle = veh.back();

    double pos = TraCI->commandGetLanePosition(leadingVehicle);

    if(pos >= stopPosition)
    {
        // start breaking at stopPosition, and stop (waiting for other vehicles)
        TraCI->commandSetSpeed(leadingVehicle, 0.);

        // get # of vehicles that have entered simulation so far
        int n = TraCI->commandGetNoVehicles();

        // if all vehicles are in the simulation
        if( n == totalVehicles )
        {
            scheduleAt(simTime() + waitingTime, warmupFinish);
        }
    }

    return false;
}


void Warmup::finish()
{


}

