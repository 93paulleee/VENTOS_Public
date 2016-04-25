/****************************************************************************/
/// @file    codeLoader.cc
/// @author  Mani Amoozadeh <maniam@ucdavis.edu>
/// @author  second author name
/// @date    Apr 2016
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

#include "codeLoader.h"
#include <thread>
#include <fstream>
#include "dev.h"

namespace VENTOS {

Define_Module(VENTOS::codeLoader);


codeLoader::~codeLoader()
{

}


void codeLoader::initialize(int stage)
{
    super::initialize(stage);

    if(stage ==0)
    {
        on = par("on").boolValue();
        if(!on)
            return;

        // get a pointer to the TraCI module
        cModule *module = simulation.getSystemModule()->getSubmodule("TraCI");
        ASSERT(module);
        TraCI = static_cast<TraCI_Commands *>(module);
        ASSERT(TraCI);

        Signal_initialize_withTraCI = registerSignal("initialize_withTraCI");
        simulation.getSystemModule()->subscribe("initialize_withTraCI", this);

        Signal_executeEachTS = registerSignal("executeEachTS");
        simulation.getSystemModule()->subscribe("executeEachTS", this);

        // construct the path to redpineAppl folder
        boost::filesystem::path VENTOS_FullPath = cSimulation::getActiveSimulation()->getEnvir()->getConfig()->getConfigEntry("network").getBaseDirectory();
        redpineAppl_FullPath = VENTOS_FullPath / "src" / "redpineAppl";

        initScriptName = par("initScriptName").stringValue();
        if(initScriptName == "")
            error("initScriptName is empty!");

        remoteDir_Driver = par("remoteDir_Driver").stringValue();
        if(remoteDir_Driver == "")
            error("remoteDir_Driver is empty!");

        remoteDir_SourceCode = par("remoteDir_SourceCode").stringValue();
        if(remoteDir_SourceCode == "")
            error("remoteDir_SourceCode is empty!");
    }
}


void codeLoader::finish()
{

}


void codeLoader::handleMessage(cMessage *msg)
{

}


void codeLoader::receiveSignal(cComponent *source, simsignal_t signalID, long i)
{
    if(!on)
        return;

    Enter_Method_Silent();

    if(signalID == Signal_initialize_withTraCI)
    {
        initialize_withTraCI();
    }
    else if(signalID == Signal_executeEachTS)
    {
        executeEachTimestep();
    }
}


void codeLoader::initialize_withTraCI()
{

}


void codeLoader::executeEachTimestep()
{
    // run this code only once
    static bool wasExecuted = false;
    if (on && !wasExecuted)
    {
        start();
        wasExecuted = true;
    }
}


void codeLoader::start()
{
    int numDev = par("numDev").longValue();
    if(numDev < 0)
        error("numDev should be >= 0");
    else if(numDev == 0)
        return;

    cModule* parentMod = getParentModule();
    if (!parentMod)
        error("Parent Module not found");

    cModuleType* nodeType = cModuleType::get("VENTOS.src.redpineAppl.dev");

    for(int i = 0; i < numDev; i++)
    {
        cModule* mod = nodeType->create("dev", parentMod, numDev, i);
        mod->finalizeParameters();
        mod->getDisplayString().updateWith("i=old/x_yellow");
        mod->getDisplayString().updateWith("p=0,0");
        mod->buildInside();
        mod->scheduleStart(simTime());
        mod->callInitialize();
    }

    // call init_board for each board in a separate child thread
    std::vector<std::thread> workers;
    for(int ii = 0; ii < numDev; ii++)
    {
        cModule *module = simulation.getSystemModule()->getSubmodule("dev", ii);
        ASSERT(module);
        workers.push_back( std::thread (&codeLoader::init_board, this, module) );
    }

    // wait for all threads to finish
    std::for_each(workers.begin(), workers.end(), [](std::thread &t) {
        t.join();
    });

    std::cout << "All done! \n" << std::flush;
}


void codeLoader::init_board(cModule *mod)
{
    ASSERT(mod);

    printf(">>> Connecting to %s ... \n\n", mod->par("host").stringValue());
    std::cout.flush();

    // create SSH connection to the dev
    SSH *board = new SSH(mod->par("host").stringValue(),
            mod->par("port").longValue(),
            mod->par("username").stringValue(),
            mod->par("password").stringValue(), true);

    ASSERT(board);

    // should we reboot this dev before proceeding?
    if(mod->par("rebootAtStart").boolValue())
    {
        printf(">>> Re-booting device @%s ... Please wait \n\n", board->getHostName().c_str());
        std::cout.flush();

        ssh_channel rebootShell = board->openShell();  // open a shell
        double duration_ms = board->rebootDev(rebootShell, 40000 /*timeout in ms*/);
        board->closeShell(rebootShell);  // close the shell

        printf(">>> Device @%s is up and running! Boot time ~ %.2f seconds. Reconnecting ... \n\n", board->getHostName().c_str(), duration_ms / 1000.);
        std::cout.flush();

        // previous SSH connection is lost. Re-connect to the dev
        board = new SSH(board->getHostName(), board->getPort(), board->getUsername(), board->getPassword());
        ASSERT(board);
    }

    //#################################################
    // Step 1: copy the init script to remoteDir_Driver
    //#################################################
    boost::filesystem::path script_FullPath = redpineAppl_FullPath / initScriptName;

    printf(">>> Copying the init script to %s:%s ... \n\n", board->getHostName().c_str(), remoteDir_Driver.c_str());
    std::cout.flush();

    // read file contents into a string
    std::ifstream ifs(script_FullPath.c_str());
    std::string content( (std::istreambuf_iterator<char>(ifs) ),
            (std::istreambuf_iterator<char>()    ) );

    // get a pointer to the dev class
    dev *devPtr = static_cast<dev *>(mod);
    ASSERT(devPtr);

    // ask dev to substitute its parameters in the init script
    devPtr->substituteParams(content);

    // do the copying
    board->copyFileStr_SFTP(initScriptName, content, remoteDir_Driver);

    //##################################################################
    // Step 2: copying new/modified source codes to remoteDir_SourceCode
    //##################################################################
    printf(">>> Syncing source codes with %s ... \n\n", board->getHostName().c_str());
    std::cout.flush();

    boost::filesystem::path sampleAppl_FullPath = redpineAppl_FullPath / "sampleAppl";
    board->syncDir(sampleAppl_FullPath, remoteDir_SourceCode);

    //##################################
    // Step 3: remotely compile the code
    //##################################
    std::string applName = mod->par("applName").stringValue();
    if(applName == "")
        throw cRuntimeError("applName is empty!");

    printf(">>> Compiling %s @%s ... \n\n", applName.c_str(), board->getHostName().c_str());
    std::cout.flush();

    char command[500];
    sprintf(command, "gcc %s.c -o %s ./rsi_wave_api/lib_rsi_wave_api.a ./dsrc/libdsrc.a -I ./dsrc/includes/ -lpthread", applName.c_str(), applName.c_str());

    ssh_channel compileShell = board->openShell();  // open a shell
    board->run_command(compileShell, "cd " + remoteDir_SourceCode.string());
    board->run_command(compileShell, command, 5, false);
    board->closeShell(compileShell);  // close the shell

    //########################################################
    // Step 4: remotely run the script in the remoteDir_Driver
    //########################################################
    printf(">>> Running the init script @%s ... \n\n", board->getHostName().c_str());
    std::cout.flush();

    ssh_channel driverShell = board->openShell();  // open a shell
    board->run_command(driverShell, "sudo su");
    board->run_command(driverShell, "cd " + remoteDir_Driver.string());
    board->run_command(driverShell, "./" + initScriptName, 10, false);
    board->closeShell(driverShell);  // close the shell

    //###############################################
    // Step 5: remotely start 1609 stack in WAVE mode
    //###############################################
    printf(">>> Start 1609 stack in WAVE mode @%s ... \n\n", board->getHostName().c_str());
    std::cout.flush();

    ssh_channel rsi1609Shell = board->openShell();  // open a shell
    board->run_command(rsi1609Shell, "sudo su");
    board->run_command(rsi1609Shell, "cd " + remoteDir_Driver.string());
    board->run_command(rsi1609Shell, "if ! pgrep rsi_1609 > /dev/null; then ./rsi_1609; fi", 10, false);  // should not close this shell

    //##############################
    // Step 6: remotely run the code
    //##############################
    printf(">>> Running %s @%s ... \n\n", applName.c_str(), board->getHostName().c_str());
    std::cout.flush();

    ssh_channel applShell = board->openShell();  // open a shell
    board->run_command(applShell, "sudo su");
    board->run_command(applShell, "cd " + remoteDir_SourceCode.string());
    board->run_command(applShell, "./" + applName, 3, true);  // should not close this shell

    // we are done with the SSH session
    delete board;
}

}
