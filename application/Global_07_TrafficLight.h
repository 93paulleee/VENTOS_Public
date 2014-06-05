
#ifndef TRAFFICLIGHT
#define TRAFFICLIGHT

#include <omnetpp.h>
#include "mobility/traci/TraCIScenarioManagerLaunchd.h"
#include "mobility/traci/TraCIMobility.h"
#include "mobility/traci/TraCIConstants.h"
#include "Global_01_TraCI_Extend.h"
#include "ApplV_05_Manager.h"

#include <sstream>
#include <iostream>
#include <fstream>


class TrafficLight : public BaseModule
{
	public:
		virtual ~TrafficLight();
		virtual void initialize(int stage);
        virtual void handleMessage(cMessage *msg);
		virtual void finish();

	public:
	    void Execute();

	private:
        // NED variables
        cModule *nodePtr;   // pointer to the Node
        TraCI_Extend *TraCI;  // pointer to the TraCI module
};


#endif
