#include <iostream>
#include <vector>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <algorithm>

using namespace std;
string trim (string input){
    int i=0;
    while (i < input.size() && input [i] == ' ')
        i++;
    if (i < input.size())
        input = input.substr (i);
    else{
        return "";
    }
    
    i = input.size() - 1;
    while (i>=0 && input[i] == ' ')
        i--;
    if (i >= 0)
        input = input.substr (0, i+1);
    else
        return "";
    
    return input;
}

vector<string> split (string line, string separator=" "){
    vector<string> result;
    while (line.size()){
        size_t found = line.find(separator);
        if (found == string::npos){
            string lastpart = trim (line);
            if (lastpart.size()>0){
                result.push_back(lastpart);
            }
            break;
        }
        string segment = trim (line.substr(0, found));
        line = line.substr (found+1);
        if (segment.size() != 0) 
            result.push_back (segment);
    }
    return result;
}

char** vec_to_char_array (vector<string> parts){
    char ** result = new char * [parts.size() + 1]; // add 1 for the NULL at the end
    for (int i=0; i<parts.size(); i++){
        // allocate a big enough string
        result [i] = new char [parts [i].size() + 1]; // add 1 for the NULL byte
        strcpy (result [i], parts[i].c_str());
    }
    result [parts.size()] = NULL;
    return result;
}

void execute (string command){
    vector<string> argstrings = split (command, " "); // split the command into space-separated parts
    char** args = vec_to_char_array (argstrings);// convert vec<string> into an array of char*
    execvp (args[0], args);
}

