
#include "ApplVPlatoonFormation2.h"

Define_Module(ApplVPlatoonFormation2);

void ApplVPlatoonFormation2::initialize(int stage)
{
    ApplVPlatoonFormation::initialize(stage);

	if (stage == 0)
	{
	    // NED variables
        platoonLeaderLeave = par("platoonLeaderLeave").boolValue();
        platoonMemberLeave = par("platoonMemberLeave").boolValue();
        timer3Value = par("timer3Value").doubleValue();

	    EventTimer1 = new cMessage("timer_PL_leave", timer_PL_leave);
	    TIMER3 = new cMessage("timer_wait_for_newPL_response", timer_wait_for_newPL_response);

	    if(SUMOvID == "CACC1")
	    {
            scheduleAt(simTime() + 120, EventTimer1);
	    }
	}
}


void ApplVPlatoonFormation2::handleLowerMsg(cMessage* msg)
{
    error("ApplVPlatoonFormation2 should not receive any lower message!");
}


void ApplVPlatoonFormation2::handleSelfMsg(cMessage* msg)
{
    ApplVPlatoonFormation::handleSelfMsg(msg);

    // PL wants to leave the platoon
    if(msg == EventTimer1)
    {
        if( vehicleState == state_platoonLeader )
        {
            if(platoonSize == 1)
            {
                EV << "platoonSize is 1; Platoon leader can leave." << endl;

                vehicleState = state_parked;
                FSMchangeState();
            }
            else if(platoonSize > 1)
            {
                vehicleState = state_wait_for_new_PL;
                FSMchangeState();
            }
        }
    }
    else if(msg == TIMER3)
    {
        if(vehicleState == state_wait_for_new_PL)
        {
            // re-do state state_wait_for_new_PL
            FSMchangeState();
        }
    }
}


void ApplVPlatoonFormation2::onBeacon(Beacon* wsm)
{
    ApplVPlatoonFormation::onBeacon(wsm);


}


void ApplVPlatoonFormation2::onData(PlatoonMsg* wsm)
{
    ApplVPlatoonFormation::onData(wsm);

    if (wsm->getReq_res_type() == NEW_LEADER_ACCEPT_response)
    {
        // check if NEW_LEADER_ACCEPT_response is sent to me (it is unicast)
        if(std::string(wsm->getRecipient()) == SUMOvID)
        {
            // check if the new PL is sending this
            if(std::string(wsm->getSender()) == queue.front())
            {
                if(vehicleState == state_wait_for_new_PL)
                {
                    cancelEvent(TIMER3);

                    std::string newPL = queue.front();

                    // send CHANGE_PL
                    PlatoonMsg* dataMsg = ApplVPlatoonFormation::prepareData("broadcast", CHANGE_PL, platoonID, -1, newPL);
                    printDataContent(dataMsg);
                    sendDelayedDown(dataMsg,individualOffset);
                    EV << "### " << SUMOvID << ": sent CHANGE_PL." << endl;

                    vehicleState = state_parked;
                    FSMchangeState();
                    return;
                }
            }
        }
    }
    else if(wsm->getReq_res_type() == NEW_LEADER_request && std::string(wsm->getSender()) == platoonID)
    {
        // check if NEW_LEADER_request is sent to me (it is unicast)
        if(std::string(wsm->getRecipient()) == SUMOvID)
        {
            if(vehicleState == state_platoonMember)
            {
                queue.clear();
                queue = wsm->getQueueValue();
                queue.pop_front();

                // send NEW_LEADER_ACCEPT_response
                PlatoonMsg* dataMsg = ApplVPlatoonFormation::prepareData(wsm->getSender(), NEW_LEADER_ACCEPT_response, platoonID);
                printDataContent(dataMsg);
                sendDelayedDown(dataMsg,individualOffset);
                EV << "### " << SUMOvID << ": sent NEW_LEADER_ACCEPT_response." << endl;

                return;
            }
        }
    }
    else if(wsm->getReq_res_type() == CHANGE_PL && std::string(wsm->getSender()) == platoonID)
    {
        std::string newPlatoonID = wsm->getStrValue();
        platoonID = newPlatoonID;
        myPlatoonDepth--;

        if(myPlatoonDepth == 0)
        {
            TraCIColor newColor = TraCIColor::fromTkColor("red");
            manager->commandSetVehicleColor(SUMOvID, newColor);
        }
    }
}


// is called, every time the position of vehicle changes
void ApplVPlatoonFormation2::handlePositionUpdate(cObject* obj)
{
    ApplVPlatoonFormation::handlePositionUpdate(obj);
}


void ApplVPlatoonFormation2::FSMchangeState()
{
    ApplVPlatoonFormation::FSMchangeState();

    if(vehicleState == state_parked)
    {
        EV << "### " << SUMOvID << ": current vehicle status is parked." << endl;

        // park the car
        manager->commandSetvClass(SUMOvID, "ignoring");
        manager->commandChangeLane(SUMOvID, 1, 5);
        manager->commandSetSpeed(SUMOvID, 0.);

        // disable beaconing
        disableBeaconing = true;
    }
    else if(vehicleState == state_wait_for_new_PL)
    {
        EV << "### " << SUMOvID << ": current vehicle status is wait_for_new_PL." << endl;

        std::string newPL = queue.front();

        // send NEW_LEADER request
        PlatoonMsg* dataMsg = ApplVPlatoonFormation::prepareData(newPL, NEW_LEADER_request, platoonID, -1, "", queue);
        printDataContent(dataMsg);
        sendDelayedDown(dataMsg,individualOffset);
        EV << "### " << SUMOvID << ": sent NEW_LEADER request." << endl;

        scheduleAt(simTime() + timer3Value, TIMER3);
    }
}


void ApplVPlatoonFormation2::finish()
{
    ApplVPlatoonFormation::finish();

}


ApplVPlatoonFormation2::~ApplVPlatoonFormation2()
{

}
