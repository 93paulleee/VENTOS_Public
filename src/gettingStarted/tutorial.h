#ifndef TUTORIAL_H   // #include guard
#define TUTORIAL_H

#include "BaseApplLayer.h"  // base class for all programs
#include "TraCICommands.h"  // header that defines TraCI commands

// program is in the VENTOS namespace
namespace VENTOS {

// tutorial class is inherited from BaseApplLayer class
class tutorial : public BaseApplLayer
{
public:
    // class destructor
    virtual ~tutorial();
    // OMNET++ invokes this method when tutorial module is created
    virtual void initialize(int);
    // This method should return the number of initialization stages. By default, OMNET++ provides one-stage initialization and this method returns 1. In multi-stage initialization, this method should return the number of required stages (stages are numbers from 0)
    virtual int numInitStages() const { return 1; }
    // OMNET++ invokes this method when simulation has terminated successfully
    virtual void finish();
    // OMNET++ invokes this method with a message as parameter whenever the tutorial module receives a message
    virtual void handleMessage(omnetpp::cMessage *);
    // This method receives emitted signals that carry a long value. There are multiple overloaded receiveSignal methods for different value types
    virtual void receiveSignal(omnetpp::cComponent *, omnetpp::simsignal_t, long, cObject* details);

private:
    // This method is similar to the initialize method but you have access to the TraCI interface to communicate with SUMO. In the initialize method, TraCI interface is still closed
    void initialize_withTraCI();
    // This method is called in every simulation time step
    void executeEachTimestep();

private:
    // You can access the TraCI interface using this pointer
    TraCI_Commands *TraCI;
    // This module is subscribed to the following two signals. The first signal notifies the module that the TraCI interface has been established. The second signal notifies the module that one simulation time step is executed
    omnetpp::simsignal_t Signal_initialize_withTraCI;
    omnetpp::simsignal_t Signal_executeEachTS;
};

}

#endif   // end of #include guard
