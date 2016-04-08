/****************************************************************************/
/// @file    ApplV_03_System.h
/// @author  Dylan Smith <dilsmith@ucdavis.edu>
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

#include "ApplV_03_System.h"
#include "SignalObj.h"

namespace VENTOS {

Define_Module(VENTOS::ApplVSystem);

ApplVSystem::~ApplVSystem()
{

}


void ApplVSystem::initialize(int stage)
{
    ApplVBeacon::initialize(stage);

    if(SUMOType == "TypeDummy")
        requestRoutes = false;
    else
        requestRoutes = par("requestRoutes").boolValue();

    //If this vehicle is not supposed to send system messages
    if(!requestRoutes)
        return;

    if (stage == 0)
    {
        debugLevel = simulation.getSystemModule()->par("debugLevel").longValue();
        requestInterval = par("requestInterval").doubleValue();
        hypertreeUpdateInterval = par("hypertreeUpdateInterval").doubleValue();
        maxOffset = par("maxSystemOffset").doubleValue();
        systemMsgLengthBits = par("systemMsgLengthBits").longValue();
        systemMsgPriority = par("systemMsgPriority").longValue();
        routingMode = static_cast<RouterMessage>(par("routingMode").longValue());

        // get the rootFilePath
        cModule *module = simulation.getSystemModule()->getSubmodule("router");
        router = static_cast< Router* >(module);
        std::string rootFilePath = TraCI->getSUMOFullDir();
        rootFilePath += "/Vehicles" + std::to_string(router->totalVehicleCount) + ".xml";
        router->net->vehicles[SUMOID]->maxAccel = TraCI->vehicleGetMaxAccel(SUMOID);

        // Routing
        //Temporary fix to get a vehicle's target: get it from the xml
        rapidxml::file<> xmlFile( (rootFilePath).c_str() );          // Convert our file to a rapid-xml readable object
        rapidxml::xml_document<> doc;                                // Build a rapidxml doc
        doc.parse<0>(xmlFile.data());                                // Fill it with data from our file
        rapidxml::xml_node<> *node = doc.first_node("vehicles");     // Parse up to the "nodes" declaration
        for(node = node->first_node("vehicle"); node->first_attribute()->value() != SUMOID; node = node->next_sibling()); //Find our vehicle in the .xml

        rapidxml::xml_attribute<> *attr = node->first_attribute()->next_attribute()->next_attribute()->next_attribute();  //Navigate to the destination attribute
        if((std::string)attr->name() == "destination")  //Double-check
            targetNode = attr->value(); //And save it
        else
            error("XML formatted wrong! Some vehicle was missing its destination!");

        if(find(router->nonReroutingVehicles->begin(), router->nonReroutingVehicles->end(), SUMOID.substr(1, SUMOID.length() - 1)) != router->nonReroutingVehicles->end())
        {
            requestReroutes = false;
            if(ev.isGUI() && debugLevel > 1)
            {
                std::cout << SUMOID << " is not routing" << endl;
                std::cout.flush();
            }
        }
        else
            requestReroutes = true;

        numReroutes = 0;

        //Register to receive signals from the router
        Signal_router = registerSignal("router");
        simulation.getSystemModule()->subscribe("router", this);

        //Prepare to send a system message
        Signal_system = registerSignal("system");
        nodePtr->emit(Signal_system, new systemData("", "", SUMOID, STARTED, std::string("system")));

        //Slightly offset all vehicles (0-4 seconds)
        double systemOffset = dblrand() * maxOffset;

        sendSystemMsgEvt = new cMessage("systemmsg evt");   //Create a new internal message
        scheduleAt(simTime() + systemOffset, sendSystemMsgEvt); //Schedule them to start sending
    }
}


void ApplVSystem::finish()
{
    ApplVBeacon::finish();

    if(!requestRoutes)
        return;

    if(ev.isGUI() && debugLevel > 0)
    {
        std::cout << std::endl <<"t=" << simTime().dbl() << ": " << SUMOID << " took " << simTime().dbl() - entryTime << " seconds to complete its route." << endl;
        std::cout.flush();
    }

    router->vehicleEndTimesFile << SUMOID << " " << simTime().dbl() << endl;

    //Prepare to send a system message
    nodePtr->emit(Signal_system, new systemData("", "", SUMOID, DONE, std::string("system")));

    if(requestReroutes)
    {
        if (sendSystemMsgEvt->isScheduled())
            cancelAndDelete(sendSystemMsgEvt);
        else
            delete sendSystemMsgEvt;
    }

    simulation.getSystemModule()->unsubscribe("router",this);
    simulation.getSystemModule()->unsubscribe("system",this);
}

void ApplVSystem::handleSelfMsg(cMessage* msg)  //Internal messages to self
{
    ApplVBeacon::handleSelfMsg(msg);    //Pass it down

    if(!requestRoutes)
        return;

    //If it's a system message
    if (msg == sendSystemMsgEvt)
    {
        if(requestReroutes || (routingMode == DIJKSTRA and numReroutes == 0))
            reroute();
    }
}

void ApplVSystem::reroute()
{
    if(ev.isGUI() && debugLevel > 1)
    {
        std::cout << "Rerouting " << SUMOID << " at t=" << simTime().dbl() << endl;
        std::cout.flush();
    }

    ++numReroutes;

    if(routingMode == DIJKSTRA)
    {
        nodePtr->emit(Signal_system, new systemData(TraCI->vehicleGetEdgeID(SUMOID), targetNode, SUMOID, DIJKSTRA, std::string("system")));
        if(!router->UseHysteresis)
            scheduleAt(simTime() + requestInterval, sendSystemMsgEvt);// schedule for next beacon broadcast
    }
    else if(routingMode == HYPERTREE)
    {
        nodePtr->emit(Signal_system, new systemData(TraCI->vehicleGetEdgeID(SUMOID), targetNode, SUMOID, HYPERTREE, std::string("system")));
        scheduleAt(simTime() + hypertreeUpdateInterval, sendSystemMsgEvt);// schedule for next beacon broadcast
    }
}

void ApplVSystem::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj) //Ran upon receiving signals
{
    if(signalID == Signal_router)   //If the signal is of type router
    {
        systemData *s = static_cast<systemData *>(obj); //Cast to usable data
        if(std::string(s->getSender()) == "router" and std::string(s->getRecipient()) == SUMOID) //If sent from the router and to this vehicle
        {
            if((s->getRequestType() == DIJKSTRA || s->getRequestType() == HYPERTREE)) //If sent from the router and to this vehicle
            {
                std::list<std::string> sRoute = s->getInfo(); //Copy the info from the signal (breaks if we don't do this, for some reason)

                if(ev.isGUI() && debugLevel > 1)
                {
                    std::cout << SUMOID << " got route ";
                    for(std::string s : sRoute)
                        std::cout << s << " ";
                    std::cout << endl;
                    std::cout.flush();
                }

                if (*sRoute.begin() != "failed")
                    TraCI->vehicleSetRoute(s->getRecipient(), sRoute);  //Update this vehicle's path with the proper info
            }
            else if(s->getRequestType() == 2)//later use
            {

            }
        }
    }
}


// is called, every time the position of vehicle changes
void ApplVSystem::handlePositionUpdate(cObject* obj)
{
    ApplVBeacon::handlePositionUpdate(obj);
}

}
