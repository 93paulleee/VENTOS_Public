
#include "SumoBinary.h"
#include <curl/curl.h>

namespace VENTOS {

Define_Module(VENTOS::SumoBinary);

void SumoBinary::initialize(int stage)
{
    if(stage == 0)
    {
        SUMO_GUI_FileName = par("SUMO_GUI_FileName").stringValue();
        SUMO_CMD_FileName = par("SUMO_CMD_FileName").stringValue();

        SUMO_GUI_URL = par("SUMO_GUI_URL").stringValue();
        SUMO_CMD_URL = par("SUMO_CMD_URL").stringValue();
        SUMO_Version_URL = par("SUMO_Version_URL").stringValue();

        VENTOS_FullPath = cSimulation::getActiveSimulation()->getEnvir()->getConfig()->getConfigEntry("network").getBaseDirectory();
        SUMO_Binary_FullPath = VENTOS_FullPath / "sumoBinary";

        SUMO_GUI_Binary_FullPath = SUMO_Binary_FullPath / SUMO_GUI_FileName;
        SUMO_CMD_Binary_FullPath = SUMO_Binary_FullPath / SUMO_CMD_FileName;

        update = par("update").boolValue();

        // todo: check each day!
        // a way to turn this feature off
        checkIfBinaryExists();
    }
}


void SumoBinary::finish()
{

}


void SumoBinary::handleMessage(cMessage *msg)
{

}


void SumoBinary::checkIfBinaryExists()
{
    // both binaries are missing
    if( !exists( SUMO_CMD_Binary_FullPath ) && !exists( SUMO_GUI_Binary_FullPath ) )
    {
        cout << "\nNo SUMO binaries found in " << SUMO_Binary_FullPath.string() << endl;
        cout << "Do you want to download the latest SUMO binaries? [y/n]: ";
        string answer;
        cin >> answer;
        boost::algorithm::to_lower(answer);

        if(answer == "y")
        {
            downloadBinary(SUMO_GUI_FileName, SUMO_GUI_Binary_FullPath.string(), SUMO_GUI_URL);
            downloadBinary(SUMO_CMD_FileName, SUMO_CMD_Binary_FullPath.string(), SUMO_CMD_URL);
        }
        else
            cout << "Ok! have fun." << endl;
    }
    // only GUI binary is missing
    else if( exists( SUMO_CMD_Binary_FullPath ) && !exists( SUMO_GUI_Binary_FullPath ) )
    {
        cout << "\nSUMO GUI binary is missing in " << SUMO_Binary_FullPath.string() << endl;
        cout << "Do you want to download it ? [y/n]: ";
        string answer;
        cin >> answer;
        boost::algorithm::to_lower(answer);

        if(answer == "y")
            downloadBinary(SUMO_GUI_FileName, SUMO_GUI_Binary_FullPath.string(), SUMO_GUI_URL);

        checkIfNewerVersionExists(SUMO_CMD_FileName, SUMO_CMD_Binary_FullPath.string(), SUMO_CMD_URL);
    }
    // only CMD binary is missing
    else if( !exists( SUMO_CMD_Binary_FullPath ) && exists( SUMO_GUI_Binary_FullPath ) )
    {
        cout << "\nSUMO CMD binary is missing in " << SUMO_Binary_FullPath.string() << endl;
        cout << "Do you want to download it ? [y/n]: ";
        string answer;
        cin >> answer;
        boost::algorithm::to_lower(answer);

        if(answer == "y")
            downloadBinary(SUMO_CMD_FileName, SUMO_CMD_Binary_FullPath.string(), SUMO_CMD_URL);

        checkIfNewerVersionExists(SUMO_GUI_FileName, SUMO_GUI_Binary_FullPath.string(), SUMO_GUI_URL);
    }
    // both binaries exists, check if newer version is available?
    else
    {
        checkIfNewerVersionExists(SUMO_GUI_FileName, SUMO_GUI_Binary_FullPath.string(), SUMO_GUI_URL);
        checkIfNewerVersionExists(SUMO_CMD_FileName, SUMO_CMD_Binary_FullPath.string(), SUMO_CMD_URL);
    }
}


size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

void SumoBinary::downloadBinary(string binaryName, string filePath, string url)
{
    cout << "Downloading " << binaryName << " ... ";
    cout.flush();

    FILE *fp = fopen(filePath.c_str(), "wb");
    if(fp == NULL)
    {
        fprintf(stderr, " failed! (%s)\n", "writing error! Do you have permission?");
        return;
    }

    CURL *curl = curl_easy_init();

    if (curl == NULL)
    {
        fprintf(stderr, " failed! (%s)\n", "error in curl_easy_init");
        fclose(fp);
        return;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    // we tell libcurl to follow redirection
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 6);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

    CURLcode res = curl_easy_perform(curl);
    if(res != CURLE_OK)
    {
        fprintf(stderr, " failed! (%s)\n", curl_easy_strerror(res));
        fclose(fp);
        return;
    }
    else
    {
        cout << " done!" << endl;
        fclose(fp);
        makeExecutable(binaryName, filePath);
    }

    curl_easy_cleanup(curl);
}


void SumoBinary::makeExecutable(string binaryName, string filePath)
{
    cout << "Making " << binaryName << " executable ... ";
    cout.flush();

    char command[100];
    sprintf(command, "chmod +x %s", filePath.c_str());

    FILE* pipe = popen(command, "r");
    if (!pipe)
    {
        cout << "failed! (can not open pipe)" << endl;
        return;
    }

    char buffer[128];
    string result = "";
    while(!feof(pipe))
    {
        if(fgets(buffer, 128, pipe) != NULL)
            result += buffer;
    }
    pclose(pipe);

    cout << " done!" << endl;
}


static std::string remoteVer;

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    remoteVer.append((const char*)contents, realsize);
    return realsize;
}

