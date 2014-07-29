
#ifndef PLOTTER_H
#define PLOTTER_H

#include <BaseModule.h>
#include <Appl.h>
#include "Global_01_TraCI_Extend.h"


class Plotter : public BaseModule
{
  public:
      Plotter();
      virtual ~Plotter();
      virtual void finish();
      virtual void initialize(int);
      virtual void handleMessage(cMessage *);

      void speedProfile();

  private:
      TraCI_Extend *TraCI;
      bool on;
      FILE *pipe;
};

#endif