int main (){
    vector<pid_t> processID;
    vector<pid_t> finishedID;

    while (true){ // repeat this loop until the user presses Ctrl + C
        for(int i=0;i<processID.size();++i){
            pid_t temp = waitpid(processID[i],0,WNOHANG);
            if(temp == processID[i]){
                finishedID.push_back(processID[i]);
                processID.erase(processID.begin()+i);
            }
            else if(temp == -1){
                finishedID.push_back(processID[i]);
                processID.erase(processID.begin()+i);
            }
        }
        char temp[4096];
        getlogin_r(temp,sizeof(temp));
        char temp1[4096];
        getcwd(temp1,sizeof(temp1));

        string default_console = "\033["+to_string(0)+"m";
        string green = "\033[0;32m";
        string blue = "\033[0;34m";
        cout<<green<<temp<<default_console<<":"<<blue<<"~"<<temp1<<"$ "<<default_console;
        bool redirectInput = false;
        string inputFile;
        bool redirectOutput = false;
        string outputFile;

        string commandline = "";/*get from STDIN, e.g., "ls  -la |   grep Jul  | grep . | grep .cpp" */
        getline(cin,commandline);
        
        int stdinFd = dup(0); //preserve origional input

        int dquoteCount = 0;
        int squoteCount = 0;
        for(int i=0;i<commandline.size();++i){
            if(commandline[i]== '\"')dquoteCount++;
            if(commandline[i]== '\'')squoteCount++;
            if(commandline[i]== '|'){
                if(dquoteCount%2==1 || squoteCount%2==1){
                    commandline[i] = '\0';
                }
            }
            if(commandline[i]== '<'){
                if(dquoteCount%2==1 || squoteCount%2==1){
                    commandline[i] = '\1';
                }
            }
            if(commandline[i]== '>'){
                if(dquoteCount%2==1 || squoteCount%2==1){
                    commandline[i] = '\2';
                }
            }
        }
        // split the command by the "|", which tells you the pipe levels
        vector<string> tparts = split (commandline, "|");
        // for each pipe, do the following:
        for (int i=0; i<tparts.size(); i++){
            tparts[i].erase(remove(tparts[i].begin(),tparts[i].end(),'\"'),tparts[i].end());
            tparts[i].erase(remove(tparts[i].begin(),tparts[i].end(),'\''),tparts[i].end());
            bool begin = false;
            for(int j=0;j<tparts[i].size();++j){
                if(tparts[i][j] == '{')begin = true;
                if(tparts[i][j] == '}')break;
                if(begin && tparts[i][j]==' '){
                    tparts[i].erase(j,1);
                    j--;
                }
            }

            for(int j=0;j<tparts[i].size();++j){
                if(tparts[i][j] == '<'){
                    vector<string> currComm = split(tparts[i], "<");
                    vector<string> inputFile = split(currComm[1],">");
                    if(inputFile[1].size() > 0){
                        tparts[i] = currComm[0]+">"+inputFile[1];
                    }
                    else{
                        tparts[i] = currComm[0];
                    }
                    inputFile[0].erase(remove(inputFile[0].begin(),inputFile[0].end(),' '),inputFile[0].end());
                    int fd1 = open(inputFile[0].c_str(),O_RDONLY);
                    dup2(fd1,0);
                    close(fd1);
                    break;
                }
            }

            // make pipe
            int fd[2];
            pipe(fd);

            bool bgP = false;
            for(int j=0;j<tparts[i].size();++j){
                if(tparts[i][j]== '&'){
                    bgP = true;
                }
            }
            if(bgP){
                tparts[i].erase(remove(tparts[i].begin(),tparts[i].end(),'&'),tparts[i].end());
            }
            bool isCD = false;
            for(int j=0;j<tparts[i].size()-1;++j){
                if(tparts[i][j] == 'c' && tparts[i][j+1]=='d'){
                    isCD = true;
                }
            }
            if(isCD){
                tparts[i] = tparts[i].substr(3,tparts[i].size()-2);
                char temp1[4096];
                getcwd(temp1,sizeof(temp1));
                if(tparts[i]!="-"){
                    setenv("OLDPWD",temp1,1);
                    chdir(tparts[i].c_str());
                }
                if(tparts[i]=="-"){
                    string OLDPWD = getenv("OLDPWD");
                    setenv("OLDPWD",getenv("PWD"),1);
                    setenv("PWD",OLDPWD.c_str(),1);
                    chdir(getenv("PWD"));
                }
                if(tparts[i]!="-"){
                    char temp[4096];
                    getcwd(temp,sizeof(temp));
                    setenv("PWD",temp,1);
                    setenv("OLDPWD",temp1,1);
                }
            }
            pid_t pid = fork();
            
			if (!pid){
                for(int j=0;j<tparts[i].size();++j){
                    if(tparts[i][j] == '>'){
                        vector<string> currComm = split(tparts[i], ">");
                        tparts[i] = currComm[0];
                        int fd1 = open(currComm[1].c_str(),O_WRONLY | O_TRUNC | O_CREAT , 0777);
                        dup2(fd1,1);
                        close(fd1);
                    }
                }
                if (i < tparts.size() - 1){ 
                    dup2(fd[1],1);
                    close (fd[1]);
                }
                for(int j=0;j<tparts[i].size();++j){
                    if(tparts[i][j] == '\0')tparts[i][j]='|';
                    if(tparts[i][j] == '\1')tparts[i][j]='<';
                    if(tparts[i][j] == '\2')tparts[i][j]='>';
                }
                if(tparts[i]=="jobs"){
                    for(int j=0;j<processID.size();++j){
                        if(processID[j]!= 0 )cout<<processID[j]<<endl;
                    }
                }
                else if(tparts[i]=="dir"){
                    char temp[4096];
                    getcwd(temp,sizeof(temp));
                    cout<<temp<<endl;
                }
                else if(tparts[i]=="user"){
                    char temp[4096];
                    getlogin_r(temp,sizeof(temp));
                    cout<<temp<<endl;
                }
                else if(tparts[i]=="date"){
                    time_t currtime = time(nullptr);
                    cout<<ctime(&currtime)<<endl;
                }
                else if(tparts[i]=="time"){
                    time_t currtime = time(nullptr);
                    cout<<ctime(&currtime)<<endl;
                }
                else{
                    execute (tparts [i]);
                }
            }
			else{
                if(bgP){
                    processID.push_back(pid);
                }
                else{
                    wait(0);
                }
                dup2(fd[0],0); 
                close(fd[1]);          
            }
        }
        dup2(stdinFd,0); //give input back to user
    }
}