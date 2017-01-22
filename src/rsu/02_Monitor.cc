/****************************************************************************/
/// @file    Monitor.cc
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

#include <algorithm>
#include <iomanip>

#undef ev
#include "boost/filesystem.hpp"

#include "rsu/02_Monitor.h"

namespace VENTOS {

std::vector<detectedVehicleEntry> ApplRSUMonitor::Vec_detectedVehicles;

Define_Module(VENTOS::ApplRSUMonitor);

ApplRSUMonitor::~ApplRSUMonitor()
{

}


void ApplRSUMonitor::initialize(int stage)
{
    super::initialize(stage);

    if (stage == 0)
    {
        // note that TLs set 'activeDetection' after RSU creation
        activeDetection = par("activeDetection").boolValue();
        collectVehApproach = par("collectVehApproach").boolValue();

        if(myTLid != "")
        {
            // for each incoming lane in this TL
            auto lanes = TraCI->TLGetControlledLanes(myTLid);

            // remove duplicate entries
            sort( lanes.begin(), lanes.end() );
            lanes.erase( unique( lanes.begin(), lanes.end() ), lanes.end() );

            // for each incoming lane
            for(auto &lane :lanes)
            {
                lanesTL[lane] = myTLid;

                // get the max speed on this lane
                double maxV = TraCI->laneGetMaxSpeed(lane);

                // calculate initial passageTime for this lane -- passage time is used in actuated TSC
                // todo: change fix value
                double pass = 35. / maxV;

                // check if not greater than Gmin
                if(pass > minGreenTime)
                {
                    LOG_WARNING << boost::format("\nWARNING: Passage time in lane %1% which is controlled by %2% is greater than Gmin \n") % lane % myFullId << std::flush;

                    pass = minGreenTime;
                }

                // add this lane to the laneInfo map
                laneInfoEntry_t entry = {};
                entry.TLid = myTLid;
                entry.passageTime = pass;
                laneInfo.insert( std::make_pair(lane, entry) );
            }
        }

        // todo
        // else if(!TLlist.empty())
        //    LOG_WARNING << boost::format("\n%1%'s name (%2%) does not match with any of %3% TLs \n") % myFullId % SUMOID % TLlist.size() << std::flush;
    }
}


void ApplRSUMonitor::finish()
{
    super::finish();

    // only one of the RSUs prints the results
    static bool wasExecuted = false;
    if (activeDetection && collectVehApproach && !wasExecuted)
    {
        save_VehApproach_toFile();
        wasExecuted = true;
    }
}


void ApplRSUMonitor::handleSelfMsg(omnetpp::cMessage* msg)
{
    super::handleSelfMsg(msg);
}


void ApplRSUMonitor::executeEachTimeStep()
{
    super::executeEachTimeStep();
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


// update variables upon reception of any beacon (vehicle, bike, pedestrian)
template <typename T> void ApplRSUMonitor::onBeaconAny(T wsm)
{
    Coord pos = wsm->getPos();

    // todo: change from fix values
    // Check if entity is in the detection region:
    // Coordinates can be locations of the middle of the LD ( 851.1 <= x <= 948.8 and 851.1 <= y <= 948.8)
    if ( (pos.x >= 486.21) && (pos.x <= 1313.91) && (pos.y >= 486.21) && (pos.y <= 1313.85) )
    {
        std::string sender = wsm->getSender();
        std::string lane = wsm->getLane();

        // If on one of the incoming lanes
        if(lanesTL.find(lane) != lanesTL.end() && lanesTL[lane] == myTLid)
        {
            // search queue for this vehicle
            const detectedVehicleEntry *searchFor = new detectedVehicleEntry(sender);
            std::vector<detectedVehicleEntry>::iterator counter = std::find(Vec_detectedVehicles.begin(), Vec_detectedVehicles.end(), *searchFor);

            if(counter == Vec_detectedVehicles.end())
            {
                // Add entry
                detectedVehicleEntry *tmp = new detectedVehicleEntry(sender, wsm->getSenderType(), lane, wsm->getPos(), myTLid, omnetpp::simTime().dbl(), -1, wsm->getSpeed());
                Vec_detectedVehicles.push_back(*tmp);

                LaneInfoAdd(lane, sender, wsm->getSenderType(), wsm->getSpeed());
            }
            // we have already added this vehicle
            else if (counter != Vec_detectedVehicles.end() && counter->leaveTime == -1)
            {
                LaneInfoUpdate(lane, sender, wsm->getSenderType(), wsm->getSpeed());
            }
            // the vehicle is visiting the intersection more than once
            else if (counter != Vec_detectedVehicles.end() && counter->leaveTime != -1)
            {
                counter->entryTime = omnetpp::simTime().dbl();
                counter->entrySpeed = wsm->getSpeed();
                counter->pos = wsm->getPos();
                counter->leaveTime = -1;

                LaneInfoAdd(lane, sender, wsm->getSenderType(), wsm->getSpeed());
            }
        }
        // vehicle is exiting the queue area --log the leave time
        else
        {
            // search queue for this vehicle
            const detectedVehicleEntry *searchFor = new detectedVehicleEntry(sender);
            std::vector<detectedVehicleEntry>::iterator counter = std::find(Vec_detectedVehicles.begin(), Vec_detectedVehicles.end(), *searchFor);

            if (counter == Vec_detectedVehicles.end())
                throw omnetpp::cRuntimeError("vehicle %s does not exist in the queue!", sender.c_str());

            if(counter->leaveTime == -1)
            {
                counter->leaveTime = omnetpp::simTime().dbl();

                LaneInfoRemove(counter->lane, sender);
            }
        }
    }
}


// add a new vehicle
void ApplRSUMonitor::LaneInfoAdd(std::string lane, std::string sender, std::string senderType, double speed)
{
    // look for this lane in laneInfo map
    std::map<std::string, laneInfoEntry>::iterator loc = laneInfo.find(lane);
    if(loc == laneInfo.end())
        throw omnetpp::cRuntimeError("lane %s does not exist in laneInfo map!", lane.c_str());

    // update total vehicle count
    loc->second.totalVehCount = loc->second.totalVehCount + 1;

    // this is the first vehicle on this lane
    if(loc->second.totalVehCount == 1)
        loc->second.firstDetectedTime = omnetpp::simTime().dbl();

    // update detectedTime
    loc->second.lastDetectedTime = omnetpp::simTime().dbl();

    // get stopping speed threshold
    double stoppingDelayThreshold = 0;
    if(senderType == "bicycle")
        stoppingDelayThreshold = TLptr->par("bikeStoppingDelayThreshold").doubleValue();
    else
        stoppingDelayThreshold = TLptr->par("vehStoppingDelayThreshold").doubleValue();

    // get vehicle status
    int vehStatus = speed > stoppingDelayThreshold ? VEH_STATUS_Driving : VEH_STATUS_Waiting;

    // add it to vehicle list on this lane
    allVehiclesEntry_t newVeh = {};
    newVeh.entrySpeed = speed;
    newVeh.entryTime = omnetpp::simTime().dbl();
    newVeh.vehStatus = vehStatus;
    newVeh.vehType = senderType;
    loc->second.allVehicles.insert( std::make_pair(sender, newVeh) );

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


// update vehStatus of vehicles in laneInfo
void ApplRSUMonitor::LaneInfoUpdate(std::string lane, std::string sender, std::string senderType, double speed)
{
    // look for this lane in laneInfo map
    std::map<std::string, laneInfoEntry>::iterator loc = laneInfo.find(lane);
    if(loc == laneInfo.end())
        throw omnetpp::cRuntimeError("lane %s does not exist in laneInfo map!", lane.c_str());

    // look for this vehicle in this lane
    std::map<std::string, allVehiclesEntry>::iterator ref = loc->second.allVehicles.find(sender);
    if(ref == loc->second.allVehicles.end())
        throw omnetpp::cRuntimeError("vehicle %s was not added into lane %s in laneInfo map!", sender.c_str(), lane.c_str());

    // get stopping speed threshold
    double stoppingDelayThreshold = 0;
    if(senderType == "bicycle")
        stoppingDelayThreshold = TLptr->par("bikeStoppingDelayThreshold").doubleValue();
    else
        stoppingDelayThreshold = TLptr->par("vehStoppingDelayThreshold").doubleValue();

    // get vehicle status
    int vehStatus = speed > stoppingDelayThreshold ? VEH_STATUS_Driving : VEH_STATUS_Waiting;

    ref->second.vehStatus = vehStatus;
}


// removing an existing vehicle
void ApplRSUMonitor::LaneInfoRemove(std::string lane, std::string sender)
{
    // look for this lane in laneInfo map
    std::map<std::string, laneInfoEntry>::iterator loc = laneInfo.find(lane);
    if(loc == laneInfo.end())
        throw omnetpp::cRuntimeError("lane %s does not exist in laneInfo map!", lane.c_str());

    // look for this vehicle in this lane
    std::map<std::string, allVehiclesEntry>::iterator ref = loc->second.allVehicles.find(sender);
    if(ref == loc->second.allVehicles.end())
        throw omnetpp::cRuntimeError("vehicle %s was not added into lane %s in laneInfo map!", sender.c_str(), lane.c_str());

    // remove it from the vehicles list
    loc->second.allVehicles.erase(ref);
}


void ApplRSUMonitor::save_VehApproach_toFile()
{
    int currentRun = omnetpp::getEnvir()->getConfigEx()->getActiveRunNumber();

    std::ostringstream fileName;
    fileName << boost::format("%03d_vehApproach.txt") % currentRun;

    boost::filesystem::path filePath ("results");
    filePath /= fileName.str();

    FILE *filePtr = fopen (filePath.c_str(), "w");
    if (!filePtr)
        throw omnetpp::cRuntimeError("Cannot create file '%s'", filePath.c_str());

    // write simulation parameters at the beginning of the file
    {
        // get the current config name
        std::string configName = omnetpp::getEnvir()->getConfigEx()->getVariable("configname");

        std::string iniFile = omnetpp::getEnvir()->getConfigEx()->getVariable("inifile");

        // PID of the simulation process
        std::string processid = omnetpp::getEnvir()->getConfigEx()->getVariable("processid");

        // globally unique identifier for the run, produced by
        // concatenating the configuration name, run number, date/time, etc.
        std::string runID = omnetpp::getEnvir()->getConfigEx()->getVariable("runid");

        // get number of total runs in this config
        int totalRun = omnetpp::getEnvir()->getConfigEx()->getNumRunsInConfig(configName.c_str());

        // get the current run number
        int currentRun = omnetpp::getEnvir()->getConfigEx()->getActiveRunNumber();

        // get all iteration variables
        std::vector<std::string> iterVar = omnetpp::getEnvir()->getConfigEx()->unrollConfig(configName.c_str(), false);

        // write to file
        fprintf (filePtr, "configName      %s\n", configName.c_str());
        fprintf (filePtr, "iniFile         %s\n", iniFile.c_str());
        fprintf (filePtr, "processID       %s\n", processid.c_str());
        fprintf (filePtr, "runID           %s\n", runID.c_str());
        fprintf (filePtr, "totalRun        %d\n", totalRun);
        fprintf (filePtr, "currentRun      %d\n", currentRun);
        fprintf (filePtr, "currentConfig   %s\n", iterVar[currentRun].c_str());
        fprintf (filePtr, "startDateTime   %s\n", TraCI->simulationGetStartTime().c_str());
        fprintf (filePtr, "endDateTime     %s\n", TraCI->simulationGetEndTime().c_str());
        fprintf (filePtr, "duration        %s\n\n\n", TraCI->simulationGetDuration().c_str());
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
    for(auto y : Vec_detectedVehicles)
    {
        fprintf (filePtr, "%-20s", y.vehicleName.c_str());
        fprintf (filePtr, "%-20s", y.vehicleType.c_str());
        fprintf (filePtr, "%-13s", y.lane.c_str());
        fprintf (filePtr, "%-15.2f", y.pos.x);
        fprintf (filePtr, "%-15.2f", y.pos.y);
        fprintf (filePtr, "%-15s", y.TLid.c_str());
        fprintf (filePtr, "%-15.2f", y.entryTime);
        fprintf (filePtr, "%-15.2f", y.entrySpeed);
        fprintf (filePtr, "%-15.2f\n", y.leaveTime);
    }

    fclose(filePtr);
}

}
