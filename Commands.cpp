#include <unistd.h>
#include <cstring>
#include <iostream>
#include <utility>
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
    SmallShell& smash = SmallShell::getInstance();
    int fd = open(get_arg(2), O_WRONLY | O_TRUNC , S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1){
        return perror("smash error: open failed");
    }
    smash.changeCurrFd(fd);
    smash.executeCommand(get_arg(0));
    smash.changeCurrFd(STDOUT_FILENO);
    if(close(fd) == -1){
        perror("smash error: close failed");
    }

}

/*
 * ChangeDirCommand
 */

ChangeDirCommand::ChangeDirCommand(const char *cmd_line, string plastPwd) : BuiltInCommand(cmd_line),
        lastPwd(std::move(plastPwd)) {} ;


void ChangeDirCommand::execute() {

    if(strcmp(get_arg(1),"-") == 0){
        string new_dir = lastPwd;
    }
    else {
        lastPwd = get_arg(1);
    }
    char* cdn = get_current_dir_name();

    if (cdn == nullptr){
        return perror("smash error: get_current_dir_name failed");
    }

    SmallShell::getInstance().changeLastPwd(cdn);

    if(chdir(lastPwd.c_str()) == -1){
        perror("smash error: chdir failed");
    }


}

/*
 *GetCurrDirCommand
 */

GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {};

void GetCurrDirCommand::execute() {
    char* current_dir_name = get_current_dir_name();
    if(current_dir_name == nullptr){
        return perror("smash error: get_current_dir_name failed");
    }
    strcat(current_dir_name, "\n");
    if(strlen(current_dir_name) != write(SmallShell::getInstance().getCurrFd(), current_dir_name, strlen(current_dir_name))){
        perror("smash error: write failed");
    }
}

/*
 * ShowPidCommand
 */

ShowPidCommand::ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute() {
    pid_t pid = SmallShell::getInstance().getPid();
    string message = "smash pid is ";
    message += std::to_string(pid) += "\n";
    if (message.length() != write(SmallShell::getInstance().getCurrFd(), message.c_str(), message.length())){
        perror("smash error: write failed");
    }
}

/*
 * ChangePrompt
 */

ChangePrompt::ChangePrompt(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void ChangePrompt::execute() {
    if (get_arg(1) == nullptr || _isBackgroundComamnd(get_arg(1))) {
        SmallShell::getInstance().changePrompt("");
        return;
    }
    SmallShell::getInstance().changePrompt(get_arg(1));
}



/*
 * JobsList
 */

JobsList::JobsList() : jobsList() {}

void JobsList::addJob(const char* cmd, pid_t pid, bool isStopped) {
//    time_t tmp_time;
    JobEntry jobEntry(pid , findMaxJobId()+1, time(nullptr), isStopped, cmd);
    jobsList.push_back(jobEntry);
}

void JobsList::printJobsList() {
    for (auto & it : jobsList){
        string job = "[";
        job += std::to_string(it.job_id) += "] ";
        job += it.cmd;
        job += " : ";
        job += difftime(time(nullptr), it.time_added);
        job += " secs";
        if(it.stopped){
            job += "(stopped)";
        }
        job += "\n";
        if(job.length() != write(SmallShell::getInstance().getCurrFd(), job.c_str(), job.length())){
            perror("smash error: write failed");
        }
    }
}

int JobsList::findMaxJobId() const {
    return jobsList.front().job_id;
}

ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line) {}

void ExternalCommand::execute() {
    pid_t pid = fork();

    if (pid == 0) { //child
        const char **args_t = new const char*[4];
        args_t[0] = "/bin/bash";
        args_t[1] = "-c";
        args_t[2] = cmd_line_command;
        args_t[3] = nullptr;
        execv(args_t[0], const_cast<char**>(args_t));
    }
    else if (pid == -1){
        perror("smash error: fork failed");
    }
    else{
        SmallShell::getInstance().current_fg_pid = pid;
        waitpid(pid, nullptr,  WUNTRACED);
    }
}


SmallShell::SmallShell() : prompt(def_prompt), last_pwd(""), curr_fd(STDOUT_FILENO), pid(getpid()),jobsList() {

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

    if (cmd_s.find('>') != string::npos) {
        return new RedirectionCommand(cmd_line);
    }

    if (cmd_s.find("pwd") == 0) {
        return new GetCurrDirCommand(cmd_line);
    }

    if (cmd_s.find("showpid") == 0) {
        return new ShowPidCommand(cmd_line);
    }

    if (cmd_s.find("cd") == 0) {
        return new ChangeDirCommand(cmd_line, SmallShell::getInstance().getLastPwd());
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
    SmallShell::getInstance().cmd_line_fg = cmd_line;
    cmd->execute();
    // Please note that you must fork smash process for some commands (e.g., external commands....)
}

void SmallShell::changePrompt(const string& new_prompt) {
    if (new_prompt.empty()) {
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
    last_pwd = std::move(new_pwd);
}

string SmallShell::getLastPwd() const {
    return last_pwd;
}

string SmallShell::getPrompt() const {
    return prompt;
}

pid_t SmallShell::getPid() const {
    return pid;
}

const char * SmallShell::getCmdLine() const {
    return cmd_line_fg;
}

void SmallShell::addJob(const char* cmd, pid_t pidIn, bool isStopped) {
    jobsList.addJob(cmd, pidIn, isStopped);
}

