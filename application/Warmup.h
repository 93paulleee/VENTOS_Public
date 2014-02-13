
#ifndef WARMPUP
#define WARMPUP

#include <omnetpp.h>
#include "mobility/traci/TraCIScenarioManagerLaunchd.h"
#include "mobility/traci/TraCIMobility.h"
#include "mobility/traci/TraCIConstants.h"
#include "TraCI_Extend.h"
#include "AddVehicle.h"
#include "SpeedProfile.h"


class Warmup : public cSimpleModule
{
	public:
		virtual ~Warmup();
		virtual void initialize(int stage);
        virtual void handleMessage(cMessage *msg);
		virtual void finish();

	public:
		bool DoWarmup();

	private:

        // NED variables
        cModule *nodePtr;       // pointer to the Node module
        TraCI_Extend *manager;  // pointer to the TraCI module
        AddVehicle *AddVehiclePtr;
        bool on;
        std::string leadingVehicle;
        double stopPosition;  // the position that first vehicle should stop waiting for others
        double waitingTime;
        int totalVehicles;

        // class variables
        double startTime;     // the time that Warmup starts
        bool IsWarmUpFinished;
        cMessage* warmupFinish;

        // methods
        bool warmUpFinished();
};


#endif
