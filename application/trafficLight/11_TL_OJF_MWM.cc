/****************************************************************************/
/// @file    TL_OJF_MWM.cc
/// @author  Mani Amoozadeh <maniam@ucdavis.edu>
/// @date    Jul 2015
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

#include <11_TL_OJF_MWM.h>

namespace VENTOS {

Define_Module(VENTOS::TrafficLight_OJF_MWM);


TrafficLight_OJF_MWM::~TrafficLight_OJF_MWM()
{

}


void TrafficLight_OJF_MWM::initialize(int stage)
{
    TrafficLight_LQF_MWM::initialize(stage);

    if(TLControlMode != TL_OJF_MWM)
        return;

    if(stage == 0)
    {
        // NED variables
        greenExtension = par("greenExtension").boolValue();

        // turn on active detection
        activeDetection = true;
        this->par("activeDetection") = true;

        ChangeEvt = new cMessage("ChangeEvt", 1);
    }
}


void TrafficLight_OJF_MWM::finish()
{
    TrafficLight_LQF_MWM::finish();
}


void TrafficLight_OJF_MWM::handleMessage(cMessage *msg)
{
    TrafficLight_LQF_MWM::handleMessage(msg);

    if(TLControlMode != TL_OJF_MWM)
        return;

    if (msg == ChangeEvt)
    {
        chooseNextInterval();

        if(intervalOffSet <= 0)
            error("intervalOffSet is <= 0");

        // Schedule next light change event:
        scheduleAt(simTime().dbl() + intervalOffSet, ChangeEvt);
    }
}


void TrafficLight_OJF_MWM::executeFirstTimeStep()
{
    // call parent
    TrafficLight_LQF_MWM::executeFirstTimeStep();

    if(TLControlMode != TL_OJF_MWM)
        return;

    std::cout << "Multi-class OJF-MWM traffic signal control ..." << endl;

    // set initial values
    currentInterval = phase1_5;
    intervalOffSet = minGreenTime;
    intervalElapseTime = 0;

    scheduleAt(simTime().dbl() + intervalOffSet, ChangeEvt);

    for (std::list<std::string>::iterator TL = TLList.begin(); TL != TLList.end(); ++TL)
    {
        TraCI->TLSetProgram(*TL, "adaptive-time");
        TraCI->TLSetState(*TL, currentInterval);

        firstGreen[*TL] = currentInterval;

        // initialize TL status
        updateTLstate(*TL, "init", currentInterval);
    }

    // make sure RSUptr is pointing to our corresponding RSU
    ASSERT(RSUptr);

    char buff[300];
    sprintf(buff, "SimTime: %4.2f | Planned interval: %s | Start time: %4.2f | End time: %4.2f", simTime().dbl(), currentInterval.c_str(), simTime().dbl(), simTime().dbl() + intervalOffSet);
    std::cout << endl << buff << endl << endl;
}


void TrafficLight_OJF_MWM::executeEachTimeStep(bool simulationDone)
{
    // call parent
    TrafficLight_LQF_MWM::executeEachTimeStep(simulationDone);

    if(TLControlMode != TL_OJF_MWM)
        return;

    intervalElapseTime += updateInterval;

    // todo: remove later
//    std::map<std::string, laneInfoEntry> laneInfo = RSU->laneInfo;
//    for(std::map<std::string, laneInfoEntry>::iterator y = laneInfo.begin(); y != laneInfo.end(); ++y)
//    {
//        std::string lane = (*y).first;
//        laneInfoEntry entry = (*y).second;
//        std::map<std::string, queuedVehiclesEntry> vehs = entry.queuedVehicles;
//
//        std::cout << "lane: " << lane << ", TLid: " << entry.TLid << ", LDT: " << entry.lastDetectedTime << ", pass: " << entry.passageTime << ", queuedVeh: |";
//        for(std::map<std::string, queuedVehiclesEntry>::iterator z = vehs.begin(); z != vehs.end(); ++z)
//        {
//            std::cout << (*z).first << ", " << (*z).second.entryTime << ", " <<  (*z).second.entrySpeed << ", " << (*z).second.vehicleType << "|";
//        }
//        std::cout << endl;
//    }
//    std::cout << endl << endl;
}


void TrafficLight_OJF_MWM::chooseNextInterval()
{
    if (currentInterval == "yellow")
    {
        currentInterval = "red";

        std::string str = TraCI->TLGetState("C");
        std::string nextInterval = "";
        for(char& c : str) {
            if (c == 'y')
                nextInterval += 'r';
            else
                nextInterval += c;
        }

        // set the new state
        TraCI->TLSetState("C", nextInterval);
        intervalElapseTime = 0.0;
        intervalOffSet = redTime;

        // update TL status for this phase
        updateTLstate("C", "red");

        char buff[300];
        sprintf(buff, "SimTime: %4.2f | Planned interval: %s | Start time: %4.2f | End time: %4.2f", simTime().dbl(), currentInterval.c_str(), simTime().dbl(), simTime().dbl() + intervalOffSet);
        std::cout << buff << endl << endl;
    }
    else if (currentInterval == "red")
    {
        // update TL status for this phase
        if(nextGreenInterval == firstGreen["C"])
            updateTLstate("C", "phaseEnd", nextGreenInterval, true);
        else
            updateTLstate("C", "phaseEnd", nextGreenInterval);

        currentInterval = nextGreenInterval;

        // set the new state
        TraCI->TLSetState("C", nextGreenInterval);
        intervalElapseTime = 0.0;
        intervalOffSet = minGreenTime;

        char buff[300];
        sprintf(buff, "SimTime: %4.2f | Planned interval: %s | Start time: %4.2f | End time: %4.2f", simTime().dbl(), currentInterval.c_str(), simTime().dbl(), simTime().dbl() + intervalOffSet);
        std::cout << buff << endl << endl;
    }
    else
        chooseNextGreenInterval();
}


void TrafficLight_OJF_MWM::chooseNextGreenInterval()
{
    std::map<std::string, laneInfoEntry> laneInfo = RSUptr->laneInfo;

    std::map<std::string, double> lastDetectionTime;
    std::map<std::string, double> passageTime;

    for(std::map<std::string, laneInfoEntry>::iterator it = laneInfo.begin(); it != laneInfo.end(); ++it)
    {
        std::string lane = (*it).first;
        lastDetectionTime[lane] = (*it).second.lastDetectedTime;
        passageTime[lane] = (*it).second.passageTime;
    //    std::map<std::string /*vehicle id*/, queuedVehiclesEntry> queuedVehicles;
    }











//    bool extend = false;
//    std::string nextInterval;
//
//    // Do proper transition:
//    if (currentInterval == phase1_5)
//    {
//        if (greenExtension && intervalElapseTime < maxGreenTime &&
//                lastDetectionTime["NC_4"] < passageTime["NC_4"] &&
//                lastDetectionTime["SC_4"] < passageTime["SC_4"])
//        {
//            intervalOffSet = std::max(passageTime["NC_4"]-lastDetectionTime["NC_4"], passageTime["SC_4"]-lastDetectionTime["SC_4"]);
//            extend = true;
//        }
//        else if (lastDetectionTime["NC_4"] < passageTime["NC_4"])
//        {
//            nextGreenInterval = phase2_5;
//            nextInterval = "rrrrGrrrrrrrrryrrrrrrrrr";
//            extend = false;
//        }
//        else if (lastDetectionTime["SC_4"] < passageTime["SC_4"])
//        {
//            nextGreenInterval = phase1_6;
//            nextInterval = "rrrryrrrrrrrrrGrrrrrrrrr";
//            extend = false;
//        }
//        else
//        {
//            nextGreenInterval = phase2_6;
//            nextInterval = "rrrryrrrrrrrrryrrrrrrrrr";
//            extend = false;
//        }
//    }
//    else if (currentInterval == phase2_5)
//    {
//        if (greenExtension && intervalElapseTime < maxGreenTime &&
//                lastDetectionTime["NC_4"] < passageTime["NC_4"])
//        {
//            intervalOffSet = passageTime["NC_4"] - lastDetectionTime["NC_4"];
//            extend = true;
//        }
//        else
//        {
//            nextGreenInterval = phase2_6;
//            nextInterval = "gGgGyrrrrrrrrrrrrrrrrrrG";
//            extend = false;
//        }
//    }
//    else if (currentInterval == phase1_6)
//    {
//        if (greenExtension && intervalElapseTime < maxGreenTime &&
//                lastDetectionTime["SC_4"] < passageTime["SC_4"])
//        {
//            intervalOffSet = passageTime["SC_4"] - lastDetectionTime["SC_4"];
//            extend = true;
//        }
//        else
//        {
//            nextGreenInterval = phase2_6;
//            nextInterval = "rrrrrrrrrrgGgGyrrrrrrGrr";
//            extend = false;
//        }
//    }
//    else if (currentInterval == phase2_6)
//    {
//        if (greenExtension && intervalElapseTime < maxGreenTime &&
//                (lastDetectionTime["NC_2"] < passageTime["NC_2"] ||
//                        lastDetectionTime["NC_3"] < passageTime["NC_3"] ||
//                        lastDetectionTime["SC_2"] < passageTime["SC_2"] ||
//                        lastDetectionTime["SC_3"] < passageTime["SC_3"]))
//        {
//            double biggest1 = std::max(passageTime["NC_2"]-lastDetectionTime["NC_2"], passageTime["SC_2"]-lastDetectionTime["SC_2"]);
//            double biggest2 = std::max(passageTime["NC_3"]-lastDetectionTime["NC_3"], passageTime["SC_3"]-lastDetectionTime["SC_3"]);
//            intervalOffSet = std::max(biggest1, biggest2);
//            extend = true;
//        }
//        else
//        {
//            nextGreenInterval = phase3_7;
//            nextInterval = "yyyyrrrrrryyyyrrrrrrryry";
//            extend = false;
//        }
//    }
//    else if (currentInterval == phase3_7)
//    {
//        if (greenExtension && intervalElapseTime < maxGreenTime &&
//                lastDetectionTime["WC_4"] < passageTime["WC_4"] &&
//                lastDetectionTime["EC_4"] < passageTime["EC_4"])
//        {
//            intervalOffSet = std::max(passageTime["WC_4"]-lastDetectionTime["WC_4"], passageTime["EC_4"]-lastDetectionTime["EC_4"]);
//            extend = true;
//        }
//        else if (lastDetectionTime["WC_4"] < passageTime["WC_4"])
//        {
//            nextGreenInterval = phase3_8;
//            nextInterval = "rrrrrrrrryrrrrrrrrrGrrrr";
//            extend = false;
//        }
//        else if (lastDetectionTime["EC_4"] < passageTime["EC_4"])
//        {
//            nextGreenInterval = phase4_7;
//            nextInterval = "rrrrrrrrrGrrrrrrrrryrrrr";
//            extend = false;
//        }
//        else
//        {
//            nextGreenInterval = phase4_8;
//            nextInterval = "rrrrrrrrryrrrrrrrrryrrrr";
//            extend = false;
//        }
//    }
//    else if (currentInterval == phase3_8)
//    {
//        if (greenExtension && intervalElapseTime < maxGreenTime &&
//                lastDetectionTime["WC_4"] < passageTime["WC_4"])
//        {
//            intervalOffSet = passageTime["WC_4"] - lastDetectionTime["WC_4"];
//            extend = true;
//        }
//        else
//        {
//            nextGreenInterval = phase4_8;
//            nextInterval = "rrrrrrrrrrrrrrrgGgGyrrGr";
//            extend = false;
//        }
//    }
//    else if (currentInterval == phase4_7)
//    {
//        if (greenExtension && intervalElapseTime < maxGreenTime &&
//                lastDetectionTime["EC_4"] < passageTime["EC_4"])
//        {
//            intervalOffSet = passageTime["EC_4"] - lastDetectionTime["EC_4"];
//            extend = true;
//        }
//        else
//        {
//            nextGreenInterval = phase4_8;
//            nextInterval = "rrrrrgGgGyrrrrrrrrrrGrrr";
//            extend = false;
//        }
//    }
//    else if (currentInterval == phase4_8)
//    {
//        if (greenExtension && intervalElapseTime < maxGreenTime &&
//                (lastDetectionTime["WC_2"] < passageTime["WC_2"] ||
//                        lastDetectionTime["WC_3"] < passageTime["WC_3"] ||
//                        lastDetectionTime["EC_2"] < passageTime["EC_2"] ||
//                        lastDetectionTime["EC_3"] < passageTime["EC_3"]))
//        {
//            double biggest1 = std::max(passageTime["WC_2"]-lastDetectionTime["WC_2"], passageTime["EC_2"]-lastDetectionTime["EC_2"]);
//            double biggest2 = std::max(passageTime["WC_3"]-lastDetectionTime["WC_3"], passageTime["EC_3"]-lastDetectionTime["EC_3"]);
//            intervalOffSet = std::max(biggest1, biggest2);
//            extend = true;
//        }
//        else
//        {
//            nextGreenInterval = phase1_5;
//            nextInterval = "rrrrryyyyrrrrrryyyyryryr";
//            extend = false;
//        }
//    }
//
//    // the current green interval should be extended
//    if(extend)
//    {
//        // give a lower bound
//        intervalOffSet = std::max(updateInterval, intervalOffSet);
//
//        // interval duration after this offset
//        double newIntervalTime = intervalElapseTime + intervalOffSet;
//
//        // never extend past maxGreenTime
//        if (newIntervalTime > maxGreenTime)
//            intervalOffSet = intervalOffSet - (newIntervalTime - maxGreenTime);
//
//        // offset can not be too small
//        if(intervalOffSet < updateInterval)
//        {
//            intervalOffSet = 0.0001;
//            intervalElapseTime = maxGreenTime;
//            std::cout << ">>> Green extension offset is too small. Terminating the current phase ..." << endl << endl;
//        }
//        else
//            std::cout << ">>> Extending green for both movements by " << intervalOffSet << "s" << endl << endl;
//    }
//    // we should terminate the current green interval
//    else
//    {
//        currentInterval = "yellow";
//        TraCI->TLSetState("C", nextInterval);
//
//        intervalElapseTime = 0.0;
//        intervalOffSet =  yellowTime;
//
//        // update TL status for this phase
//        updateTLstate("C", "yellow");
//
//        char buff[300];
//        sprintf(buff, "SimTime: %4.2f | Planned interval: %s | Start time: %4.2f | End time: %4.2f", simTime().dbl(), currentInterval.c_str(), simTime().dbl(), simTime().dbl() + intervalOffSet);
//        std::cout << buff << endl << endl;
//    }
}

}
