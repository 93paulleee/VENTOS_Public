
#ifndef STATISTICS_H
#define STATISTICS_H

#include <omnetpp.h>
#include "FindModule.h"
#include "NetwControlInfo.h"
#include <BaseModule.h>
#include <Appl.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include "Global_01_TraCI_Extend.h"
#include <ApplRSU.h>
#include <stdlib.h>

#include <Eigen/Dense>
using namespace Eigen;

using namespace std;

class Statistics : public BaseModule
{
  public:
      Statistics();
      virtual ~Statistics();
      virtual void finish();
      virtual void initialize(int);
      virtual void handleMessage(cMessage *);
	  virtual void receiveSignal(cComponent *, simsignal_t, long);
	  virtual void receiveSignal(cComponent *, simsignal_t, cObject *);

  private:
	  void postProcess();
	  void printStatistics();
      void printToFile();
      std::vector<NodeEntry *> SortByID(std::vector<NodeEntry *>);
      int getNodeIndex(const char*);
      int findInVector(std::vector<NodeEntry *>, const char *);
      int findInVector(std::vector<MacStatEntry *>, const char *);
      void printAID();

      // NED variables
      TraCI_Extend *TraCI;
      double updateInterval;
      double terminate;
      bool printBeaconsStatistics;

      simsignal_t Signal_terminate;

	  simsignal_t Signal_beaconP;
	  simsignal_t Signal_beaconO;
	  simsignal_t Signal_beaconD;

	  simsignal_t Signal_MacStats;

      std::vector<NodeEntry *> Vec_BeaconsP;    // beacons from proceeding
      std::vector<NodeEntry *> Vec_BeaconsO;    // beacons from other vehicles
      std::vector<NodeEntry *> Vec_BeaconsDP;   // Doped beacons from preceding vehicle
      std::vector<NodeEntry *> Vec_BeaconsDO;   // Doped beacons from other vehicles

      std::vector<NodeEntry *> totalBeaconsP;
      std::vector<NodeEntry *> totalBeaconsO;
      std::vector<NodeEntry *> totalBeaconsDP;
      std::vector<NodeEntry *> totalBeaconsDO;

      std::vector<NodeEntry *> beaconsDO_interval;
      std::vector<NodeEntry *> beaconsDP_interval;

      std::vector<MacStatEntry *> Vec_MacStat;
};

#endif
