#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iomanip>
#include <fstream>
#include "Commands.h"
#include <time.h>


using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cerr << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cerr << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

#define DEBUG_PRINT cerr << "DEBUG: "

#define EXEC(path, arg) \
  execvp((path), (arg));

string _ltrim(const std::string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
  return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)));
  for(std::string s; iss >> s; ) {
    args[i] = new char[s.length()+1];
    memset(args[i], 0, s.length()+1);
    strcpy(args[i], s.c_str());
    args[++i] = nullptr;
  }
  return i;

  FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
  const string str(cmd_line);
  // find last character other than spaces
  int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos) {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&') {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h

/*
 * Command
 */

Command::Command(const char *cmd_line) {

    char* cmd_line_tmp = new char[strlen(cmd_line)+1];
    cmd_line_command = strcpy(cmd_line_tmp, cmd_line);
    args = new char*[COMMAND_MAX_ARGS+2];

    num_of_args = _parseCommandLine(cmd_line, args);

    name = args[0];
}

Command::~Command() {
    delete cmd_line_command;
    for(int i = 0; i < num_of_args ; ++i){
        delete args[i];
    }
    delete[] args;
}

char * Command::get_arg(int i) const {
    return args[i];
}

/*
 * BuiltInCommand
 */

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line) {};

/*
 * RedirectCommand
 */

RedirectionCommand::RedirectionCommand(const char *cmd_line) : Command(cmd_line) {}

void RedirectionCommand::execute() {

    int fd = open(get_arg(2), O_WRONLY | O_CREAT | O_TRUNC , S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    smash.changeCurrFd(fd);
    smash.executeCommand(get_arg(0));
    smash.changeCurrFd(STDOUT_FILENO);
}

/*
 * ChangeDirCommand
 */

ChangeDirCommand::ChangeDirCommand(const char *cmd_line, string plastPwd) : BuiltInCommand(cmd_line),
        lastPwd(plastPwd) {} ;


void ChangeDirCommand::execute() {

    if(strcmp(get_arg(1),"-") == 0){
        string new_dir = lastPwd;
    }
    else {
        lastPwd = get_arg(1);
    }

    smash.changeLastPwd(get_current_dir_name());

    chdir(lastPwd.c_str());


}

/*
 *GetCurrDirCommand
 */

GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {};

void GetCurrDirCommand::execute() {
    string current_dir_name = get_current_dir_name();
    current_dir_name += "\n";
    write(smash.getCurrFd(), current_dir_name.c_str(), current_dir_name.length());
    //TODO: handle with errors (perror)
}

/*
 * ShowPidCommand
 */

ShowPidCommand::ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute() {
    pid_t pid = getpid();
    string message = "smash pid is ";
    message += std::to_string(pid) += "\n";
    write(smash.getCurrFd(), message.c_str(), message.length());
    //TODO: handle with errors (perror)
}

/*
 * ChangePrompt
 */

ChangePrompt::ChangePrompt(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void ChangePrompt::execute() {
    if (get_arg(1) == nullptr) {
        smash.changePrompt("");
        return;
    }
    smash.changePrompt(get_arg(1));
}



/*
 * JobsList
 */

JobsList::JobsList() : jobsList() {}

void JobsList::addJob(Command *cmd, bool isStopped) {
    time_t tmp_time;
    auto* jobEntry = new JobEntry(getpid(), isStopped, jobsList.size()+1, time(nullptr));
    jobsList.push_back(jobEntry);
}


ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line) {}

void ExternalCommand::execute() {
    pid_t pid = fork();
//    char** args_tmp = new char*[num_of_args+1];
//    char* cmd_new = new char[strlen(cmd_line_command)+1+3];
//    strcpy(cmd_new, cmd_line_command);
//    strcat(cmd_new, " -c");
//    args_tmp[num_of_args] = new char[3];
//    strcpy(args_tmp[num_of_args], " -c");
//    for (int i=0; i<num_of_args ; i++){
//        args_tmp[i+1] = new char[strlen(args[i])+1];
//        strcpy(args_tmp[i+1],args[i]);
//    }
//
//    args_tmp[num_of_args+1] = nullptr;
    char* args_t[] = {"-c", "ls", NULL};
    if (pid == 0) { //child
        execv("/bin/bash", args_t);
    }
    else{
        wait(nullptr);
    }
}


SmallShell::SmallShell() : prompt(def_prompt), last_pwd(""), curr_fd(STDOUT_FILENO), jobsList() {
// TODO: add your implementation
}

SmallShell::~SmallShell() {
// TODO: add your implementation
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/

Command *SmallShell::CreateCommand(const char *cmd_line) {
    // For example:

    string cmd_s = string(cmd_line);

    if (cmd_s.find(">") != string::npos) {
        return new RedirectionCommand(cmd_line);
    }

    if (cmd_s.find("pwd") == 0) {
        return new GetCurrDirCommand(cmd_line);
    }

    if (cmd_s.find("showpid") == 0) {
        return new ShowPidCommand(cmd_line);
    }

    if (cmd_s.find("cd") == 0) {
        return new ChangeDirCommand(cmd_line, smash.getLastPwd());
    }

    if (cmd_s.find("chprompt") == 0) {
        return new ChangePrompt(cmd_line);
    }

    return new ExternalCommand(cmd_line);
}

void SmallShell::executeCommand(const char *cmd_line) {
    // TODO: Add your implementation here
    // for example:
    Command *cmd = CreateCommand(cmd_line);
    cmd->execute();
    // Please note that you must fork smash process for some commands (e.g., external commands....)
}

void SmallShell::changePrompt(string new_prompt) {
    if (new_prompt == "") {
        prompt = def_prompt;
    } else {
        prompt = new_prompt;
    }
}

void SmallShell::changeCurrFd(int new_fd) {
    curr_fd = new_fd;
}

int SmallShell::getCurrFd() const {
    return curr_fd;
}

void SmallShell::changeLastPwd(string new_pwd) {
    last_pwd = new_pwd;
}

string SmallShell::getLastPwd() const {
    return last_pwd;
}

string SmallShell::getPrompt() const {
    return prompt;
}

