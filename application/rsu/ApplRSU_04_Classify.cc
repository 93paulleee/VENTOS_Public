/****************************************************************************/
/// @file    ApplRSU_04_Classify.cc
/// @author  Mani Amoozadeh <maniam@ucdavis.edu>
/// @author  second author name
/// @date    Nov 2015
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

#include "ApplRSU_04_Classify.h"
#include <Plotter.h>

#undef ev
//###begin<skeleton>
#include <shark/Data/Dataset.h>
#include <shark/Data/Csv.h>
#include <shark/ObjectiveFunctions/Loss/ZeroOneLoss.h>
//###end<skeleton>

//###begin<SVM-includes>
#include <shark/Models/Kernels/GaussianRbfKernel.h>
#include <shark/Models/Kernels/KernelExpansion.h>
#include <shark/Algorithms/Trainers/CSvmTrainer.h>
//###end<SVM-includes>
#define ev  (*cSimulation::getActiveEnvir())

namespace VENTOS {

Define_Module(VENTOS::ApplRSUCLASSIFY);

ApplRSUCLASSIFY::~ApplRSUCLASSIFY()
{

}


void ApplRSUCLASSIFY::initialize(int stage)
{
    ApplRSUTLVANET::initialize(stage);

    if (stage==0)
    {
        classifier = par("classifier").boolValue();
        if(!classifier)
            return;

        // we need this RSU to be associated with a TL
        if(myTLid == "")
            error("The id of %s does not match with any TL. Check RSUsLocation.xml file!", myFullId);

        // for each incoming lane in this TL
        std::list<std::string> lan = TraCI->TLGetControlledLanes(myTLid);

        // remove duplicate entries
        lan.unique();

        // for each incoming lane
        for(std::list<std::string>::iterator it2 = lan.begin(); it2 != lan.end(); ++it2)
            lanesTL[*it2] = myTLid;

        if(ev.isGUI())
            initializeGnuPlot();

        classifierF();
    }
}


void ApplRSUCLASSIFY::finish()
{
    ApplRSUTLVANET::finish();
}


void ApplRSUCLASSIFY::handleSelfMsg(cMessage* msg)
{
    ApplRSUTLVANET::handleSelfMsg(msg);
}


void ApplRSUCLASSIFY::executeEachTimeStep()
{
    ApplRSUTLVANET::executeEachTimeStep();
}


void ApplRSUCLASSIFY::onBeaconVehicle(BeaconVehicle* wsm)
{
    ApplRSUTLVANET::onBeaconVehicle(wsm);

    if (classifier)
        onBeaconAny(wsm);
}


void ApplRSUCLASSIFY::onBeaconBicycle(BeaconBicycle* wsm)
{
    ApplRSUTLVANET::onBeaconBicycle(wsm);

    if (classifier)
        onBeaconAny(wsm);
}


void ApplRSUCLASSIFY::onBeaconPedestrian(BeaconPedestrian* wsm)
{
    ApplRSUTLVANET::onBeaconPedestrian(wsm);

    if (classifier)
        onBeaconAny(wsm);
}


void ApplRSUCLASSIFY::onBeaconRSU(BeaconRSU* wsm)
{
    ApplRSUTLVANET::onBeaconRSU(wsm);
}


void ApplRSUCLASSIFY::onData(LaneChangeMsg* wsm)
{
    ApplRSUTLVANET::onData(wsm);
}


void ApplRSUCLASSIFY::initializeGnuPlot()
{
    // get a pointer to the plotter module
    cModule *pmodule = simulation.getSystemModule()->getSubmodule("plotter");

    if(pmodule == NULL)
        error("plotter module is not found!");

    // return if plotter is off
    if(!pmodule->par("on").boolValue())
        return;

    // get a pointer to the class
    Plotter *pltPtr = static_cast<Plotter *>(pmodule);
    ASSERT(pltPtr);

    plotterPtr = pltPtr->pipeGnuPlot;
    ASSERT(plotterPtr);

    // We need feature in GNUPLOT 5.0 and above
    if(pltPtr->vers < 5)
        error("GNUPLOT version should be >= 5");

    // set title name
    fprintf(plotterPtr, "set title 'Sample Points' \n");

    // set axis labels
    fprintf(plotterPtr, "set xlabel 'X pos' offset -5 \n");
    fprintf(plotterPtr, "set ylabel 'Y pos' offset 3 \n");
    fprintf(plotterPtr, "set zlabel 'Speed' offset -2 rotate left \n");

    // change ticks
    //   fprintf(pipe, "set xtics 20 \n");
    //   fprintf(pipe, "set ytics 20 \n");

    // set range
    //   fprintf(pipe, "set yrange [885:902] \n");

    // set grid and border
    fprintf(plotterPtr, "set grid \n");
    fprintf(plotterPtr, "set border 4095 \n");

    // set agenda location
    fprintf(plotterPtr, "set key outside right top box \n");

    // define line style
    fprintf(plotterPtr, "set style line 1 pointtype 7 pointsize 1 lc rgb 'red'  \n");
    fprintf(plotterPtr, "set style line 2 pointtype 7 pointsize 1 lc rgb 'green' \n");
    fprintf(plotterPtr, "set style line 3 pointtype 7 pointsize 1 lc rgb 'blue' \n");

    fflush(plotterPtr);
}


// update variables upon reception of any beacon (vehicle, bike, pedestrian)
template <typename T> void ApplRSUCLASSIFY::onBeaconAny(T wsm)
{
    std::string sender = wsm->getSender();
    Coord pos = wsm->getPos();

    // If in the detection region:
    // todo: change from fix values
    // coordinates are the exact location of the middle of the LD
    if ( (pos.x >= 851.1 /*x-pos of west LD*/) && (pos.x <= 948.8 /*x-pos of east LD*/) && (pos.y >= 851.1 /*y-pos of north LD*/) && (pos.y <= 948.8 /*y-pos of north LD*/) )
    {
        std::string lane = wsm->getLane();

        // If on one of the incoming lanes
        if(lanesTL.find(lane) != lanesTL.end() && lanesTL[lane] == myTLid && lane.find("WC") != std::string::npos )
        {
            feature *tmp = new feature(pos.x, pos.y, wsm->getSpeed());

            // add each instance of input vector (posX, posY, speed) to the dataset
            if(std::string(wsm->getSenderType()) == "passenger")
                dataSet[0].push_back(*tmp);  // class 1
            else if(std::string(wsm->getSenderType()) == "bicycle")
                dataSet[1].push_back(*tmp);  // class 2
            else if(std::string(wsm->getSenderType()) == "pedestrian")
                dataSet[2].push_back(*tmp);  // class 3

            // draw the dataset on gnuplot
            if(plotterPtr != NULL)
            {
                // create a data blocks out of dataset (only gnuplot > 5.0 supports this)
                fprintf(plotterPtr, "$data << EOD \n");

                // data block 1
                for(auto &i : dataSet[0])
                    fprintf(plotterPtr, "%f  %f  %f \n", i.xPos, i.yPos, i.speed);

                // two blank lines as data block separator
                fprintf(plotterPtr, "\n\n \n");

                // data block 2
                for(auto &i : dataSet[1])
                    fprintf(plotterPtr, "%f  %f  %f \n", i.xPos, i.yPos, i.speed);

                // two blank lines as data block separator
                fprintf(plotterPtr, "\n\n \n");

                // data block 3
                for(auto &i : dataSet[2])
                    fprintf(plotterPtr, "%f  %f  %f \n", i.xPos, i.yPos, i.speed);

                // data block terminator
                fprintf(plotterPtr, "EOD \n");

                // make plot
                fprintf(plotterPtr, "splot '$data' index 0 using 1:2:3 with points ls 1 title 'Class 1',");
                fprintf(plotterPtr, "      ''      index 1 using 1:2:3 with points ls 2 title 'Class 2',");
                fprintf(plotterPtr, "      ''      index 2 using 1:2:3 with points ls 3 title 'Class 3' \n");

                fflush(plotterPtr);
            }
        }
    }
}


/*
The classes are as follows:
    - class 1: points very close to the origin
    - class 2: points on the circle of radius 10 around the origin
    - class 3: points that are on a circle of radius 4 but not around the origin at all */
//void ApplRSUCLASSIFY::generate_data (std::vector<sample_type_2D>& samples, std::vector<double>& labels)
//{
//    if(pipe != NULL)
//    {
//        fprintf(pipe, "set title 'Sample Points' \n");
//        fprintf(pipe, "set xlabel 'X' \n");
//        fprintf(pipe, "set ylabel 'Y' \n");
//        fprintf(pipe, "set key outside right top box \n");
//
//        // The "-" is used to specify that the data follows the plot command
//        fprintf(pipe, "plot '-' using 1:2 with points pointtype 7 pointsize 0.5 title 'type 1' ,");
//        fprintf(pipe, "'-' using 1:2 with points pointtype 6 pointsize 0.5 title 'type 2' ,");
//        fprintf(pipe, "'-' using 1:2 with points pointtype 6 pointsize 0.5 title 'type 3' \n");
//    }
//
//    const long num = 50;
//
//    sample_type_2D m;
//
//    dlib::rand rnd;
//
//    // make some samples near the origin
//    double radius = 4;
//    for (long i = 0; i < num+10; ++i)
//    {
//        double sign = 1;
//        if (rnd.get_random_double() < 0.5)
//            sign = -1;
//
//        m(0) = 2*radius*rnd.get_random_double()-radius;
//        m(1) = sign*sqrt(radius*radius - m(0)*m(0));
//
//        // add this sample to our set of training samples
//        samples.push_back(m);
//        labels.push_back(1);
//
//        if(pipe != NULL)
//            fprintf(pipe, "%f %f\n", m(0), m(1));
//    }
//
//    fflush(pipe);
//    fprintf(pipe, "e\n"); // termination character
//
//    // make some samples in a circle around the origin but far away
//    radius = 10.0;
//    for (long i = 0; i < num+20; ++i)
//    {
//        double sign = 1;
//        if (rnd.get_random_double() < 0.5)
//            sign = -1;
//
//        m(0) = 2*radius*rnd.get_random_double()-radius;
//        m(1) = sign*sqrt(radius*radius - m(0)*m(0));
//
//        // move points to the right
//        m(0) += 25;
//
//        // add this sample to our set of training samples
//        samples.push_back(m);
//        labels.push_back(2);
//
//        if(pipe != NULL)
//            fprintf(pipe, "%f %f\n", m(0), m(1));
//    }
//
//    fflush(pipe);
//    fprintf(pipe, "e\n"); // termination character
//
//    // make some samples in a circle around the point (25,25)
//    radius = 4.0;
//    for (long i = 0; i < num+30; ++i)
//    {
//        double sign = 1;
//        if (rnd.get_random_double() < 0.5)
//            sign = -1;
//        m(0) = 2*radius*rnd.get_random_double()-radius;
//        m(1) = sign*sqrt(radius*radius - m(0)*m(0));
//
//        // translate this point away from the origin
//        m(0) += 25;
//        m(1) += 25;
//
//        // add this sample to our set of training samples
//        samples.push_back(m);
//        labels.push_back(3);
//
//        if(pipe != NULL)
//            fprintf(pipe, "%f %f\n", m(0), m(1));
//    }
//
//    std::cout << "samples.size(): " << samples.size() << std::endl << endl;
//
//    fflush(pipe);
//    fprintf(pipe, "e\n"); // termination character
//}


void ApplRSUCLASSIFY::classifierF()
{

    // Load data, use 70% for training and 30% for testing.
    // The path is hard coded; make sure to invoke the executable
    // from a place where the data file can be found. It is located
    // under [shark]/examples/Supervised/data.
    shark::ClassificationDataset traindata, testdata;
    shark::importCSV(traindata, "/home/mani/Desktop/dataset/quickstartData.csv", shark::LAST_COLUMN, ' ');
    testdata = shark::splitAtElement(traindata, 70 * traindata.numberOfElements() / 100);
    //###end<skeleton>

    //###begin<SVM>
    double gamma = 1.0;         // kernel bandwidth parameter
    double C = 10.0;            // regularization parameter
    shark::GaussianRbfKernel<shark::RealVector> kernel(gamma);
    shark::KernelClassifier<shark::RealVector> model(&kernel);
    shark::CSvmTrainer<shark::RealVector> trainer(
            &kernel,
            C,
            true); /* true: train model with offset */
    //###end<SVM>

    //###begin<skeleton>
    trainer.train(model, traindata);

    shark::Data<unsigned int> prediction = model(testdata.inputs());

    shark::ZeroOneLoss<unsigned int> loss;
    double error_rate = loss(testdata.labels(), prediction);

    std::cout << "model: " << model.name() << std::endl
            << "trainer: " << trainer.name() << std::endl
            << "test error rate: " << error_rate << std::endl;



    //    std::vector<sample_type_2D> samples;
    //    std::vector<double> labels;
    //
    //    // First, get our labeled set of training data
    //    generate_data(samples, labels);
    //
    //    try
    //    {
    //        // Program we will work with a one_vs_one_trainer object which stores any
    //        // kind of trainer that uses our sample_type samples.
    //        typedef dlib::one_vs_one_trainer<dlib::any_trainer<sample_type_2D> > ovo_trainer;
    //
    //        // Finally, make the one_vs_one_trainer.
    //        ovo_trainer trainer;
    //
    //        // making the second binary classification trainer object.
    //        // this uses 'kernel ridge regression' and 'RBF kernels'
    //        typedef dlib::radial_basis_kernel<sample_type_2D> rbf_kernel;
    //        dlib::krr_trainer<rbf_kernel> rbf_trainer;  // make the binary trainer
    //        rbf_trainer.set_kernel(rbf_kernel(0.1));    // set some parameters
    //
    //        // making the first binary classification trainer object.
    //        // this uses 'support vector machine' and 'polynomial kernels'
    //        typedef dlib::polynomial_kernel<sample_type_2D> poly_kernel;
    //        dlib::svm_nu_trainer<poly_kernel> poly_trainer;   // make the binary trainer
    //        poly_trainer.set_kernel(poly_kernel(0.1, 1, 2));  // set some parameters
    //
    //        // Now tell the one_vs_one_trainer that, by default, it should use the rbf_trainer
    //        // to solve the individual binary classification subproblems.
    //        trainer.set_trainer(rbf_trainer);
    //
    //        // We can also get more specific. Here we tell the one_vs_one_trainer to use the
    //        // poly_trainer to solve the class 1 vs class 2 subproblem. All the others will
    //        // still be solved with the rbf_trainer.
    //        trainer.set_trainer(poly_trainer, 1, 2);
    //
    //        // Now let's do 5-fold cross-validation using the one_vs_one_trainer we just setup.
    //        // As an aside, always shuffle the order of the samples before doing cross validation.
    //        // For a discussion of why this is a good idea see the svm_ex.cpp example.
    //        //randomize_samples(samples, labels);
    //        //std::cout << "cross validation: \n" << cross_validate_multiclass_trainer(trainer, samples, labels, 5) << endl;
    //
    //        // The output is shown below.  It is the confusion matrix which describes the results.  Each row
    //        // corresponds to a class of data and each column to a prediction.  Reading from top to bottom,
    //        // the rows correspond to the class labels if the labels have been listed in sorted order.  So the
    //        // top row corresponds to class 1, the middle row to class 2, and the bottom row to class 3.  The
    //        // columns are organized similarly, with the left most column showing how many samples were predicted
    //        // as members of class 1.
    //        //
    //        // So in the results below we can see that, for the class 1 samples, 60 of them were correctly predicted
    //        // to be members of class 1 and 0 were incorrectly classified.  Similarly, the other two classes of data
    //        // are perfectly classified.
    //        /*
    //            cross validation:
    //            60  0  0
    //            0 70  0
    //            0  0 80
    //        */
    //
    //        // Next, if you wanted to obtain the decision rule learned by a one_vs_one_trainer you
    //        // would store it into a one_vs_one_decision_function.
    //        dlib::one_vs_one_decision_function<ovo_trainer> df = trainer.train(samples, labels);
    //
    //        sample_type_2D newPoint;
    //        newPoint(0) = 15;
    //        newPoint(1) = 30;
    //
    //        std::cout << "predicted label: " << df(newPoint) << endl << endl;
    //    }
    //    catch (std::exception& e)
    //    {
    //        std::cout << "exception thrown!" << endl;
    //        std::cout << e.what() << endl;
    //    }
}

}
