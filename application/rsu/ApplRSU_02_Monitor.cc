/****************************************************************************/
/// @file    ApplRSU_02_Monitor.cc
/// @author  Mani Amoozadeh <maniam@ucdavis.edu>
/// @author  second author name
/// @date    Dec 2015
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

#include "ApplRSU_02_Monitor.h"
#include <algorithm>
#include <iomanip>

// un-defining ev!
// why? http://stackoverflow.com/questions/24103469/cant-include-the-boost-filesystem-header
#undef ev
#include "boost/filesystem.hpp"
#define ev  (*cSimulation::getActiveEnvir())

namespace VENTOS {

std::vector<detectedVehicleEntry> ApplRSUMonitor::Vec_detectedVehicles;

Define_Module(VENTOS::ApplRSUMonitor);

ApplRSUMonitor::~ApplRSUMonitor()
{

}


void ApplRSUMonitor::initialize(int stage)
{
    ApplRSUBase::initialize(stage);

    if (stage == 0)
    {
        // note that TLs set 'activeDetection' after RSU creation
        activeDetection = par("activeDetection").boolValue();

        collectVehApproach = par("collectVehApproach").boolValue();
    }
}


void ApplRSUMonitor::finish()
{
    ApplRSUBase::finish();

    // write to file at the end of simulation
    if(activeDetection && collectVehApproach)
        saveVehApproach();
}


void ApplRSUMonitor::handleSelfMsg(cMessage* msg)
{
    ApplRSUBase::handleSelfMsg(msg);
}


void ApplRSUMonitor::executeEachTimeStep()
{
    ApplRSUBase::executeEachTimeStep();
}


void ApplRSUMonitor::onBeaconVehicle(BeaconVehicle* wsm)
{
    activeDetection = par("activeDetection").boolValue();
    if(activeDetection)
        onBeaconAny(wsm);
}


void ApplRSUMonitor::onBeaconBicycle(BeaconBicycle* wsm)
{
    activeDetection = par("activeDetection").boolValue();
    if(activeDetection)
        onBeaconAny(wsm);
}


void ApplRSUMonitor::onBeaconPedestrian(BeaconPedestrian* wsm)
{
    activeDetection = par("activeDetection").boolValue();
    if(activeDetection)
        onBeaconAny(wsm);
}


void ApplRSUMonitor::onBeaconRSU(BeaconRSU* wsm)
{

}


void ApplRSUMonitor::onData(LaneChangeMsg* wsm)
{

}


void ApplRSUMonitor::getAllLanes()
{
    // we need this RSU to be associated with a TL
    if(myTLid == "")
        error("The id of %s does not match with any TL. Check RSUsLocation.xml file!", myFullId);

    // for each incoming lane in this TL
    std::list<std::string> lan = TraCI->TLGetControlledLanes(myTLid);

    // remove duplicate entries
    lan.unique();

    // for each incoming lane
    for(auto &it2 :lan)
    {
        std::string lane = it2;

        lanesTL[lane] = myTLid;

        // get the max speed on this lane
        double maxV = TraCI->laneGetMaxSpeed(lane);

        // calculate initial passageTime for this lane
        // todo: change fix value
        double pass = 35. / maxV;

        // check if not greater than Gmin
        if(pass > minGreenTime)
        {
            std::cout << "WARNING (" << myFullId << "): Passage time is greater than Gmin in lane " << lane << endl;
            pass = minGreenTime;
        }

        // add this lane to the laneInfo map
        laneInfoEntry *entry = new laneInfoEntry(myTLid, 0, 0, pass, 0, std::map<std::string, queuedVehiclesEntry>());
        laneInfo.insert( std::make_pair(lane, *entry) );
    }
}


// update variables upon reception of any beacon (vehicle, bike, pedestrian)
template <typename T> void ApplRSUMonitor::onBeaconAny(T wsm)
{
    if(lanesTL.empty())
        getAllLanes();

    std::string sender = wsm->getSender();
    Coord pos = wsm->getPos();

    // If in the detection region:
    // todo: change from fix values
    // coordinates are the exact location of the middle of the LD
    if ( (pos.x >= 851.1 /*x-pos of west LD*/) && (pos.x <= 948.8 /*x-pos of east LD*/) && (pos.y >= 851.1 /*y-pos of north LD*/) && (pos.y <= 948.8 /*y-pos of north LD*/) )
    {
        std::string lane = wsm->getLane();

        // If on one of the incoming lanes
        if(lanesTL.find(lane) != lanesTL.end() && lanesTL[lane] == myTLid)
        {
            // search queue for this vehicle
            const detectedVehicleEntry *searchFor = new detectedVehicleEntry(sender);
            std::vector<detectedVehicleEntry>::iterator counter = std::find(Vec_detectedVehicles.begin(), Vec_detectedVehicles.end(), *searchFor);

            // return, if we have already added it
            if (counter != Vec_detectedVehicles.end() && counter->leaveTime == -1)
            {
                return;
            }
            // the vehicle is visiting the intersection more than once
            else if (counter != Vec_detectedVehicles.end() && counter->leaveTime != -1)
            {
                counter->entryTime = simTime().dbl();
                counter->entrySpeed = wsm->getSpeed();
                counter->pos = wsm->getPos();
                counter->leaveTime = -1;
            }
            else
            {
                // Add entry
                detectedVehicleEntry *tmp = new detectedVehicleEntry(sender, wsm->getSenderType(), lane, wsm->getPos(), myTLid, simTime().dbl(), -1, wsm->getSpeed());
                Vec_detectedVehicles.push_back(*tmp);
            }

            LaneInfoAdd(lane, sender, wsm->getSenderType(), wsm->getSpeed());
        }
        // Else exiting queue area, so log leave time
        else
        {
            // search queue for this vehicle
            const detectedVehicleEntry *searchFor = new detectedVehicleEntry(sender);
            std::vector<detectedVehicleEntry>::iterator counter = std::find(Vec_detectedVehicles.begin(), Vec_detectedVehicles.end(), *searchFor);

            if (counter == Vec_detectedVehicles.end())
                error("vehicle %s does not exist in the queue!", sender.c_str());

            if(counter->leaveTime == -1)
            {
                counter->leaveTime = simTime().dbl();

                LaneInfoRemove(counter->lane, sender);
            }
        }
    }
}


void ApplRSUMonitor::saveVehApproach()
{
    boost::filesystem::path filePath;

    if(ev.isGUI())
    {
        filePath = "results/gui/vehApproach.txt";
    }
    else
    {
        // get the current run number
        int currentRun = ev.getConfigEx()->getActiveRunNumber();
        std::ostringstream fileName;
        fileName << std::setfill('0') << std::setw(3) << currentRun << "_vehApproach.txt";
        filePath = "results/cmd/" + fileName.str();
    }

    FILE *filePtr = fopen (filePath.string().c_str(), "w");

    // write simulation parameters at the beginning of the file in CMD mode
    if(!ev.isGUI())
    {
        // get the current config name
        std::string configName = ev.getConfigEx()->getVariable("configname");

        // get number of total runs in this config
        int totalRun = ev.getConfigEx()->getNumRunsInConfig(configName.c_str());

        // get the current run number
        int currentRun = ev.getConfigEx()->getActiveRunNumber();

        // get all iteration variables
        std::vector<std::string> iterVar = ev.getConfigEx()->unrollConfig(configName.c_str(), false);

        // write to file
        fprintf (filePtr, "configName      %s\n", configName.c_str());
        fprintf (filePtr, "totalRun        %d\n", totalRun);
        fprintf (filePtr, "currentRun      %d\n", currentRun);
        fprintf (filePtr, "currentConfig   %s\n\n\n", iterVar[currentRun].c_str());
    }

    // write header
    fprintf (filePtr, "%-20s","vehicleName");
    fprintf (filePtr, "%-20s","vehicleType");
    fprintf (filePtr, "%-15s","lane");
    fprintf (filePtr, "%-15s","posX");
    fprintf (filePtr, "%-15s","posY");
    fprintf (filePtr, "%-15s","TLid");
    fprintf (filePtr, "%-15s","entryTime");
    fprintf (filePtr, "%-15s","entrySpeed");
    fprintf (filePtr, "%-15s\n\n","leaveTime");

    // write body
    for(std::vector<detectedVehicleEntry>::iterator y = Vec_detectedVehicles.begin(); y != Vec_detectedVehicles.end(); ++y)
    {
        fprintf (filePtr, "%-20s ", (*y).vehicleName.c_str());
        fprintf (filePtr, "%-20s ", (*y).vehicleType.c_str());
        fprintf (filePtr, "%-13s ", (*y).lane.c_str());
        fprintf (filePtr, "%-15.2f ", (*y).pos.x);
        fprintf (filePtr, "%-15.2f ", (*y).pos.y);
        fprintf (filePtr, "%-15s ", (*y).TLid.c_str());
        fprintf (filePtr, "%-15.2f ", (*y).entryTime);
        fprintf (filePtr, "%-15.2f", (*y).entrySpeed);
        fprintf (filePtr, "%-15.2f\n", (*y).leaveTime);
    }

    fclose(filePtr);
}


// update laneInfo
void ApplRSUMonitor::LaneInfoAdd(std::string lane, std::string sender, std::string senderType, double speed)
{
    // look for this lane in laneInfo map
    std::map<std::string, laneInfoEntry>::iterator loc = laneInfo.find(lane);
    if(loc == laneInfo.end())
        error("lane %s does not exist in laneInfo map!", lane.c_str());

    // update total vehicle count
    loc->second.totalVehCount = loc->second.totalVehCount + 1;

    // this is the first vehicle on this lane
    if(loc->second.totalVehCount == 1)
        loc->second.firstDetectedTime = simTime().dbl();

    // update detectedTime
    loc->second.lastDetectedTime = simTime().dbl();

    // add it as a queued vehicle on this lane
    queuedVehiclesEntry *newVeh = new queuedVehiclesEntry( senderType, simTime().dbl(), speed );
    loc->second.queuedVehicles.insert( std::make_pair(sender, *newVeh) );

    // get the approach speed from the beacon
    double approachSpeed = speed;
    // update passage time for this lane
    if(approachSpeed > 0)
    {
        // calculate passageTime for this lane
        // todo: change fix value
        double pass = 35. / approachSpeed;
        // check if not greater than Gmin
        if(pass > minGreenTime)
            pass = minGreenTime;

        // update passage time
        std::map<std::string, laneInfoEntry>::iterator location = laneInfo.find(lane);
        location->second.passageTime = pass;
    }
}


// update laneInfo
void ApplRSUMonitor::LaneInfoRemove(std::string lane, std::string sender)
{
    // look for this lane in laneInfo map
    std::map<std::string, laneInfoEntry>::iterator loc = laneInfo.find(lane);
    if(loc == laneInfo.end())
        error("lane %s does not exist in laneInfo map!", lane.c_str());

    // remove it from the queued vehicles
    std::map<std::string, queuedVehiclesEntry>::iterator ref = loc->second.queuedVehicles.find(sender);
    if(ref != loc->second.queuedVehicles.end())
        loc->second.queuedVehicles.erase(ref);
    else
        error("vehicle %s was not added into lane %s in laneInfo map!", sender.c_str(), lane.c_str());
}

}
