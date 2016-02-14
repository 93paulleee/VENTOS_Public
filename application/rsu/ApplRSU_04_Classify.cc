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

        GPSerror = par("GPSerror").doubleValue();
        if(GPSerror < 0)
            error("GPSerror value is not correct!");

        debugLevel = simulation.getSystemModule()->par("debugLevel").longValue();

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

        // resize shark_sample to the correct size
        // we have 3 features, thus shark_sample should be 1 * 3
        shark_sample.resize(1,3);

        if(ev.isGUI())
            initializeGnuPlot();

        int status = loadTrainer();
        // cannot load trainer from the disk and there is no training data
        if(status == -1)
        {
            collectTrainingData = true;
            std::cout << "Cannot train the algorithm! Run simulation to collect training data!" << std::endl << std::endl;
        }
        else
        {
            collectTrainingData = false;
            std::cout << "Training was successful." << std::endl << std::endl;
        }
    }
}


void ApplRSUCLASSIFY::finish()
{
    ApplRSUTLVANET::finish();

    if(collectTrainingData)
        saveSampleToFile();

    saveClassificationResults();
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


void ApplRSUCLASSIFY::draw()
{
    if(plotterPtr == NULL)
        return;

    // create a data blocks out of dataset (only gnuplot > 5.0 supports this)
    fprintf(plotterPtr, "$data << EOD \n");

    // data block 1
    //    for(auto &i : dataSet[0])
    //            fprintf(plotterPtr, "%f  %f  %f \n", i.xPos, i.yPos, i.speed);

    // two blank lines as data block separator
    fprintf(plotterPtr, "\n\n \n");

    // data block 2
    //      for(auto &i : dataSet[1])
    //           fprintf(plotterPtr, "%f  %f  %f \n", i.xPos, i.yPos, i.speed);

    // two blank lines as data block separator
    fprintf(plotterPtr, "\n\n \n");

    // data block 3
    //        for(auto &i : dataSet[2])
    //            fprintf(plotterPtr, "%f  %f  %f \n", i.xPos, i.yPos, i.speed);

    // data block terminator
    fprintf(plotterPtr, "EOD \n");

    // make plot
    fprintf(plotterPtr, "splot '$data' index 0 using 1:2:3 with points ls 1 title 'passenger',");
    fprintf(plotterPtr, "      ''      index 1 using 1:2:3 with points ls 2 title 'bicycle',");
    fprintf(plotterPtr, "      ''      index 2 using 1:2:3 with points ls 3 title 'pedestrian' \n");

    fflush(plotterPtr);
}


int ApplRSUCLASSIFY::loadTrainer()
{
    shark::GaussianRbfKernel<> *kernel = new shark::GaussianRbfKernel<shark::RealVector> (0.5 /*kernel bandwidth parameter*/);

    kc_model = new shark::KernelClassifier<shark::RealVector> (kernel);

    // Training of a multi-class SVM by the one-versus-all (OVA) method
    shark::AbstractSvmTrainer<shark::RealVector, unsigned int> *trainer[2];
    trainer[0]  = new shark::McSvmOVATrainer<shark::RealVector>(kernel, 10.0, false /*without bias*/);
    trainer[1]  = new shark::McSvmOVATrainer<shark::RealVector>(kernel, 10.0, true  /*with bias*/);

    // Training of a binary-class SVM
    // shark::CSvmTrainer<shark::RealVector> trainer(kernel, 1000.0 /*regularization parameter*/, true /*with bias*/);

    for (int i = 0; i <= 1; i++)
    {
        std::string bias = trainer[i]->trainOffset()? "_withBias":"_withoutBias";
        std::string fileName = trainer[i]->name() + bias + ".model";
        boost::filesystem::path filePath = "results/ML/" + fileName;

        // check if this model was trained before
        std::ifstream ifs(filePath.string());
        if(!ifs.fail())
        {
            std::cout << "loading " << fileName << " from disk... ";
            boost::archive::polymorphic_text_iarchive ia(ifs);
            kc_model->read(ia);
            ifs.close();
            std::cout << "done \n";
        }
        else
        {
            int status = trainClassifier(trainer[i]);

            // check if training was successful
            if(status == -1)
                return -1;

            // save the model to file
            std::ofstream ofs(filePath.string());
            boost::archive::polymorphic_text_oarchive oa(ofs);
            kc_model->write(oa);
            ofs.close();
        }
    }

    // read classes from file
    std::cout << "reading classes... " << std::flush;
    std::string cName;
    unsigned int cNum;
    std::ifstream infile(trainingClassFilePath.string());
    if(infile.fail())
    {
        std::cout << trainingClassFilePath.string() << " not found! \n";
        return -1;
    }

    while (infile >> cName >> cNum)
    {
        auto se = entityClasses.find(cName);
        if(se == entityClasses.end())
            entityClasses[cName] = cNum;
        else
        {
            std::cout << "duplicate class name " << cName.c_str() << "\n";
            return -1;
        }
    }

    if(entityClasses.empty())
    {
        std::cout << "No classes found! \n";
        return -1;
    }

    std::cout << "done \n";

    return 0;
}


int ApplRSUCLASSIFY::trainClassifier(shark::AbstractSvmTrainer<shark::RealVector, unsigned int> *trainer)
{
    // load sampleData only once
    if(sampleData.elements().empty())
    {
        try
        {
            std::cout << "reading training samples... " << std::flush;
            // Load data from external file
            shark::importCSV(sampleData, trainingFilePath.string(), shark::LAST_COLUMN /*label position*/, ' ' /*separator*/);
            std::cout << sampleData.elements().size() << " samples fetched! \n" << std::flush;
        }
        catch (std::exception& e)
        {
            std::cout << std::endl << e.what() << std::endl;
            return -1;
        }

        std::cout << "number of classes: " << numberOfClasses(sampleData) << std::endl;
        int classCount = 0;
        for(auto i : classSizes(sampleData))
        {
            std::printf("  class %-2d: %-5lu, ", classCount, i);
            classCount++;

            if(classCount % 3 == 0)
                std::cout << std::endl;
        }

        std::cout << "shuffling training samples... " << std::flush;
        sampleData.shuffle();   // shuffle sampleData
        std::cout << "done \n" << std::flush;
    }

    std::printf("training model %10s %s... \n",
            trainer->name().c_str(),
            trainer->trainOffset()? "with bias":"without bias");
    std::cout.flush();

    // start training
    trainer->train(*kc_model, sampleData);

    std::printf("  iterations= %d, accuracy= %f, time= %g seconds \n",
            (int)trainer->solutionProperties().iterations,
            trainer->solutionProperties().accuracy,
            trainer->solutionProperties().seconds);

    std::cout << "  training error= " << std::flush;

    // evaluate the model on training set
    shark::ZeroOneLoss<unsigned int> loss; // 0-1 loss
    shark::Data<unsigned int> output = (*kc_model)(sampleData.inputs());
    double train_error = loss.eval(sampleData.labels(), output);

    std::cout << train_error << std::endl << std::flush;

    return 0;
}


// update variables upon reception of any beacon (vehicle, bike, pedestrian)
template <typename beaconGeneral>
void ApplRSUCLASSIFY::onBeaconAny(beaconGeneral wsm)
{
    if(collectTrainingData)
    {
        collectSample(wsm);
        return;
    }

    // on one of the incoming lanes?
    std::string lane = wsm->getLane();
    auto it = lanesTL.find(lane);

    // not on any incoming lanes
    if(it == lanesTL.end())
        return;

    // I do not control this lane!
    if(it->second != myTLid)
        return;

    // retrieve info from beacon
    double posX = wsm->getPos().x;
    double posY = wsm->getPos().y;
    double speed = wsm->getSpeed();

    if(GPSerror != 0)
    {
        double r = 0;

        // Produce a random double in the range [0,1) using generator 0, then scale it to -1 <= r < 1
        r = (dblrand() - 0.5) * 2;
        // add error to posX
        posX = posX + (r * GPSerror);

        // Produce a random double in the range [0,1) using generator 0, then scale it to -1 <= r < 1
        r = (dblrand() - 0.5) * 2;
        // add error to posY
        posY = posY + (r * GPSerror);

        // Produce a random double in the range [0,1) using generator 0, then scale it to -1 <= r < 1
        r = (dblrand() - 0.5) * 2;
        // add error to speed
        //speed = speed + speed * (r * GPSerror);  // todo
    }

    shark_sample(0,0) = posX;  // xPos
    shark_sample(0,1) = posY;  // yPos
    shark_sample(0,2) = speed; // speed

    // make prediction
    unsigned int predicted_label = (*kc_model)(shark_sample)[0];

    // get the real label
    auto re = entityClasses.find(lane);
    if(re == entityClasses.end())
        error("class %s not found!", lane.c_str());
    unsigned int real_label = re->second;

    // save classification results
    std::string sender = wsm->getSender();
    resultEntry *res = new resultEntry(predicted_label, real_label, simTime().dbl());
    auto xr = classifyResults.find(sender);
    if(xr == classifyResults.end())
    {
        std::vector<resultEntry> tmp {*res};
        classifyResults[sender] = tmp;
    }
    else
        xr->second.push_back(*res);

    // save classification statistics
    bool correct = (predicted_label == real_label) ? true : false;
    auto xs = classifyStat.find(sender);
    if(xs == classifyStat.end())
    {
        statEntry *res = new statEntry(1, correct ? 1 : 0);
        classifyStat.insert(std::make_pair(sender, *res));
    }
    else
    {
        xs->second.total_predicted += 1;
        if(correct)
            xs->second.correct_predicted += 1;
    }

    // print debug information
    if(debugLevel > 1)
    {
        printf("%0.3f, %0.3f, %06.3f --> predicted label: %2d, true label: %2d, sender: %s \n",
                shark_sample(0,0),
                shark_sample(0,1),
                shark_sample(0,2),
                predicted_label,
                real_label,
                sender.c_str());

        std::cout.flush();
    }
}


template <typename beaconGeneral>
void ApplRSUCLASSIFY::collectSample(beaconGeneral wsm)
{
    std::string lane = wsm->getLane();

    auto it = lanesTL.find(lane);

    // not on any incoming lanes
    if(it == lanesTL.end())
        return;

    // I do not control this lane!
    if(it->second != myTLid)
        return;

    // make an instance
    Coord pos = wsm->getPos();
    sample_type *m = new sample_type(pos.x, pos.y, wsm->getSpeed());

    // check instance's class
    auto it2 = entityClasses.find(lane);
    if(it2 == entityClasses.end())
    {
        int classNum = entityClasses.size();
        entityClasses[lane] = classNum;

        samples.push_back(*m);
        labels.push_back(classNum);
    }
    else
    {
        samples.push_back(*m);
        labels.push_back(it2->second);
    }
}


void ApplRSUCLASSIFY::saveSampleToFile()
{
    FILE *filePtr = fopen (trainingFilePath.string().c_str(), "w");

    for(unsigned int i = 0; i < samples.size(); ++i)
        fprintf (filePtr, "%f %f %f %d \n", samples[i].xPos, samples[i].yPos, samples[i].speed, labels[i]);

    fclose(filePtr);

    filePtr = fopen (trainingClassFilePath.string().c_str(), "w");

    for(auto i : entityClasses)
        fprintf (filePtr, "%s %d \n", (i.first).c_str(), i.second);

    fclose(filePtr);
}


void ApplRSUCLASSIFY::saveClassificationResults()
{
    boost::filesystem::path filePath;

    if(ev.isGUI())
    {
        filePath = "results/ML/classificationResults.txt";
    }
    else
    {
        // get the current run number
        int currentRun = ev.getConfigEx()->getActiveRunNumber();
        std::ostringstream fileName;
        fileName << std::setfill('0') << std::setw(3) << currentRun << "_classificationResults.txt";
        filePath = "results/ML/cmd/" + fileName.str();
    }

    FILE *filePtr = fopen (filePath.string().c_str(), "w");

    // write simulation parameters at the beginning of the file in CMD mode
    if(!ev.isGUI())
    {
        // get the current config name
        std::string configName = ev.getConfigEx()->getVariable("configname");

        // get number of total runs in this config
        int totalRun = ev.getConfigEx()->getNumRunsInConfig(configName.c_str());

        // get the current run number
        int currentRun = ev.getConfigEx()->getActiveRunNumber();

        // get all iteration variables
        std::vector<std::string> iterVar = ev.getConfigEx()->unrollConfig(configName.c_str(), false);

        // write to file
        fprintf (filePtr, "configName      %s\n", configName.c_str());
        fprintf (filePtr, "totalRun        %d\n", totalRun);
        fprintf (filePtr, "currentRun      %d\n", currentRun);
        fprintf (filePtr, "currentConfig   %s\n\n\n", iterVar[currentRun].c_str());
    }

    for(auto i : classifyResults)
    {
        fprintf (filePtr, "%s ", i.first.c_str());

        auto r = classifyStat.find(i.first);
        if(r == classifyStat.end())
            error("cannot find %s in classifyStat", i.first.c_str());
        else
            fprintf (filePtr, "%u %u ", r->second.total_predicted, r->second.correct_predicted);

        for(auto j : i.second)
            fprintf (filePtr, "%0.2f %u %u ", j.time, j.label_predicted, j.label_true);

        fprintf (filePtr, "\n\n");
    }

    fclose(filePtr);
}

}
