VENTOS
======

VENTOS is an integrated C++ simulator for studying traffic flows in Vehicular Ad-hoc Networks (VANETs). You can find more information [here](http://rubinet.ece.ucdavis.edu/projects/ventos). Follow these instructions to install VENTOS (and its prerequisites) on your OS. Currently we support the following operating systems:

  Ubuntu    |     Mac OS X
:---------: | :------------------:
15          | El Capitan (10.11)
14          | Yosemite (10.10)
12          |


**Step 1:** If you do not have git installed on your machine, then install it. In Ubuntu, simply run the following command in terminal: 

    sudo apt-get install git

In Mac OS, type `git` in terminal and choose the Install button in the window that pops up. This will install command line tools that includes git.

**Step 2:** Clone the VENTOS repository on a folder that you have write permission such as Desktop or home folder (folder path should not have space). Note that you need to provide your github username and password.

    git clone https://github.com/ManiAm/VENTOS

**Step 3:** Go to the VENTOS folder and run the `runme` script. This bash script checks your system and installs the required packages and libraries. Script execution takes some time, so be patient. **Do not run the script using sudo.** Some of the commands need sudo access and you need to type your sudo password at least once. The script might show you many compile-time warning messages and you can safely ignore many of them.

    ./runme

**Step 4:** You can run the Eclipse IDE using the desktop shortcut, application launcher or typing `omnetpp` in terminal. The first time you run OMNET++, Eclipse IDE asks you to select a workspace. Select the folder that you will use to store all your project files. If you have intention to store all your projects on Desktop, then change the workspace to Desktop. Also check "Use this as the default and do not ask again".

"Introduction to OMNET++" page will appear. Click on "Workbench". Then it asks you to install INET framework or import some examples into your workspace. Uncheck them both since we do not need them for the time being.

**Step 5:** Now you can import VENTOS project into the OMNET++ IDE. Choose "File->Import" from the menu. Choose "General->Existing Projects into Workspace" from the upcoming dialog and proceed with "Next". Choose "Select root directory" and select the VENTOS folder. "VENTOS" should appear in the "Projects" section. Unselect "Copy project into workspace" if the VENTOS folder is already in your workspace. Click "Finish".

**Step 6:** Now you can build the VENTOS project. Use Ctrl+B or right-click on the project name and choose "Build Project". Wait for a while and then check the console windows at the bottom of the Eclipse IDE to make sure no errors occurred.


Running VENTOS
--------------

SUMO binaries (`sumo-guiD` and `sumoD`) are located in SUMO bin folder. sumo-guiD is the GUI version that provides a graphical interface to SUMO, and is visible while the OMNET++ simulation is running (and is very useful for debugging purposes). On the other hands, sumoD is the command-line version which is faster.Note that you can not use the official SUMO binaries since we have extended the TraCI commands, car-following models, etc. Using the official SUMO binaries will probably give you run-time error.

**Step 1:** To save you the trouble of manually running SUMO prior to every OMNeT++ simulation, the Veins module framework comes with a small python script (veins-sumo-launchd.py) to do that for you. This script will proxy TCP connections between OMNeT++ and SUMO, starting a new copy of the SUMO simulation for every OMNeT++ simulation connecting. From the VENTOS folder, run either of the following commands:

SUMO in graphical mode:

    python veins-sumo-launchd.py -vv -c sumo-guiD

SUMO in command-line mode:

    python veins-sumo-launchd.py -vv -c sumoD

The terminal should look like the following picture. The script will print `Listening on port 9999` and wait for the simulation to start. Note that after running either of these commands, DO NOT close the terminal window. Leave it open.

<img src="https://github.com/ManiAm/VENTOS/blob/master/launchd.png" />

**Step 2:** To make the life easier, you can create an alias command to make a shortcut to the above long commands. Open .bashrc and add these lines at the end (you should change the PATHs accordingly).

    export VENTOSPATH=/home/mani/Desktop/VENTOS
    alias sumo-cmd='python $VENTOSPATH/veins-sumo-launchd.py -vv -c sumoD'
    alias sumo-gui='python $VENTOSPATH/veins-sumo-launchd.py -vv -c sumo-guiD'

Now you can use sumo-cmd and sumo-gui commands instead!

**Step 3:** Run the VENTOS project by right clicking on the project name in the IDE and choose: 

    "Run as" -> "Run configurations..."

Choose 'OMNET++ Simulation' and click on 'New launch configuration' button at the top left. Give this configuration a name like myConfig. In Executable, choose opp_run and in 'Ini file(s)' choose one of the ini files in the project like params2_CACC.ini. From 'Config name' choose a configuration from the drop down list like CACCVehicleStream1. Leave the rest of the options to default. Click Apply and then click Run.


Running VENTOS from command-line
--------------------------------

The Eclipse IDE launcher helps you start the simulation easily by building the command line and the environment for your program automatically. Sometimes it would be great to start the simulation from outside the IDE. To get the command line used for running the simulation, run the simulation from the IDE. Go to 'debug view' and right click on your program and select 'properties'. The IDE shows the currently used command line such as:

    opp_run -u Cmdenv -c TrafficSignalControl -n . -l application/VENTOS params4_TL.ini -r 0

- `opp_run` allows starting simulation models that are linked as shared libraries
- `-u Cmdenv` tells omnetpp to run under Cmdenv (command-line environment)
- `-c <configname>` option is used to select a configuration
- `-n` option is used to specify the NED path
- `-l` option is used to load additional shared libraries
- `-r <runnumber>` allows you to select runs

OMNET has a utility program called opp_runall which allows you to execute a simulation batch in command line mode. You must specify the whole command line you would use to run your batch in Cmdenv. For example:

    opp_runall -j24 opp_run -u Cmdenv -c TrafficSignalControl -n . -l application/VENTOS params4_TL.ini -r 0..30

The -j option is used to specify the maximum number of parallel runs allowed. opp_runall basically expands the -r option; for example, a simulation command line containing -r 0..2,8,10 will generate 5 commands with the -r 0, -r 1, -r 2, -r 8 and -r 10 options, respectively.


Running VENTOS on a Server
--------------------------

You can run the project on a remote server (Ubuntu Server preferably). 

**Step 1:** You have to make sure that a ssh server is running on the remote server. To check if a ssh server is running on the server, type the following command in the server:

    ssh localhost

If you receive the following message then the ssh server is not installed.

    ssh: connect to host localhost port 22: Connection refused

Install openssh-server using the following command:

    apt-get install openssh-server

Now if you type ssh localhost, you should get the following message (type no):

    The authenticity of host 'localhost (127.0.0.1)' can't be established.
    ECDSA key fingerprint is fc:7e:9a:c7:33:86:47:4b:35:00:22:b7:be:5c:d1:c7.
    Are you sure you want to continue connecting (yes/no)? 

**Step 2:** SSH to the remote server as following. `mars.ece.ucdavis.edu` is the name of the server (you can use the server's IP address as well) and rubinet is the username.

    ssh -X rubinet@mars.ece.ucdavis.edu

Note 1: The -X flag enables X11 forwarding and forwards the program that uses a GUI through your SSH connection. Whenever you launch a program that uses a GUI, it will pop up as if you were physically sitting at that computer (this method is preferrable to using VNC which forwards the entire desktop). 

Note 2: The `X server` should be running on **your** machine. Use the following command to check if X server is running on your machine:

    xset q

If X server is not installed on your machine, then install it:

    sudo apt-get install xorg
    sudo apt-get install openbox

**Step 3:** Follow the same instructions in previous sections to install VENTOS on the remote server.

**Step 4:** While using OMNET++, the SSH session should be open. Closing the terminal windows or restarting your machine terminates the SSH session and the running simulation will be closed. To keep the SSH session open, you need to use `screen` or `tmux`. We will use `tmux` since it is more powerfull. 

Start tmux by typing `tmux` into the shell. When you do that, it just looks like your screen clears and you’re back at the same terminal prompt. You will notice that the terminal now has a green bar along the bottom. Type `omnetpp` to start the IDE and then type `sumo-cmd &` to run the python script in the background.

**Step 5:** Detach the tmux session by pressing `Ctrl+B` and then typing `D`. You can now safely logoff from the remote machine, your process will keep running inside tmux. When you come back again and want to check the status of your process you can use `tmux attach` to attach to your tmux session.

In order to exchange files between your computer and remote server, you can use filezilla.
