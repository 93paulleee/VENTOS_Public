
#ifndef ___MAC1609_4_MOD_H_
#define ___MAC1609_4_MOD_H_

#include <Mac1609_4.h>
#include <ExtraClasses.h>

class Mac1609_4_Mod : public Mac1609_4
{
	public:
		~Mac1609_4_Mod() { };

	protected:
		virtual void initialize(int);

	    virtual void handleSelfMsg(cMessage*);

		virtual void handleLowerMsg(cMessage*);
	    virtual void handleLowerControl(cMessage* msg);

		virtual void handleUpperMsg(cMessage*);
		virtual void handleUpperControl(cMessage* msg);

        virtual void finish();

	private:
        void print_MacStatistics();

        cModule *nodePtr;   // pointer to the Node
};

#endif
