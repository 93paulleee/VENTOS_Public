
#ifndef STATISTICS_H
#define STATISTICS_H

#include <Appl.h>
#include "Global_01_TraCI_Extend.h"
#include <ApplRSU_03_Manager.h>

namespace VENTOS {

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

	  void executeOneTimestep(bool); // should be public

  private:
      void vehiclesData();
      void saveVehicleData(string);
      void vehiclesDataToFile();

      void inductionLoops();
      void inductionLoopToFile();

      void incidentDetectionToFile();

      void MAClayerToFile();

      void plnManageToFile();



	  void postProcess();
      void printToFile();
      vector<NodeEntry *> SortByID(vector<NodeEntry *>);
      int getNodeIndex(const char*);

      int findInVector(vector<LoopDetector *> , const char *, const char *);
      int findInVector(vector<NodeEntry *>, const char *);
      int findInVector(vector<MacStatEntry *>, const char *);



      // NED variables
      TraCI_Extend *TraCI;
      double updateInterval;
      double terminate;

      // NED variables
      bool collectMAClayerData;
      bool collectVehiclesData;
      bool collectInductionLoopData;
      bool collectPlnManagerData;
      bool printBeaconsStatistics;
      bool printIncidentDetection;

      // class variables
      int index;

      // class variables (signals)
	  simsignal_t Signal_beaconP;
	  simsignal_t Signal_beaconO;
	  simsignal_t Signal_beaconD;

	  simsignal_t Signal_MacStats;
	  simsignal_t Signal_SentPlatoonMsg;
	  simsignal_t Signal_VehicleState;

	  // class variables (vectors)
      vector<VehicleData *> Vec_vehiclesData;
      vector<LoopDetector *> Vec_loopDetectors;
      vector<MacStatEntry *> Vec_MacStat;
      vector<plnManagement*> Vec_plnManagement;

      vector<NodeEntry *> Vec_BeaconsP;    // beacons from proceeding
      vector<NodeEntry *> Vec_BeaconsO;    // beacons from other vehicles
      vector<NodeEntry *> Vec_BeaconsDP;   // Doped beacons from preceding vehicle
      vector<NodeEntry *> Vec_BeaconsDO;   // Doped beacons from other vehicles

      vector<NodeEntry *> totalBeaconsP;
      vector<NodeEntry *> totalBeaconsO;
      vector<NodeEntry *> totalBeaconsDP;
      vector<NodeEntry *> totalBeaconsDO;

      vector<NodeEntry *> beaconsDO_interval;
      vector<NodeEntry *> beaconsDP_interval;
};
}

#endif
