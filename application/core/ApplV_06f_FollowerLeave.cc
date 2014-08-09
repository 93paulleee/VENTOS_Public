
#include "ApplV_06_PlatoonMg.h"

namespace VENTOS {

void ApplVPlatoonMg::followerLeave_handleSelfMsg(cMessage* msg)
{
    if(!followerLeaveEnabled)
        return;

    if(msg == plnTIMER10)
    {
        if(vehicleState == state_waitForLeaveReply)
        {
            vehicleState = state_sendLeaveReq;
            reportStateToStat();

            followerLeave_DataFSM();
        }
    }
    else if(msg == plnTIMER11)
    {
        // check if we are free agent?
        if(vehicleState == state_platoonLeader && plnSize == 1)
        {
            TraCI->commandSetvClass(SUMOvID, "private");   // change vClass

            int32_t bitset = TraCI->commandMakeLaneChangeMode(10, 01, 01, 01, 01);
            TraCI->commandSetLaneChangeMode(SUMOvID, bitset);  // alter 'lane change' mode
            TraCI->commandChangeLane(SUMOvID, 0, 5);   // change to lane 0 (normal lane)

            TraCI->commandSetSpeed(SUMOvID, 15.);

            // change color to yellow!
            TraCIColor newColor = TraCIColor::fromTkColor("yellow");
            TraCI->getCommandInterface()->setColor(SUMOvID, newColor);

            plnSize = -1;
            myPlnDepth = -1;
            busy = false;

            vehicleState = state_idle;
            reportStateToStat();
        }
        else
            scheduleAt(simTime() + .5, plnTIMER11);
    }
}


void ApplVPlatoonMg::followerLeave_BeaconFSM(BeaconVehicle *wsm)
{
    if(!followerLeaveEnabled)
        return;

}


void ApplVPlatoonMg::followerLeave_DataFSM(PlatoonMsg *wsm)
{
    if(!followerLeaveEnabled)
        return;

    if(vehicleState == state_sendLeaveReq)
    {
        // send a unicast LEAVE_REQ to the leader
        PlatoonMsg* dataMsg = prepareData(plnID, LEAVE_REQ, plnID, myPlnDepth);
        EV << "### " << SUMOvID << ": sent LEAVE_REQ." << endl;
        printDataContent(dataMsg);
        sendDelayed(dataMsg, individualOffset, lowerLayerOut);
        reportCommandToStat(dataMsg);

        vehicleState = state_waitForLeaveReply;
        reportStateToStat();

        scheduleAt(simTime() + 5., plnTIMER10);
    }
    else if(vehicleState == state_waitForLeaveReply)
    {
        if(wsm->getType() == LEAVE_REJECT && wsm->getSender() == plnID)
        {
            cancelEvent(plnTIMER10);

            vehicleState = state_platoonFollower;
            reportStateToStat();
        }
        else if(wsm->getType() == LEAVE_ACCEPT && wsm->getSender() == plnID)
        {
            cancelEvent(plnTIMER10);

            vehicleState = state_platoonFollower;
            reportStateToStat();

            // now we should wait for the leader to do the split(s), and make us a free agent.
            // we check every 0.5s to see if we are free agent
            scheduleAt(simTime() + .5, plnTIMER11);
        }
    }
    else if(vehicleState == state_platoonLeader)
    {
        // leader receives a LEAVE_REQ
        if (wsm->getType() == LEAVE_REQ && wsm->getRecipient() == SUMOvID)
        {
            // send LEAVE_ACCEPT
            PlatoonMsg* dataMsg = prepareData(wsm->getSender(), LEAVE_ACCEPT, plnID);
            EV << "### " << SUMOvID << ": sent LEAVE_ACCEPT." << endl;
            printDataContent(dataMsg);
            sendDelayed(dataMsg, individualOffset, lowerLayerOut);
            reportCommandToStat(dataMsg);

            if(wsm->getDblValue() <= 0 || wsm->getDblValue() >= plnSize)
                error("depth of the follower is not right!");

            // last follower wants to leave
            // one split is enough
            if(wsm->getDblValue() + 1 == plnSize)
            {
                RemainingSplits = 1;

                splittingDepth = wsm->getDblValue();
                splittingVehicle = plnMembersList[splittingDepth];
                splitCaller = 1;  // Notifying split that follower leave is the caller

                vehicleState = state_sendSplitReq;
                reportStateToStat();

                split_DataFSM();
            }
            // middle follower wants to leave
            // we need two splits
            else
            {
                RemainingSplits = 2;

                // start the first split
                splittingDepth = wsm->getDblValue() + 1;
                splittingVehicle = plnMembersList[splittingDepth];
                splitCaller = 1;  // Notifying split that follower leave is the caller

                vehicleState = state_sendSplitReq;
                reportStateToStat();

                split_DataFSM();
            }
        }
        // leader receives a GAP_CREATED
        else if(wsm->getType() == GAP_CREATED && wsm->getRecipient() == SUMOvID)
        {
            RemainingSplits--;

            if(RemainingSplits == 0)
            {
                // no more splits are needed. We are done!
            }
            else if(RemainingSplits == 1)
            {
                // start the second split
                splittingDepth = plnSize - 1;
                splittingVehicle = plnMembersList[splittingDepth];
                splitCaller = 1;  // Notifying split that follower leave is the caller

                vehicleState = state_sendSplitReq;
                reportStateToStat();

                split_DataFSM();
            }
        }
    }
}

}