// get remote file and save it in memory
int SumoBinary::getRemoteVersion()
{
    remoteVer.clear();

    CURL *curl = curl_easy_init();
    if (curl == NULL)
    {
        fprintf(stderr, " failed! (%s)\n", "error in curl_easy_init");
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, SUMO_Version_URL.c_str());
    // we tell libcurl to follow redirection
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 6);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    /* send all data to this function  */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

    CURLcode res = curl_easy_perform(curl);
    if(res != CURLE_OK)
    {
        fprintf(stderr, " failed! (%s)\n", curl_easy_strerror(res));
        return -1;
    }
    else
        return 1;

    curl_easy_cleanup(curl);
}


void SumoBinary::checkIfNewerVersionExists(string binaryName, string filePath, string url)
{
    if(!update)
        return;

    cout << "Checking for " << binaryName << "'s update ... ";
    cout.flush();

    // get the local version
    char command[100];
    sprintf(command, "%s -V", filePath.c_str());

    FILE* pipe = popen(command, "r");
    if (!pipe)
    {
        cout << "failed! (can not open pipe)" << endl;
        return;
    }

    char buffer[128];
    string result = "";
    while(!feof(pipe))
    {
        if(fgets(buffer, 128, pipe) != NULL)
            result += buffer;
    }
    pclose(pipe);

    // get the first line of the output
    char_separator<char> sep("\n");
    tokenizer< char_separator<char> > tokens(result, sep);
    string firstLine;
    for(tokenizer< char_separator<char> >::iterator beg=tokens.begin(); beg!=tokens.end();++beg)
    {
        firstLine = (*beg).c_str();
        break;
    }

    std::size_t pos = firstLine.find("dev-SVN-");
    string localVer = firstLine.substr(pos);

    // now get the remote version
    int val = getRemoteVersion();

    if(val == -1)
        return;

    // get the first line of remoteVer
    char_separator<char> sep2("\n");
    tokenizer< char_separator<char> > tokens2(remoteVer, sep2);
    for(tokenizer< char_separator<char> >::iterator beg=tokens2.begin(); beg!=tokens2.end();++beg)
    {
        remoteVer = (*beg).c_str();
        break;
    }

    if( localVer.compare(remoteVer) == 0 )
        cout << "Up to date!" << endl;
    else if( localVer.compare(remoteVer) < 0 )
    {
        cout << "\nYour current version is " << localVer << endl;
        cout << "Do you want to update to version " << remoteVer << " ? [y/n]: ";
        string answer;
        cin >> answer;
        boost::algorithm::to_lower(answer);

        if(answer == "y")
            downloadBinary(binaryName, filePath, url);
    }
}

} // namespace


