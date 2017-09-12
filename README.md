# VENTOS #

VENTOS is an open-source VANET C++ simulator for studying vehicular traffic flows, collaborative driving, and interactions between vehicles and infrastructure through DSRC-enabled wireless communication capability. VENTOS is being developed in Rubinet Lab, UC Davis since 2013 and is the main tool in [C3PO](http://maniam.github.io/VENTOS/). See the official [website](http://maniam.github.io/VENTOS/) for documentation, installation instructions and tutorial.

## Changelog ##

### Git ###

**bug fixes**
+ VENTOS can now be installed from the downloaded zip file
+ vehicleGetColor is now working correctly
+ fix various memory leakes
+ measure sim start/end/duration more accuratly
+ fix simulation end time recording
+ fix getRSUposition when calling from initialize

**enhancements**
+ adding more TraCI commands
+ using shorter relative path for sumoConfig
+ gnuplot has its own class now
+ vehicle equilibrium is now working for emulated cars
+ enable parallel build
+ can now set the vglog window title with loggingWindowTitle
+ recording PHY frames that are sent but not received in statistics
+ adding vehicleExist TraCI command
+ adding vehicleSlowDown TraCI command
+ adding new commands to get XML elements of addNode.xml
+ can now run omnet++ in cmd mode without forc-running sumo in cmd mode
+ adding glogActive parameters to turn glog window off/on
+ runme script checks eigen installation more accuratly
+ adding vehicleGetWaitingTime TraCI command
+ updating the manual
+ various small improvements

### VENTOS 1.0 (July 4, 2017) ###

+ improving the user interface
+ adding traffic signal controls
+ introducing the VENTOS manual

### VENTOS 0.5 (Oct 21, 2015) ###

+ first public release
+ first implementation of our platoon management protocol
+ first implementation of our ACC/CACC car-following model
+ adversary module to simulate security attacks

## Contact Info ##

+ [Mani Amoozadeh](mailto:maniam@ucdavis.edu)
