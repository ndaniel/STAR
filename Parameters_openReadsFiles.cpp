#include "Parameters.h"
#include "ErrorWarning.h"
#include <fstream>
#include <sys/stat.h>
void Parameters::openReadsFiles() {
    string readFilesCommandString("");
    if (readFilesCommand.at(0)=="-") {
        if (readFilesIn.at(0).find(',')<readFilesIn.at(0).size()) readFilesCommandString="cat";//concatenate multiple files
    } else {
        for (uint ii=0; ii<readFilesCommand.size(); ii++) readFilesCommandString+=readFilesCommand.at(ii)+"   ";
    };

    if (readFilesCommandString=="") {//read from file
        for (uint ii=0;ii<readNmates;ii++) {//open readIn files
            if ( inOut->readIn[ii].is_open() ) inOut->readIn[ii].close();
            inOut->readIn[ii].open(readFilesIn.at(ii).c_str()); //try to open the Sequences file right away, exit if failed
            if (inOut->readIn[ii].fail()) {
                ostringstream errOut;
                errOut <<"EXITING because of fatal input ERROR: could not open readFilesIn=" << readFilesIn.at(ii) <<"\n";
                exitWithError(errOut.str(), std::cerr, inOut->logMain, EXIT_CODE_PARAMETER, *this);                
            };  
        };    
    } else {//create fifo files, execute pre-processing command

         vector<string> readsCommandFileName;

         readFilesNames.resize(readNmates); 

         for (uint imate=0;imate<readNmates;imate++) {//open readIn files
            ostringstream sysCom;
            sysCom << outFileTmp <<"tmp.fifo.read"<<imate+1;
            readFilesInTmp.push_back(sysCom.str());
            remove(readFilesInTmp.at(imate).c_str());                
            mkfifo(readFilesInTmp.at(imate).c_str(), S_IRUSR | S_IWUSR );

            inOut->logMain << "Input read files for mate "<< imate+1 <<", from input string " << readFilesIn.at(imate) <<endl;

            readsCommandFileName.push_back(outFileTmp+"/readsCommand_read" + to_string(imate+1));
            ofstream readsCommandFile( readsCommandFileName.at(imate).c_str());

            string readFilesInString(readFilesIn.at(imate));
            size_t pos=0;
            readFilesN=0;
            do {//cycle over multiple files separated by comma
                pos = readFilesInString.find(',');
                string file1 = readFilesInString.substr(0, pos);
                readFilesInString.erase(0, pos + 1);
                readFilesNames.at(imate).push_back(file1);
                inOut->logMain <<file1<<endl;

                readsCommandFile << "echo FILE " <<readFilesN << " > " << ("\""+readFilesInTmp.at(imate)+"\"") << "\n";
                readsCommandFile << readFilesCommandString << "   " <<("\""+file1+"\"") << (" > \""+readFilesInTmp.at(imate)+"\"") <<"\n";
                ++readFilesN;//only increase file count for one mate

            } while (pos!= string::npos);

            readsCommandFile.close();
            chmod(readsCommandFileName.at(imate).c_str(),S_IXUSR | S_IRUSR | S_IWUSR);
            
            readFilesCommandPID[imate]=0;
            
            pid_t PID=fork();
            switch (PID) {
                case -1:
                    exitWithError("EXITING: because of fatal EXECUTION error: someting went wrong with forking readFilesCommand", std::cerr, inOut->logMain, EXIT_CODE_PARAMETER, *this);
                    break;
                
                case 0:
                    //this is the child
                    execlp(readsCommandFileName.at(imate).c_str(), readsCommandFileName.at(imate).c_str(), (char*) NULL); 
                    exit(0);
                    
                default:
                    //this is the father, record PID of the children
                    readFilesCommandPID[imate]=PID;
            };
            
//             system((("\""+readsCommandFileName.at(imate)+"\"") + " & ").c_str());
            inOut->readIn[imate].open(readFilesInTmp.at(imate).c_str());                
        };
        if (readNmates==2 && readFilesNames.at(0).size() != readFilesNames.at(1).size()) {
            ostringstream errOut;
            errOut <<"EXITING: because of fatal INPUT ERROR: number of input files for mate1: "<<readFilesNames.at(0).size()  << " is not equal to that for mate2: "<< readFilesNames.at(1).size() <<"\n";
            errOut <<"Make sure that the number of files in --readFilesIn is the same for both mates\n";
            exitWithError(errOut.str(), std::cerr, inOut->logMain, EXIT_CODE_PARAMETER, *this);                
        };

        if (outSAMattrRG.size()>1 && outSAMattrRG.size()!=readFilesN) {
            ostringstream errOut;
            errOut <<"EXITING: because of fatal INPUT ERROR: number of input read files: "<<readFilesN << " does not agree with number of read group RG entries: "<< outSAMattrRG.size() <<"\n";
            errOut <<"Make sure that the number of RG lines in --outSAMattrRGline is equal to either 1, or the number of input read files in --readFilesIn\n";
            exitWithError(errOut.str(), std::cerr, inOut->logMain, EXIT_CODE_PARAMETER, *this);                
        } else if (outSAMattrRG.size()==1) {//use the same read group for all files              
            for (uint32 ifile=1;ifile<readFilesN;ifile++) {
                outSAMattrRG.push_back(outSAMattrRG.at(0));
            };
        };
    };
    readFilesIndex=0;
};