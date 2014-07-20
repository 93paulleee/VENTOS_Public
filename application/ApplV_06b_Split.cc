
#include "ApplV_06_PlatoonMg.h"


void ApplVPlatoonMg::split_handleSelfMsg(cMessage* msg)
{
    if(msg == plnTIMER4)
    {
        vehicleState = state_sendSplitReq;
        reportStateToStat();

        split_DataFSM();
    }
    else if(msg == plnTIMER6)
    {
        if(vehicleState == state_waitForAck)
        {
            vehicleState = state_makeItFreeAgent;
            reportStateToStat();

            split_DataFSM();
        }
    }
    else if(msg == plnTIMER7)
    {
        if(vehicleState == state_waitForAllAcks2)
        {
            vehicleState = state_changePL;
            reportStateToStat();

            split_DataFSM();
        }
    }
    else if(msg == plnTIMER5)
    {
        if(vehicleState == state_waitForCHANGEPL)
        {
            vehicleState = state_platoonMember;
            reportStateToStat();
        }
    }
    else if(msg == plnTIMER8)
    {
        if(vehicleState == state_waitForSplitDone)
        {
            vehicleState = state_sendingACK;
            reportStateToStat();

            split_DataFSM();
        }
    }
    else if(msg == plnTIMER8a)
    {
        if( GapDone() )
        {
            vehicleState = state_platoonLeader;
            reportStateToStat();
        }
        else
            scheduleAt(simTime() + 0.5, plnTIMER8a);
    }
}


void ApplVPlatoonMg::split_BeaconFSM(BeaconVehicle *wsm)
{


}


