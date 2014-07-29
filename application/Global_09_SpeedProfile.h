
#ifndef SPEEDPROFILE
#define SPEEDPROFILE

#include "BaseModule.h"
#include "Global_01_TraCI_Extend.h"

namespace VENTOS {

class SpeedProfile : public BaseModule
{
	public:
		virtual ~SpeedProfile();
		virtual void initialize(int stage);
        virtual void handleMessage(cMessage *msg);
		virtual void finish();

	public:
        void Change();

	private:

        // NED variables
        cModule *nodePtr;   // pointer to the Node
        TraCI_Extend *TraCI;
        bool on;
	    int mode;
	    string laneId;
	    double minSpeed;
        double normalSpeed;
        double maxSpeed;
        double switchTime;

	    // class variables
        string profileVehicle;
        string lastProfileVehicle;
	    double old_speed;
	    double old_time;
	    double startTime;  // the time that speed profiling starts
        FILE *f2;
        bool endOfFile;

	    // method
	    void AccelDecel(double, double, double);
	    void AccelDecelZikZak(double, double, double);
        void AccelDecelPeriodic(double, double, double, double);
	    void ExTrajectory(double);
};
}

#endif
