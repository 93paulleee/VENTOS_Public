/****************************************************************************/
/// @file    TL_Webster.cc
/// @author  Mani Amoozadeh <maniam@ucdavis.edu>
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

#include <02_TL_Adaptive_Webster.h>
#include <algorithm>

namespace VENTOS {

Define_Module(VENTOS::TrafficLightWebster);


TrafficLightWebster::~TrafficLightWebster()
{

}


void TrafficLightWebster::initialize(int stage)
{
    super::initialize(stage);

    if(TLControlMode != TL_Adaptive_Webster)
        return;

    if(stage == 0)
    {
        alpha = par("alpha").doubleValue();
        if(alpha < 0 || alpha > 1)
            throw omnetpp::cRuntimeError("alpha value should be [0,1]");

        intervalChangeEVT = new omnetpp::cMessage("intervalChangeEVT", 1);

        greenSplit.clear();

        // measure traffic demand
        measureTrafficDemand = true;
    }
}


void TrafficLightWebster::finish()
{
    super::finish();
}


void TrafficLightWebster::handleMessage(omnetpp::cMessage *msg)
{
    super::handleMessage(msg);

    if(TLControlMode != TL_Adaptive_Webster)
        return;

    if (msg == intervalChangeEVT)
    {
        chooseNextInterval();

        if(intervalDuration <= 0)
            throw omnetpp::cRuntimeError("intervalDuration is <= 0");

        // Schedule next light change event:
        scheduleAt(omnetpp::simTime().dbl() + intervalDuration, intervalChangeEVT);
    }
}


void TrafficLightWebster::initialize_withTraCI()
{
    super::initialize_withTraCI();

    if(TLControlMode != TL_Adaptive_Webster)
        return;

    std::cout << std::endl << "Adaptive Webster traffic signal control ... " << std::endl << std::endl;

    // run Webster at the beginning of the cycle
    calculateGreenSplits();

    // set initial values
    currentInterval = phase1_5;
    intervalDuration = greenSplit[phase1_5];

    scheduleAt(omnetpp::simTime().dbl() + intervalDuration, intervalChangeEVT);

    for (auto &TL : TLList)
    {
        TraCI->TLSetProgram(TL, "adaptive-time");
        TraCI->TLSetState(TL, currentInterval);

        firstGreen[TL] = currentInterval;

        // initialize TL status
        updateTLstate(TL, "init", currentInterval);
    }

    if(omnetpp::cSimulation::getActiveEnvir()->isGUI() && debugLevel > 0)
    {
        char buff[300];
        sprintf(buff, "SimTime: %4.2f | Planned interval: %s | Start time: %4.2f | End time: %4.2f", omnetpp::simTime().dbl(), currentInterval.c_str(), omnetpp::simTime().dbl(), omnetpp::simTime().dbl() + intervalDuration);
        std::cout << buff << std::endl << std::endl;
        std::cout.flush();
    }
}


void TrafficLightWebster::executeEachTimeStep()
{
    super::executeEachTimeStep();

    if(TLControlMode != TL_Adaptive_Webster)
        return;
}


void TrafficLightWebster::chooseNextInterval()
{
    if (currentInterval == "yellow")
    {
        currentInterval = "red";

        // change all 'y' to 'r'
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
        intervalDuration = redTime;

        // update TL status for this phase
        updateTLstate("C", "red");
    }
    else if (currentInterval == "red")
    {
        // update TL status for this phase
        if(nextGreenInterval == firstGreen["C"])
        {
            updateTLstate("C", "phaseEnd", nextGreenInterval, true);  // new cycle
            calculateGreenSplits();  // run Webster at the beginning of the cycle
        }
        else
            updateTLstate("C", "phaseEnd", nextGreenInterval);

        currentInterval = nextGreenInterval;

        // set the new state
        TraCI->TLSetState("C", nextGreenInterval);
        intervalDuration = greenSplit[nextGreenInterval];
    }
    else
        chooseNextGreenInterval();

    if(omnetpp::cSimulation::getActiveEnvir()->isGUI() && debugLevel > 0)
    {
        char buff[300];
        sprintf(buff, "SimTime: %4.2f | Planned interval: %s | Start time: %4.2f | End time: %4.2f", omnetpp::simTime().dbl(), currentInterval.c_str(), omnetpp::simTime().dbl(), omnetpp::simTime().dbl() + intervalDuration);
        std::cout << buff << std::endl << std::endl;
        std::cout.flush();
    }
}


void TrafficLightWebster::chooseNextGreenInterval()
{
    std::string nextInterval;

    if(currentInterval == phase1_5)
    {
        nextGreenInterval = phase2_6;
        nextInterval = "grgrygrgrrgrgrygrgrrrrrr";
    }
    else if(currentInterval == phase2_6)
    {
        nextGreenInterval = phase3_7;
        nextInterval = "gygyrgrgrrgygyrgrgrrryry";
    }
    else if(currentInterval == phase3_7)
    {
        nextGreenInterval = phase4_8;
        nextInterval = "grgrrgrgrygrgrrgrgryrrrr";
    }
    else if(currentInterval == phase4_8)
    {
        nextGreenInterval = phase1_5;
        nextInterval = "grgrrgygyrgrgrrgygyryryr";
    }

    currentInterval = "yellow";
    TraCI->TLSetState("C", nextInterval);

    intervalDuration =  yellowTime;

    // update TL status for this phase
    updateTLstate("C", "yellow");
}


void TrafficLightWebster::calculateGreenSplits()
{
    // debugging (print traffic demand)
    if(omnetpp::cSimulation::getActiveEnvir()->isGUI() && debugLevel > 1)
    {
        std::cout << ">>> Measured traffic demands at the beginning of this cycle: ";
        for(auto &y : laneTD)
        {
            std::string lane = y.first;
            std::string TLid = y.second.first;
            boost::circular_buffer<std::vector<double>> buf = y.second.second;

            double aveTD = 0;

            if(!buf.empty())
            {
                // calculate 'exponential moving average' of TD for link i
                aveTD = buf[0].at(0);  // get the oldest value (queue front)
                for (boost::circular_buffer<std::vector<double>>::iterator it = buf.begin()+1; it != buf.end(); ++it)
                {
                    double nextTD = (*it).at(0);
                    aveTD = alpha*nextTD + (1-alpha)*aveTD;
                }

                if(aveTD != 0)
                    std::cout << lane << ": " << aveTD << " | ";
            }
        }
        std::cout << std::endl << std::endl;
        std::cout.flush();
    }

    std::vector<double> critical;
    critical.clear();

    double Y = 0;
    for (std::string prog : phases)
    {
        double Y_i = -1;  // critical volume-to-capacity ratio for this movement batch
        // for each link in this batch
        for(unsigned int i = 0; i < prog.size(); ++i)
        {
            // if link i is active
            if(prog[i] == 'g' || prog[i] == 'G')
            {
                bool rightTurn = std::find(std::begin(rightTurns), std::end(rightTurns), i) != std::end(rightTurns);

                // if link i is not a right-turn (right turns are all permissive)
                if(!rightTurn)
                {
                    // get all TD measurements so far for link i
                    boost::circular_buffer<std::vector<double>> buffer = linkTD[std::make_pair("C",i)];

                    double aveTD = 0;

                    if(!buffer.empty())
                    {
                        // calculate 'exponential moving average' of TD for link i
                        aveTD = buffer[0].at(0);  // get the oldest value (queue front)
                        for (boost::circular_buffer<std::vector<double>>::iterator it = buffer.begin()+1; it != buffer.end(); ++it)
                        {
                            double nextTD = (*it).at(0);
                            aveTD = alpha*nextTD + (1-alpha)*aveTD;
                        }
                    }

                    Y_i = std::max(Y_i, aveTD / saturationTD);
                }
            }
        }

        critical.push_back(Y_i);
        Y = Y + Y_i;
    }

    // print Y_i for each phase
    if(omnetpp::cSimulation::getActiveEnvir()->isGUI() && debugLevel > 1) std::cout << ">>> critical v/c for each phase: ";
    int activePhases = 0;
    for(double y : critical)
    {
        if(omnetpp::cSimulation::getActiveEnvir()->isGUI() && debugLevel > 1) std::cout << y << ", ";
        if(y != 0) activePhases++;
    }
    if(omnetpp::cSimulation::getActiveEnvir()->isGUI() && debugLevel > 1) std::cout << std::endl << std::endl;

    if(Y < 0)
    {
        throw omnetpp::cRuntimeError("WTH! total critical v/c is negative!");
    }
    // no TD in any directions. Give G_min to each phase
    else if(Y == 0)
    {
        if(omnetpp::cSimulation::getActiveEnvir()->isGUI() && debugLevel > 0)
        {
            std::cout << ">>> Total critical v/c is zero! Set green split for each phase to G_min=" << minGreenTime << std::endl << std::endl;
            std::cout.flush();
        }

        // green split for each phase
        for (std::string prog : phases)
            greenSplit[prog] = minGreenTime;
    }
    else if(Y > 0 && Y < 1)
    {
        double startupLoss_i = 1.0;
        double changeInterval_i = yellowTime + redTime;
        double clearanceLoss_i = (1-0.8)*changeInterval_i;
        double totalLoss_i = startupLoss_i + clearanceLoss_i;   // total loss time per phase
        double totalLoss = totalLoss_i * activePhases;  // total loss time for all phases

        double cycle = ((1.5*totalLoss) + 5) / (1 - Y);  // cycle length

        if(omnetpp::cSimulation::getActiveEnvir()->isGUI() && debugLevel > 1)
        {
            std::cout << ">>> Webster Calculation: " << std::endl;
            std::cout << "total critical v/c=" << Y << ", total loss time=" << totalLoss << ", cycle length=" << cycle << std::endl;
            std::cout.flush();
        }

        // make sure that cycle length is not too big.
        // this happens when Y is too close to 1
        if(cycle > maxCycleLength)
        {
            std::cout << "WARNING: cycle length exceeds max C_y=" << maxCycleLength << std::endl;
            cycle = maxCycleLength;
        }

        double effectiveG = cycle - totalLoss;   // total effective green time

        // green split for each phase
        int phaseNumber = 0;
        for (std::string prog : phases)
        {
            double GS = (critical[phaseNumber] / Y) * effectiveG;
            greenSplit[prog] = std::min(std::max(minGreenTime, GS), maxGreenTime);
            phaseNumber++;
        }

        if(omnetpp::cSimulation::getActiveEnvir()->isGUI() && debugLevel > 1)
        {
            std::cout << "Updating green splits for each phase: ";
            for(auto &y : greenSplit)
                std::cout << y.second << ", ";
            std::cout << std::endl << std::endl;
            std::cout.flush();
        }
    }
    else if(Y >= 1)
    {
        throw omnetpp::cRuntimeError("total critical v/c >= 1. Saturation flow might be low ?!");
    }
}

}
