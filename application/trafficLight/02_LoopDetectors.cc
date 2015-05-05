/****************************************************************************/
/// @file    LoopDetectors.cc
/// @author  Mani Amoozadeh <maniam@ucdavis.edu>
/// @author
/// @date    April 2015
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

#include <02_LoopDetectors.h>

namespace VENTOS {

Define_Module(VENTOS::LoopDetectors);


LoopDetectors::~LoopDetectors()
{

}


void LoopDetectors::initialize(int stage)
{
    TrafficLightBase::initialize(stage);

    if(stage == 0)
    {
        collectInductionLoopData = par("collectInductionLoopData").boolValue();
        measureIntersectionQueue = par("measureIntersectionQueue").boolValue();
        measureTrafficDemand = par("measureTrafficDemand").boolValue();

        LD_demand.clear();
        LD_actuated_start.clear();
        LD_actuated_end.clear();
        laneQueueSize.clear();
    }
}


void LoopDetectors::finish()
{
    TrafficLightBase::finish();

}


void LoopDetectors::handleMessage(cMessage *msg)
{
    TrafficLightBase::handleMessage(msg);

}


void LoopDetectors::executeFirstTimeStep()
{
    TrafficLightBase::executeFirstTimeStep();

    getAllDetectors();
}


void LoopDetectors::executeEachTimeStep(bool simulationDone)
{
    TrafficLightBase::executeEachTimeStep(simulationDone);

    if(collectInductionLoopData)
    {
        inductionLoops();    // collecting induction loop data in each timeStep

        if(ev.isGUI())
            inductionLoopToFile();  // (if in GUI) write to file what we have collected so far
        else if(simulationDone)
            inductionLoopToFile();  // (if in CMD) write to file at the end of simulation
    }

    if(measureTrafficDemand)
        trafficDemand();

    if(measureIntersectionQueue)
        measureQueue();
}


void LoopDetectors::getAllDetectors()
{
    // get all loop detectors
    list<string> str = TraCI->LDGetIDList();

    // for each loop detector
    for (list<string>::iterator it=str.begin(); it != str.end(); ++it)
    {
        string lane = TraCI->LDGetLaneID(*it);

        if( string(*it).find("demand_") != std::string::npos )
            LD_demand.insert(pair<string, string>(lane, *it));
        else if( string(*it).find("actuated_") != std::string::npos )
        {
            if( string(*it).find("start_") != std::string::npos )
                LD_actuated_start.insert(pair<string, string>(lane, *it));
            else if( string(*it).find("end_") != std::string::npos )
                LD_actuated_end.insert(pair<string, string>(lane, *it));
        }
    }

    cout << endl << LD_demand.size() << " demand loop detectors found!" << endl;
    cout << LD_actuated_start.size() << " actuated_start loop detectors found!" << endl;
    cout << LD_actuated_end.size() << " actuated_end loop detectors found!" << endl << endl;

    // some traffic signal controls need actuated LD on incoming lanes
    if(TLControlMode == 2)
    {
        for (list<string>::iterator it = TLList.begin(); it != TLList.end(); it++)
        {
            list<string> lan = TraCI->TLGetControlledLanes(*it);

            // remove duplicate entries
            lan.unique();

            // for each incoming lane
            for(list<string>::iterator it2 = lan.begin(); it2 != lan.end(); ++it2)
            {
                if( LD_actuated_end.find(*it2) == LD_actuated_end.end() )
                    cout << "WARNING: no loop detector found on lane (" << *it2 << "). No actuation is available for this lane." << endl;
            }
        }
        cout << endl;
    }

    // if we are measuring queue length then
    // make sure all actuated detectors are in pair
    if(measureIntersectionQueue)
    {
        for (list<string>::iterator it = TLList.begin() ; it != TLList.end(); ++it)
        {
            list<string> lan = TraCI->TLGetControlledLanes(*it);

            // remove duplicate entries
            lan.unique();

            // for each incoming lane
            for(list<string>::iterator it2 = lan.begin(); it2 != lan.end(); ++it2)
            {
                bool exists1 = LD_actuated_start.find(*it2) != LD_actuated_start.end();
                bool exists2 = LD_actuated_end.find(*it2) != LD_actuated_end.end();

                if( !exists1 && !exists2 )
                    cout << "WARNING: no loop detector found on lane (" << *it2 << "). No queue measurement is available for this lane." << endl;
                else if( !exists1 && exists2 )
                    error("Matching loop detector for (%s) not found!", LD_actuated_end[*it2].c_str() );
                else if( exists1 && !exists2 )
                    error("Matching loop detector for (%s) not found!", LD_actuated_start[*it2].c_str() );
            }
        }
        cout << endl;
    }
}


void LoopDetectors::inductionLoops()
{
    // get all loop detectors
    list<string> str = TraCI->LDGetIDList();

    // for each loop detector
    for (list<string>::iterator it=str.begin(); it != str.end(); ++it)
    {
        vector<string>  st = TraCI->LDGetLastStepVehicleData(*it);

        // only if this loop detector detected a vehicle
        if( st.size() > 0 )
        {
            // laneID of loop detector
            string lane = TraCI->LDGetLaneID(*it);

            // get vehicle information
            string vehicleName = st.at(0);
            double entryT = atof( st.at(2).c_str() );
            double leaveT = atof( st.at(3).c_str() );
            double speed = TraCI->LDGetLastStepMeanVehicleSpeed(*it);  // vehicle speed at current moment

            // save it only when collectInductionLoopData=true
            if(collectInductionLoopData)
            {
                int counter = findInVector(Vec_loopDetectors, (*it).c_str(), vehicleName.c_str());

                // its a new entry, so we add it
                if(counter == -1)
                {
                    LoopDetectorData *tmp = new LoopDetectorData( (*it).c_str(), lane.c_str(), vehicleName.c_str(), entryT, leaveT, speed, speed );
                    Vec_loopDetectors.push_back(tmp);
                }
                // if found, just update leaveTime and leaveSpeed
                else
                {
                    Vec_loopDetectors[counter]->leaveTime = leaveT;
                    Vec_loopDetectors[counter]->leaveSpeed = speed;
                }
            }
        }
    }
}


void LoopDetectors::inductionLoopToFile()
{
    boost::filesystem::path filePath;

    if( ev.isGUI() )
    {
        filePath = "results/gui/loopDetector.txt";
    }
    else
    {
        // get the current run number
        int currentRun = ev.getConfigEx()->getActiveRunNumber();
        ostringstream fileName;
        fileName << currentRun << "_loopDetector.txt";
        filePath = "results/cmd/" + fileName.str();
    }

    FILE *filePtr = fopen (filePath.string().c_str(), "w");

    // write header
    fprintf (filePtr, "%-30s","loopDetector");
    fprintf (filePtr, "%-20s","lane");
    fprintf (filePtr, "%-20s","vehicleName");
    fprintf (filePtr, "%-20s","vehicleEntryTime");
    fprintf (filePtr, "%-20s","vehicleLeaveTime");
    fprintf (filePtr, "%-22s","vehicleEntrySpeed");
    fprintf (filePtr, "%-22s\n\n","vehicleLeaveSpeed");

    // write body
    for(vector<LoopDetectorData *>::iterator y = Vec_loopDetectors.begin(); y != Vec_loopDetectors.end(); y++)
    {
        fprintf (filePtr, "%-30s ", (*y)->detectorName);
        fprintf (filePtr, "%-20s ", (*y)->lane);
        fprintf (filePtr, "%-20s ", (*y)->vehicleName);
        fprintf (filePtr, "%-20.2f ", (*y)->entryTime);
        fprintf (filePtr, "%-20.2f ", (*y)->leaveTime);
        fprintf (filePtr, "%-20.2f ", (*y)->entrySpeed);
        fprintf (filePtr, "%-20.2f\n", (*y)->leaveSpeed);
    }

    fclose(filePtr);
}


int LoopDetectors::findInVector(vector<LoopDetectorData *> Vec, const char *detectorName, const char *vehicleName)
{
    unsigned int counter;
    bool found = false;

    for(counter = 0; counter < Vec.size(); counter++)
    {
        if( strcmp(Vec[counter]->detectorName, detectorName) == 0 && strcmp(Vec[counter]->vehicleName, vehicleName) == 0)
        {
            found = true;
            break;
        }
    }

    if(!found)
        return -1;
    else
        return counter;
}


// todo: dynamic demand calculation does not make sense!
void LoopDetectors::trafficDemand()
{
    // for each loop detector that measures the traffic demand
    for (map<string,string>::iterator it=LD_demand.begin(); it != LD_demand.end(); ++it)
    {
        if( string( (*it).second ) == "demand_WC_3" )
        {
            double lastDetectionT = TraCI->LDGetLastDetectionTime( (*it).second );

            if(lastDetectionT == 0 && !freeze)
            {
                passedVeh++;
                freeze = true;

                if(passedVeh == 1)
                    total = lastDetectionT_old = 0;
            }

            if(freeze && lastDetectionT != 0)
                freeze = false;

            total = total + lastDetectionT_old;

            //cout << endl << passedVeh << ", " << total << ", " << (3600 * passedVeh)/total << endl;

            lastDetectionT_old = lastDetectionT;
        }
    }
}


void LoopDetectors::measureQueue()
{
    for (list<string>::iterator it = TLList.begin() ; it != TLList.end(); ++it)
    {
        list<string> lan = TraCI->TLGetControlledLanes(*it);

        // remove duplicate entries
        lan.unique();

        // for each incoming lane
        for(list<string>::iterator it2 = lan.begin(); it2 != lan.end(); ++it2)
        {
            // make sure we have both detectors
            if( LD_actuated_end.find(*it2) == LD_actuated_end.end() || LD_actuated_start.find(*it2) == LD_actuated_start.end() )
                break;

            string queueEnd = LD_actuated_end[*it2];
            string queueStart = LD_actuated_start[*it2];

            vector<string>  st1 = TraCI->LDGetLastStepVehicleData(queueEnd);
            // only if this LP detected a vehicle
            if( st1.size() > 0 )
            {
                double leaveT = atof( st1.at(3).c_str() );

                // one vehicle entered
                if(leaveT != -1)
                {
                    map<string,pair<string,int>>::iterator location = laneQueueSize.find(*it2);

                    // its a new entry, so we add it
                    if( location == laneQueueSize.end() )
                    {
                        laneQueueSize.insert( make_pair( *it2, make_pair(*it,1) ) );
                    }
                    // if found, just update queue information
                    else
                    {
                        pair<string,int> store = location->second;
                        location->second = make_pair( store.first, (store.second)+1 );
                    }
                }
            }

            vector<string>  st2 = TraCI->LDGetLastStepVehicleData(queueStart);
            // only if this LD detected a vehicle
            if( st2.size() > 0 )
            {
                double leaveT = atof( st2.at(3).c_str() );

                // one vehicle entered
                if(leaveT != -1)
                {
                    map<string,pair<string,int>>::iterator location = laneQueueSize.find(*it2);

                    if(location == laneQueueSize.end())
                    {
                        error("The vehicle is removed from queue of %s without being inserted!", (*it2).c_str());
                    }
                    // just update queue information
                    else
                    {
                        pair<string,int> store = location->second;
                        location->second = make_pair( store.first, (store.second)-1 );
                    }
                }
            }
        }
    }
}

}