void ApplVPlatoonMg::split_DataFSM(PlatoonMsg *wsm)
{
    if(vehicleState == state_sendSplitReq)
    {
        splittingDepth = optPlnSize;

        if(plnSize == -1)
            error("WTH! platoon size is -1!");

        if(splittingDepth <= 0 || splittingDepth >= plnSize)
            error("splitting depth is wrong!");

        // get the name of the vehicle
        splittingVehicle = plnMembersList[splittingDepth];

        if(splittingVehicle == "")
            error("splitting vehicle is not known!");

        // send a unicast SPLIT_REQ to the follower
        PlatoonMsg* dataMsg = prepareData(splittingVehicle, SPLIT_REQ, plnID);
        EV << "### " << SUMOvID << ": sent SPLIT_REQ." << endl;
        printDataContent(dataMsg);
        sendDelayed(dataMsg, individualOffset, lowerLayerOut);
        reportCommandToStat(dataMsg);

        vehicleState = state_waitForSplitReply;
        reportStateToStat();

        scheduleAt(simTime() + 1., plnTIMER4);
    }
    else if(vehicleState == state_waitForSplitReply)
    {
        if(wsm->getType() == SPLIT_ACCEPT && wsm->getSender() == splittingVehicle)
        {
            cancelEvent(plnTIMER4);

            vehicleState = state_makeItFreeAgent;
            reportStateToStat();

            split_DataFSM();
        }
    }
    else if(vehicleState == state_makeItFreeAgent)
    {
        // send CHANGE_PL to the splitting vehicle (last two parameters are data attached to this ucommand)
        PlatoonMsg* dataMsg = prepareData(splittingVehicle, CHANGE_PL, plnID, (-splittingDepth), splittingVehicle);
        EV << "### " << SUMOvID << ": sent CHANGE_PL." << endl;
        printDataContent(dataMsg);
        sendDelayed(dataMsg, individualOffset, lowerLayerOut);
        reportCommandToStat(dataMsg);

        vehicleState = state_waitForAck;
        reportStateToStat();

        scheduleAt(simTime() + 5., plnTIMER6);
    }
    else if(vehicleState == state_waitForAck)
    {
        if (wsm->getType() == ACK && wsm->getSender() == splittingVehicle)
        {
            cancelEvent(plnTIMER6);

            // store the elements of second platoon before erasing
            secondPlnMembersList.clear();
            for (deque<string>::iterator it=plnMembersList.begin()+splittingDepth; it!=plnMembersList.end(); ++it)
                secondPlnMembersList.push_back(*it);

            // if splitting vehicle is the last follower
            // i.e: no follower exists after the splitting vehicle
            if(splittingDepth + 1 == plnSize)
            {
                vehicleState = state_splitDone;
                reportStateToStat();
            }
            else
            {
                vehicleState = state_changePL;
                reportStateToStat();
            }

            split_DataFSM();
        }
    }
    else if(vehicleState == state_splitDone)
    {
        plnSize = splittingDepth;
        plnMembersList.pop_back();

        if(plnSize < (maxPlnSize / 2))
        {
            // decrease Tg
            PlatoonMsg* dataMsg = prepareData("multicast", CHANGE_Tg, plnID, 0.55);
            EV << "### " << SUMOvID << ": sent CHANGE_Tg with value " << 0.55 << endl;
            printDataContent(dataMsg);
            sendDelayed(dataMsg, individualOffset, lowerLayerOut);
            reportCommandToStat(dataMsg);
        }

        busy = false;

        // send unicast SPLIT_DONE
        PlatoonMsg* dataMsg = prepareData(splittingVehicle, SPLIT_DONE, plnID, -1, "", secondPlnMembersList);
        EV << "### " << SUMOvID << ": sent SPLIT_DONE." << endl;
        printDataContent(dataMsg);
        sendDelayed(dataMsg, individualOffset, lowerLayerOut);
        reportCommandToStat(dataMsg);

        vehicleState = state_platoonLeader;
        reportStateToStat();

        updateColorDepth();
    }
    else if(vehicleState == state_changePL)
    {
        // send CHANGE_PL to followers after the splitting vehicle
        for (deque<string>::iterator it=plnMembersList.begin()+splittingDepth+1; it!=plnMembersList.end(); ++it)
        {
            PlatoonMsg* dataMsg = prepareData(*it, CHANGE_PL, plnID, (-splittingDepth), splittingVehicle);
            EV << "### " << SUMOvID << ": sent CHANGE_PL." << endl;
            printDataContent(dataMsg);
            sendDelayed(dataMsg, individualOffset, lowerLayerOut);
            reportCommandToStat(dataMsg);
        }

        vehicleState = state_waitForAllAcks2;
        reportStateToStat();

        scheduleAt(simTime() + 5., plnTIMER7);
    }
    else if(vehicleState == state_waitForAllAcks2)
    {
        if (wsm->getType() == ACK && wsm->getSendingPlatoonID() == plnID && wsm->getReceivingPlatoonID() == plnID)
        {
            string followerID = wsm->getSender();
            RemoveFollowerFromList_Split(followerID);

            // all followers ACK-ed
            if(plnSize -1 == optPlnSize)
            {
                cancelEvent(plnTIMER7);
                vehicleState = state_splitDone;
                reportStateToStat();

                split_DataFSM();
            }
        }
    }
    else if(vehicleState == state_platoonMember)
    {
        // splitting vehicle receives a SPLIT_REQ
        if (wsm->getType() == SPLIT_REQ && wsm->getRecipient() == SUMOvID)
        {
            // send SPLIT_ACCEPT
            PlatoonMsg* dataMsg = prepareData(plnID, SPLIT_ACCEPT, plnID);
            EV << "### " << SUMOvID << ": sent SPLIT_ACCEPT." << endl;
            printDataContent(dataMsg);
            sendDelayed(dataMsg, individualOffset, lowerLayerOut);
            reportCommandToStat(dataMsg);

            vehicleState = state_waitForCHANGEPL;
            reportStateToStat();

            scheduleAt(simTime() + 1., plnTIMER5);
        }
    }
    else if(vehicleState == state_waitForCHANGEPL)
    {
        if (wsm->getType() == CHANGE_PL && wsm->getSender() == plnID && wsm->getRecipient() == SUMOvID)
        {
            cancelEvent(plnTIMER5);

            // save my old platoon leader info for future use
            oldPlnID = plnID;

            // I am a free agent now!
            plnID = wsm->getStrValue();
            myPlnDepth = myPlnDepth + wsm->getDblValue();
            plnSize = 1;

            // change color to red!
            TraCIColor newColor = TraCIColor::fromTkColor("red");
            TraCI->getCommandInterface()->setColor(SUMOvID, newColor);

            vehicleState = state_sendingACK;
            reportStateToStat();

            split_DataFSM();
        }
    }
    else if(vehicleState == state_sendingACK)
    {
        // send ACK
        PlatoonMsg* dataMsg = prepareData(oldPlnID, ACK, plnID);
        EV << "### " << SUMOvID << ": sent ACK." << endl;
        printDataContent(dataMsg);
        sendDelayed(dataMsg, individualOffset, lowerLayerOut);
        reportCommandToStat(dataMsg);

        vehicleState = state_waitForSplitDone;
        reportStateToStat();

        scheduleAt(simTime() + 1., plnTIMER8);
    }
    else if(vehicleState == state_waitForSplitDone)
    {
        if(wsm->getType() == SPLIT_DONE && wsm->getSender() == oldPlnID && wsm->getRecipient() == SUMOvID)
        {
            cancelEvent(plnTIMER8);
            TraCI->commandSetTg(SUMOvID, 3.5);

            plnMembersList.clear();
            plnMembersList = wsm->getQueueValue();

            plnSize = plnMembersList.size();

            updateColorDepth();

            vehicleState = state_waitForGap;
            reportStateToStat();

            scheduleAt(simTime() + .5, plnTIMER8a);
        }
    }
}


void ApplVPlatoonMg::RemoveFollowerFromList_Split(string followerID)
{
    bool found = false;
    unsigned int i = 0;

    for(i = 0; i < plnMembersList.size(); i++)
    {
        if(plnMembersList[i] == followerID)
        {
            found = true;
            break;
        }
    }

    if(found)
    {
        plnMembersList.erase(plnMembersList.begin() + i);
        plnSize--;
    }
}


bool ApplVPlatoonMg::GapDone()
{
    // we use our sonar to check the gap
    vector<string> vleaderIDnew = TraCI->commandGetLeading(SUMOvID, sonarDist);
    string vleaderID = vleaderIDnew[0];
    double gap = atof( vleaderIDnew[1].c_str() );

    if(gap > 20)
        return true;

    return false;
}

