
#include "ApplVSumoInteraction.h"

Define_Module(ApplVSumoInteraction);

void ApplVSumoInteraction::initialize(int stage)
{
    ApplVPlatoonFormation::initialize(stage);

	if (stage == 0)
	{
        // NED variables (packet loss ratio)
        droppT = par("droppT").doubleValue();
        droppV = par("droppV").stringValue();
        plr = par("plr").doubleValue();

        // NED variable
        modeSwitch = par("modeSwitch").boolValue();

        // set modeSwitch parameter in Sumo
        manager->commandSetModeSwitch(SUMOvID, modeSwitch);
	}
}


void ApplVSumoInteraction::handleLowerMsg(cMessage* msg)
{
    // make sure msg is of type WaveShortMessage
    WaveShortMessage* wsm = dynamic_cast<WaveShortMessage*>(msg);
    ASSERT(wsm);

    if (std::string(wsm->getName()) == "beacon")
    {
        // vehicles other than CACC should ignore the received beacon
        if( !isCACC() )
            return;

        Beacon* wsm = dynamic_cast<Beacon*>(msg);
        ASSERT(wsm);

        if( !dropBeacon(droppT, droppV, plr) )
        {
            ApplVSumoInteraction::onBeacon(wsm);
        }
        // drop the beacon, and report it to statistics
        else
        {
            reportDropToStatistics(wsm);
        }
    }
    else if(std::string(wsm->getName()) == "data")
    {
        PlatoonMsg* wsm = dynamic_cast<PlatoonMsg*>(msg);
        ASSERT(wsm);

        ApplVSumoInteraction::onData(wsm);
    }
}


// simulate packet loss in application layer
bool ApplVSumoInteraction::dropBeacon(double time, std::string vehicle, double plr)
{
    if(simTime().dbl() >= time)
    {
        // vehicle == "" --> drop beacon in all vehicles
        // vehicle == SUMOvID --> drop beacon only in specified vehicle
        if (vehicle == "" || vehicle == SUMOvID)
        {
            // random number in [0,1)
            double p = dblrand();

            if( p < (plr/100) )
                return true;   // drop the beacon
            else
                return false;  // keep the beacon
        }
        else
            return false;
    }
    else
        return false;
}


void ApplVSumoInteraction::reportDropToStatistics(Beacon* wsm)
{
    if(one_vehicle_look_ahead)
    {
        bool result =  ApplVPlatoon::isBeaconFromLeading(wsm);

        if(result)
        {
            // a beacon from preceding vehicle is drooped
            simsignal_t Signal_beaconD = registerSignal("beaconD");
            nodePtr->emit(Signal_beaconD, 1);
        }
        else
        {
            // a beacon from other vehicle is drooped
            simsignal_t Signal_beaconD = registerSignal("beaconD");
            nodePtr->emit(Signal_beaconD, 2);
        }
    }
    else
    {
        bool result = ApplVPlatoon::isBeaconFromMyPlatoonLeader(wsm);

        // todo:
        if(result)
        {
            // a beacon from platoon leader is drooped
            simsignal_t Signal_beaconD = registerSignal("beaconD");
            nodePtr->emit(Signal_beaconD, 3);
        }
        else
        {
            // a beacon from other vehicle is drooped
            simsignal_t Signal_beaconD = registerSignal("beaconD");
            nodePtr->emit(Signal_beaconD, 2);
        }
    }
}


void ApplVSumoInteraction::handleSelfMsg(cMessage* msg)
{
    ApplVPlatoonFormation::handleSelfMsg(msg);
}


void ApplVSumoInteraction::onBeacon(Beacon* wsm)
{
    EV << "## " << SUMOvID << " received beacon ..." << endl;
    ApplVBeacon::printBeaconContent(wsm);

    // pass it down
    ApplVPlatoonFormation::onBeacon(wsm);

    bool result;

    if(one_vehicle_look_ahead)
    {
        result = ApplVPlatoon::isBeaconFromLeading(wsm);
    }
    else
    {
        result = ApplVPlatoon::isBeaconFromMyPlatoonLeader(wsm);
    }

    // send results to SUMO if result = true
    if(result)
    {
        char buffer [100];
        sprintf (buffer, "%f#%f#%f#%f", (double)wsm->getSpeed(), (double)wsm->getAccel(), (double)wsm->getMaxDecel(), (simTime().dbl())*1000);
        manager->commandSetPreceding(SUMOvID, buffer);

        // a beacon from the leading vehicle or platoon leader is received
        data *pair = new data(wsm->getSender());
        simsignal_t Signal_beaconP = registerSignal("beaconP");
        nodePtr->emit(Signal_beaconP, pair);
    }
    else
    {
        // a beacon from other vehicles is received
        data *pair = new data(wsm->getSender());
        simsignal_t Signal_beaconO = registerSignal("beaconO");
        nodePtr->emit(Signal_beaconO, pair);
    }
}


void ApplVSumoInteraction::onData(PlatoonMsg* wsm)
{
    ApplVPlatoonFormation::onData(wsm);
}


// is called, every time the position of vehicle changes
void ApplVSumoInteraction::handlePositionUpdate(cObject* obj)
{
    ApplVPlatoonFormation::handlePositionUpdate(obj);
}


void ApplVSumoInteraction::finish()
{

}


ApplVSumoInteraction::~ApplVSumoInteraction()
{

}

