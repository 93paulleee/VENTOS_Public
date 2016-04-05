/****************************************************************************/
/// @file    ApplV_10_Coordinator.cc
/// @author  Mani Amoozadeh <maniam@ucdavis.edu>
/// @author  second author name
/// @date    August 2013
///
/****************************************************************************/
// VENTOS, Vehicular Network Open Simulator; see http:?
// Copyright (C) 2013-2015
/****************************************************************************/
//
// This file is part of VENTOS.
// VENTOS is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "ApplV_10_Coordinator.h"

namespace VENTOS {

double ApplVCoordinator::stopTime = 100000;

Define_Module(VENTOS::ApplVCoordinator);

ApplVCoordinator::~ApplVCoordinator()
{

}


void ApplVCoordinator::initialize(int stage)
{
    ApplVPlatoonMg::initialize(stage);

    if (stage == 0)
    {
        if(plnMode != platoonManagement)
            return;

        coordinationMode = par("coordinationMode").longValue();

        platoonCoordination = new cMessage("coordination timer", KIND_TIMER);
        scheduleAt(simTime(), platoonCoordination);
    }
}


void ApplVCoordinator::finish()
{
    ApplVPlatoonMg::finish();
}


void ApplVCoordinator::handleSelfMsg(cMessage* msg)
{
    ApplVPlatoonMg::handleSelfMsg(msg);

    if(plnMode != platoonManagement)
        return;

    if(msg == platoonCoordination)
        coordinator();
}


void ApplVCoordinator::onBeaconVehicle(BeaconVehicle* wsm)
{
    // pass it down
    ApplVPlatoonMg::onBeaconVehicle(wsm);
}


void ApplVCoordinator::onBeaconRSU(BeaconRSU* wsm)
{
    // pass it down
    ApplVPlatoonMg::onBeaconRSU(wsm);
}


void ApplVCoordinator::onData(PlatoonMsg* wsm)
{
    // pass it down
    ApplVPlatoonMg::onData(wsm);
}


// is called, every time the position of vehicle changes
void ApplVCoordinator::handlePositionUpdate(cObject* obj)
{
    ApplVPlatoonMg::handlePositionUpdate(obj);
}


void ApplVCoordinator::coordinator()
{
    // do nothing!
    if(coordinationMode == 1)
    {

    }
    // video: three plns (1,5,5) doing merge/split
    else if(coordinationMode == 2)
    {
        scenario2();
    }
    // video: leader/last follower/middle follower leave
    else if(coordinationMode == 3)
    {
        scenario3();
    }
    // video: IEEE CSS competition
    else if(coordinationMode == 4)
    {
        scenario4();
    }
    // speed profile of platoon members in split/merge
    else if(coordinationMode == 5)
    {
        scenario5();
    }
    // measure merge, split, leader/follower leave duration for
    // a platoon of size 10
    else if(coordinationMode == 6)
    {
        scenario6();
    }
    // effect of changing Tp on merge/split duration
    else if(coordinationMode == 7)
    {
        scenario7();
    }
    else if(coordinationMode == 8)
    {
        scenario8();
    }
    // a simple platoon leader leave
    else if(coordinationMode == 9)
    {
        scenario9();
    }
    else
        error("not a valid coordination mode!");

    scheduleAt(simTime() + 0.1, platoonCoordination);
}

// -----------------------------------------------------------

void ApplVCoordinator::scenario2()
{
    if(simTime().dbl() == 37)
    {
        TraCI->vehicleSetSpeed("veh1", 20.);
    }
    else if(simTime().dbl() == 59)
    {
        optPlnSize = 13;
    }
    else if(simTime().dbl() == 94)
    {
        optPlnSize = 4;
    }
    else if(simTime().dbl() == 131)
    {
        optPlnSize = 10;
    }
    else if(simTime().dbl() == 188)
    {
        optPlnSize = 3;
    }
    else if(simTime().dbl() == 200)
    {
        optPlnSize = 2;
    }
}


void ApplVCoordinator::scenario3()
{
    if(simTime().dbl() == 26)
    {
        TraCI->vehicleSetSpeed("veh1", 20.);
    }
    // leader leaves
    else if(simTime().dbl() == 49)
    {
        if(SUMOID == "veh1")
        {
            ApplVPlatoonMg::leavePlatoon();
        }
    }
    // last follower leaves
    else if(simTime().dbl() == 68)
    {
        if(SUMOID == "veh6")
        {
            ApplVPlatoonMg::leavePlatoon();
        }
    }
    // middle follower leaves
    else if(simTime().dbl() == 80)
    {
        if(SUMOID == "veh3")
        {
            ApplVPlatoonMg::leavePlatoon();
        }
    }
}


void ApplVCoordinator::scenario4()
{
    if(TraCI->vehicleGetIDCount() == 10)
        stopTime = simTime().dbl();

    // all vehicles entering after the stopTime
    // are background traffic
    if(entryTime > stopTime)
    {
        entryEnabled = false;
        VANETenabled = false;
    }

    if(simTime().dbl() == 40)
    {
        TraCI->vehicleSetSpeed("veh1", 20.);
        TraCI->vehicleSetSpeed("veh6", 20.);
    }
    else if(simTime().dbl() == 55)
    {
        optPlnSize = 10;
    }
    else if(simTime().dbl() == 77)
    {
        optPlnSize = 4;
    }
    else if(simTime().dbl() == 100)
    {
        optPlnSize = 10;
    }
    // leader leaves
    else if(simTime().dbl() == 140)
    {
        if(SUMOID == "veh1")
        {
            ApplVPlatoonMg::leavePlatoon();
        }
    }
    //    // last follower leaves
    //    else if(simTime().dbl() == 160)
    //    {
    //        if(SUMOID == "veh5")
    //        {
    //            ApplVPlatoonMg::leavePlatoon();
    //        }
    //    }
    //    // middle follower leaves
    //    else if(simTime().dbl() == 190)
    //    {
    //        if(SUMOID == "veh2")
    //        {
    //            ApplVPlatoonMg::leavePlatoon();
    //        }
    //    }
}


void ApplVCoordinator::scenario5()
{
    if(simTime().dbl() == 40)
    {
        TraCI->vehicleSetSpeed("veh1", 20.);
    }
    else if(simTime().dbl() == 73)
    {
        // disable automatic merging
        mergeEnabled = false;

        if(SUMOID == "veh1")
            splitFromPlatoon(5);
    }
    else if(simTime().dbl() == 118)
    {
        // enable automatic merging
        mergeEnabled = true;
    }
}


void ApplVCoordinator::scenario6()
{
    if(ev.isGUI())
        error("Run coordination mode 3 in command-line, not GUI!");

    // get the current run number
    int currentRun = ev.getConfigEx()->getActiveRunNumber();

    if(currentRun == 0)
    {
        if(simTime().dbl() == 40)
        {
            TraCI->vehicleSetSpeed("veh1", 20.);
        }
        else if(simTime().dbl() == 73)
        {
            // disable automatic merging
            mergeEnabled = false;

            if(SUMOID == "veh1")
                splitFromPlatoon(9);
        }
        else if(simTime().dbl() == 118)
        {
            // enable automatic merging
            mergeEnabled = true;
        }
    }
    else if(currentRun == 1)
    {
        if(simTime().dbl() == 40)
        {
            TraCI->vehicleSetSpeed("veh1", 20.);
        }
        else if(simTime().dbl() == 73)
        {
            // disable automatic merging
            mergeEnabled = false;

            if(SUMOID == "veh1")
                splitFromPlatoon(8);
        }
        else if(simTime().dbl() == 118)
        {
            // enable automatic merging
            mergeEnabled = true;
        }
    }
    else if(currentRun == 2)
    {
        if(simTime().dbl() == 40)
        {
            TraCI->vehicleSetSpeed("veh1", 20.);
        }
        else if(simTime().dbl() == 73)
        {
            // disable automatic merging
            mergeEnabled = false;

            if(SUMOID == "veh1")
                splitFromPlatoon(7);
        }
        else if(simTime().dbl() == 118)
        {
            // enable automatic merging
            mergeEnabled = true;
        }
    }
    else if(currentRun == 3)
    {
        if(simTime().dbl() == 40)
        {
            TraCI->vehicleSetSpeed("veh1", 20.);
        }
        else if(simTime().dbl() == 73)
        {
            // disable automatic merging
            mergeEnabled = false;

            if(SUMOID == "veh1")
                splitFromPlatoon(6);
        }
        else if(simTime().dbl() == 118)
        {
            // enable automatic merging
            mergeEnabled = true;
        }
    }
    else if(currentRun == 4)
    {
        if(simTime().dbl() == 40)
        {
            TraCI->vehicleSetSpeed("veh1", 20.);
        }
        else if(simTime().dbl() == 73)
        {
            // disable automatic merging
            mergeEnabled = false;

            if(SUMOID == "veh1")
                splitFromPlatoon(5);
        }
        else if(simTime().dbl() == 118)
        {
            // enable automatic merging
            mergeEnabled = true;
        }
    }
    else if(currentRun == 5)
    {
        if(simTime().dbl() == 40)
        {
            TraCI->vehicleSetSpeed("veh1", 20.);
        }
        else if(simTime().dbl() == 73)
        {
            // disable automatic merging
            mergeEnabled = false;

            if(SUMOID == "veh1")
                splitFromPlatoon(4);
        }
        else if(simTime().dbl() == 118)
        {
            // enable automatic merging
            mergeEnabled = true;
        }
    }
    else if(currentRun == 6)
    {
        if(simTime().dbl() == 40)
        {
            TraCI->vehicleSetSpeed("veh1", 20.);
        }
        else if(simTime().dbl() == 73)
        {
            // disable automatic merging
            mergeEnabled = false;

            if(SUMOID == "veh1")
                splitFromPlatoon(3);
        }
        else if(simTime().dbl() == 118)
        {
            // enable automatic merging
            mergeEnabled = true;
        }
    }
    else if(currentRun == 7)
    {
        if(simTime().dbl() == 40)
        {
            TraCI->vehicleSetSpeed("veh1", 20.);
        }
        else if(simTime().dbl() == 73)
        {
            // disable automatic merging
            mergeEnabled = false;

            if(SUMOID == "veh1")
                splitFromPlatoon(2);
        }
        else if(simTime().dbl() == 118)
        {
            // enable automatic merging
            mergeEnabled = true;
        }
    }
    else if(currentRun == 8)
    {
        if(simTime().dbl() == 40)
        {
            TraCI->vehicleSetSpeed("veh1", 20.);
        }
        else if(simTime().dbl() == 73)
        {
            // disable automatic merging
            mergeEnabled = false;

            if(SUMOID == "veh1")
                splitFromPlatoon(1);
        }
        else if(simTime().dbl() == 118)
        {
            // enable automatic merging
            mergeEnabled = true;
        }
    }
    // ----------------------------------------
    // leave
    // ----------------------------------------
    else if(currentRun == 9)
    {
        if(simTime().dbl() == 40)
        {
            TraCI->vehicleSetSpeed("veh1", 20.);
        }
        else if(simTime().dbl() == 73)
        {
            // leader leaves
            if(SUMOID == "veh1")
                leavePlatoon();
        }
        else if(simTime().dbl() == 118)
        {
            // last follower leaves
            if(SUMOID == "veh10")
                leavePlatoon();
        }
    }
    else if(currentRun == 10)
    {
        if(simTime().dbl() == 40)
        {
            TraCI->vehicleSetSpeed("veh1", 20.);
        }
        else if(simTime().dbl() == 73)
        {
            if(SUMOID == "veh3")
                leavePlatoon();
        }
        else if(simTime().dbl() == 118)
        {
            if(SUMOID == "veh6")
                leavePlatoon();
        }
    }
    else if(currentRun == 11)
    {
        if(simTime().dbl() == 40)
        {
            TraCI->vehicleSetSpeed("veh1", 20.);
        }
        else if(simTime().dbl() == 73)
        {
            if(SUMOID == "veh7")
                leavePlatoon();
        }
        else if(simTime().dbl() == 118)
        {
            if(SUMOID == "veh5")
                leavePlatoon();
        }
    }
    else if(currentRun == 12)
    {
        if(simTime().dbl() == 40)
        {
            TraCI->vehicleSetSpeed("veh1", 20.);
        }
        else if(simTime().dbl() == 73)
        {
            if(SUMOID == "veh8")
                leavePlatoon();
        }
        else if(simTime().dbl() == 118)
        {
            if(SUMOID == "veh4")
                leavePlatoon();
        }
    }
}


void ApplVCoordinator::scenario7()
{
    if(simTime().dbl() == 40)
    {
        TraCI->vehicleSetSpeed("veh1", 20.);
    }
    else if(simTime().dbl() == 73)
    {
        // disable automatic merging
        mergeEnabled = false;

        if(SUMOID == "veh1")
            splitFromPlatoon(4);
    }
    else if(simTime().dbl() == 118)
    {
        // enable automatic merging
        mergeEnabled = true;
    }
}


void ApplVCoordinator::scenario8()
{
    if(simTime().dbl() == 40)
    {
        optPlnSize = 10;
        TraCI->vehicleSetSpeed("veh1", 20.);
    }
    else if(simTime().dbl() == 73)
    {
        // disable automatic merging
        mergeEnabled = false;

        optPlnSize = 2;
    }
    else if(simTime().dbl() == 130)
    {
        // enable automatic merging
        mergeEnabled = true;
        optPlnSize = 10;
    }
}


void ApplVCoordinator::scenario9()
{
    if(simTime().dbl() == 40)
    {
        TraCI->vehicleSetSpeed("veh1", 20.);
    }
    else if(simTime().dbl() == 73)
    {
        // disable automatic merging
        mergeEnabled = false;

        // leader leaves
        if(SUMOID == "veh1")
            dissolvePlatoon();
        //leavePlatoon();
    }
}


} // end of namespace
