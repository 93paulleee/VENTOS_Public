/****************************************************************************/
/// @file    vLog.cc
/// @author  Mani Amoozadeh <maniam@ucdavis.edu>
/// @author  second author name
/// @date    May 2016
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

#include <vLog.h>
#include <algorithm>
#include <thread>

#include <gtkmm/application.h>
#include "mainWindow.h"

namespace VENTOS {

Define_Module(VENTOS::vLog);

vLog::~vLog()
{
    // making sure to flush the remaining data in buffer
    flush();
}


void vLog::initialize(int stage)
{
    super::initialize(stage);

    if(stage ==0)
    {
        systemLogLevel = par("systemLogLevel").longValue();
        logRecordCMD = par("logRecordCMD").boolValue();

        // default output stream
        vLogStreams["std::cout"] =  &std::cout;
    }
}


void vLog::finish()
{

}


void vLog::handleMessage(omnetpp::cMessage *msg)
{

}


vLog& vLog::setLog(uint8_t logLevel, std::string category, std::string subcategory)
{
    std::lock_guard<std::mutex> lock(lock_log);

    if(category == "")
        throw omnetpp::cRuntimeError("category name can't be empty!");

    auto it = vLogStreams.find(category);
    // this is a new category
    if(it == vLogStreams.end())
    {
        static mainWindow *logWindow = NULL;

        // brings up the Qt window
        if(vLogStreams.size() == 1)
        {
            // run the QT application event loop in a child thread
            std::thread thd = std::thread([=]() {

                auto app = Gtk::Application::create("org.gtkmm.example");

                char *argv[] = {"arg1", "arg2", nullptr};
                int argc = 1;

                logWindow = new mainWindow();

                //Shows the window and returns when it is closed.
                return app->run(*logWindow, argc, argv);
            });

            thd.detach();

            // wait for the thread to set the logWindow pointer
            while(!logWindow);

            // wait for the window to become visible
            bool visible = false;
            while(!visible)
                logWindow->get_property("visible", visible);
        }

        // adding a new tab with name 'category' and
        // redirect oss to the associated text view
        std::ostream *oss = logWindow->addTab(category);

        // finally, adding oss to vLogStreams
        vLogStreams[category] = oss;
    }

    lastLogLevel = logLevel;
    lastCategory = category;

    return *this;
}


void vLog::flush()
{
    std::lock_guard<std::mutex> lock(lock_log);

    for(auto &i : vLogStreams)
        i.second->flush();
}

}
