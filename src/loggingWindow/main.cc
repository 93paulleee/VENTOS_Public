/****************************************************************************/
/// @file    main.cc
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

#include "mainWindow.h"
#include "iostream"
#include <glibmm/exception.h>

int main(int argc, char* argv[])
{
    try
    {
        auto app = Gtk::Application::create("VENTOS.vglog");

        VENTOS::mainWindow logWindow(argv[0] /*path to mainWindow file*/, argv[1] /*window title*/);

        // Shows the window and returns when it is closed
        return app->run(logWindow);
    }
    catch(const Glib::Exception& ex)
    {
        std::cout << ex.what() << std::endl;
        std::cout.flush();

        return 1;
    }
    catch(const std::exception& ex)
    {
        std::cout << ex.what() << std::endl;
        std::cout.flush();

        return 1;
    }
    catch(...)
    {
        std::cout << "Exception is thrown! \n";
        std::cout.flush();

        return 1;
    }
}
