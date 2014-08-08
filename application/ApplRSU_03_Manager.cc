
#include "ApplRSU_03_Manager.h"

namespace VENTOS {

Define_Module(VENTOS::ApplRSUManager);

ApplRSUManager::~ApplRSUManager()
{

}


void ApplRSUManager::initialize(int stage)
{
	ApplRSUAID::initialize(stage);

	if (stage==0)
	{

	}
}


void ApplRSUManager::finish()
{
    ApplRSUAID::finish();
}


void ApplRSUManager::handleSelfMsg(cMessage* msg)
{
    ApplRSUAID::handleSelfMsg(msg);

}


void ApplRSUManager::handleLowerMsg(cMessage* msg)
{
    // make sure msg is of type WaveShortMessage
    WaveShortMessage* wsm = dynamic_cast<WaveShortMessage*>(msg);
    ASSERT(wsm);

    if (string(wsm->getName()) == "beaconVehicle")
    {
        BeaconVehicle* wsm = dynamic_cast<BeaconVehicle*>(msg);
        ASSERT(wsm);

        ApplRSUManager::onBeaconVehicle(wsm);
    }
    else if (string(wsm->getName()) == "beaconRSU")
    {
        BeaconRSU* wsm = dynamic_cast<BeaconRSU*>(msg);
        ASSERT(wsm);

        ApplRSUManager::onBeaconRSU(wsm);
    }
    else if(string(wsm->getName()) == "laneChangeMsg")
    {
        LaneChangeMsg* wsm = dynamic_cast<LaneChangeMsg*>(msg);
        ASSERT(wsm);

        ApplRSUManager::onData(wsm);
    }

    delete msg;
}


void ApplRSUManager::onBeaconVehicle(BeaconVehicle* wsm)
{
    ApplRSUAID::onBeaconVehicle(wsm);
}


void ApplRSUManager::onBeaconRSU(BeaconRSU* wsm)
{
    ApplRSUAID::onBeaconRSU(wsm);
}


void ApplRSUManager::onData(LaneChangeMsg* wsm)
{
    ApplRSUAID::onData(wsm);
}

}

