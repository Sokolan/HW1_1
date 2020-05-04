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

#define write_with_error(error) \
if (write(STDOUT_FILENO, error.c_str(), error.length()) != error.length()) {\
perror("smash error: write failed");\
}

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

    cmd_line_command = new char[strlen(cmd_line)+1];
    strcpy(cmd_line_command, cmd_line);
    args = new char*[COMMAND_MAX_ARGS+2];

    num_of_args = _parseCommandLine(cmd_line, args);

}

Command::~Command() {
    delete[] cmd_line_command;
    for(int i = 0; args[i] != nullptr ; ++i){
        delete[] args[i];
    }
    delete[] args;
}

char * Command::get_arg(int i) const {
    return args[i];
}

/*
 * BuiltInCommand
 */

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line) {
    SmallShell::getInstance().fg_is_pgid = false;
};



/*
 * PipeCommand
 */

static pid_t cmd_1_pid;
static pid_t cmd_2_pid;
static bool in_pipe = false;

PipeCommand::PipeCommand(const char *cmd_line) : Command(cmd_line) {
    int command1_offset = string(cmd_line_command).find('|');
    int command2_offset = command1_offset+1;
    if (string(cmd_line_command).find('&') == (command1_offset+1)){
        command2_offset++;
        std_err = true;
    }
    else{
        std_err = false;
    }
    command1 = _trim(string(cmd_line_command).erase(command1_offset));
    command2 = _trim(string(cmd_line_command).substr(++command2_offset));
    if(_isBackgroundComamnd(command2.c_str())){
        int pos = command2.find('&');
        command2.erase(pos);
        bg = true;
    }

}

void PipeCommand::execute() {
    pid_t pid = fork();
    setpgrp();
    if (pid == 0){ //pipe process
//        int i = 0;
        in_pipe = true;
        int pipefd[2];
        if(pipe(pipefd) == -1){
            perror("smash error: pipe failed");
            exit(EXIT_FAILURE);
        }


        cmd_1_pid = fork();
        if(cmd_1_pid == -1){
            perror("smash error: fork failed");
            exit(EXIT_FAILURE);
        }
        if(cmd_1_pid == 0){// cmd1_process
            if(std_err){
                dup2(pipefd[1], STDERR_FILENO);
            }
            else{
                dup2(pipefd[1], STDOUT_FILENO);
            }

            close(pipefd[1]);
            close(pipefd[0]);
            SmallShell::getInstance().executeCommand(command1.c_str());
        }


        cmd_2_pid = fork();
        if (cmd_2_pid == -1){
            perror("smash error: fork failed");
            exit(EXIT_FAILURE);
        }
        if (cmd_2_pid == 0){ //cmd2_process
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[1]);
            close(pipefd[0]);
            SmallShell::getInstance().executeCommand(command2.c_str());
        }

        close(pipefd[1]);
        close(pipefd[0]);

//        if(signal(SIGTSTP , pipeCtrlZHandler)==SIG_ERR) {
//            perror("smash error: failed to set SIGSTOP handler");
//        }
//        if(signal(SIGCONT , pipeSIGCONTHadnler)==SIG_ERR) {
//            perror("smash error: failed to set SIGSTOP handler");
//        }
//        cout << "test :" << ++i << endl;
        int stat;
        while(waitpid(cmd_1_pid, &stat, WUNTRACED) != cmd_1_pid || WIFSTOPPED(stat)){

        }

        while(waitpid(cmd_2_pid, &stat, WUNTRACED) != cmd_2_pid || WIFSTOPPED(stat)){

        }

    }
    else{
        if(bg){
            SmallShell::getInstance().jobsList.addJob(cmd_line_command, pid);
            waitpid(pid, nullptr, WNOHANG);
        }
        else{
            SmallShell::getInstance().fg_is_pgid = true;
            SmallShell::getInstance().current_fg_pid = pid;
            waitpid(pid, nullptr, WUNTRACED);
        }
    }
}




/*
 * RedirectCommand
 */

RedirectionCommand::RedirectionCommand(const char *cmd_line) : Command(cmd_line) {
    int redirection_loc = string(cmd_line).find('>');
    int command_loc = redirection_loc++;
    append = string(cmd_line).find(">>") != string::npos;
    if(append){
        ++redirection_loc;
    }

    isBackground = _isBackgroundComamnd(cmd_line_command);

    command = string(cmd_line).erase(command_loc);
    redirection = string(cmd_line).substr(redirection_loc);
    redirection = _trim(redirection);

    if(isBackground){
        redirection.replace(redirection.find('&'),'&', "");
    }

}

void RedirectionCommand::execute() {
    SmallShell& smash = SmallShell::getInstance();
    int _oflag;
    if(append){
        _oflag = O_WRONLY | O_APPEND | O_CREAT;
    }
    else{
        _oflag = O_WRONLY | O_TRUNC | O_CREAT;
    }
    int fd = open(redirection.c_str(), _oflag , S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1){
        return perror("smash error: open failed");
    }


    Command* cmnd = SmallShell::CreateCommand(command.c_str());

    if(typeid(*cmnd) == typeid(ExternalCommand) || typeid(*cmnd) == typeid(CopyCommand)){

        pid_t pid = fork();
        setpgrp();

        if(pid == 0){// child
            in_pipe = true;
            int std_out = dup(STDOUT_FILENO);

            if (std_out == -1){
                perror("smash error: dup failed");
                return;
            }

            if (dup2(fd, STDOUT_FILENO) == -1){
                perror("smash error: dup2 failed");
                return;
            }

            cmnd->execute();
            exit(EXIT_SUCCESS);


        }
        else{
            if(isBackground){
                waitpid(pid, nullptr, WNOHANG);
                SmallShell::getInstance().jobsList.addJob(cmd_line_command, pid);
            }
            else{
                SmallShell::getInstance().fg_is_pgid = true;
                SmallShell::getInstance().current_fg_pid = pid;
                waitpid(pid, nullptr,  WUNTRACED);
            }
            return;
        }

    }

    int std_out = dup(STDOUT_FILENO);

    if (std_out == -1){
        perror("smash error: dup failed");
        return;
    }

    if (dup2(fd, STDOUT_FILENO) == -1){
        perror("smash error: dup2 failed");
        return;
    }

    SmallShell::executeCommand(command.c_str());

    if(dup2(std_out, STDOUT_FILENO) == -1){
        perror("smash error: dup2 failed");
        return;
    }

    if(close(std_out) == -1){
        perror("smash error: close failed");
    }

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
    if(get_arg(1) == nullptr){
        return;
    }
    if(get_arg(2) != nullptr){
        write_with_error(string("smash error: cd: too many arguments\n"));
        return;
    }

    if(strcmp(get_arg(1),"-") == 0){
        if(lastPwd.empty()){
            write_with_error(string("smash error: cd: OLDPWD not set\n"));
            return;
        }
        string new_dir = lastPwd;
    }
    else {
        lastPwd = get_arg(1);
    }
    char* cdn = get_current_dir_name();

    if (cdn == nullptr){
        return perror("smash error: get_current_dir_name failed");
        return;
    }

    SmallShell::getInstance().changeLastPwd(cdn);

    if(chdir(lastPwd.c_str()) == -1){
        perror("smash error: chdir failed");
    }
    free(cdn);

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

    write_with_error(string(current_dir_name));
    write_with_error(string("\n"));
    free(current_dir_name);
}

/*
 * ShowPidCommand
 */

ShowPidCommand::ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute() {
    pid_t pid = SmallShell::getInstance().getPid();
    string message = "smash pid is ";
    message += std::to_string(pid) += "\n";
    write_with_error(message);
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



QuitCommand::QuitCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {
    for(int i = 0 ; get_arg(i) != nullptr ; ++i){
        if(string(get_arg(i)).find("kill") == 0){
            kill_arg = true;
            return;
        }
    }
    kill_arg = false;
}

void QuitCommand::execute() {
    if(!kill_arg){
        exit(EXIT_SUCCESS);
    }
    SmallShell& smash = SmallShell::getInstance();
    int num_of_jobs = smash.jobsList.numOfJobs();
    string message = "smash: sending SIGKILL signal to ";
    message += to_string(num_of_jobs);
    message += " jobs:\n";
    write_with_error(message);
    smash.jobsList.killAllJobs();
    exit(EXIT_SUCCESS);
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
        job += std::to_string(it.pid);
        job += " ";
        job += std::to_string(int(difftime(time(nullptr), it.time_added)));
        job += " secs";
        if(it.stopped){
            job += " (stopped)";
        }
        job += "\n";
        write_with_error(job);
    }
}

//jobslist is sorted if this method is called
int JobsList::findMaxJobId() const {
    return jobsList.back().job_id;
}

void JobsList::removeFinishedJobs() {
    auto it = jobsList.begin();
    while ( it != jobsList.end()){
        if(waitpid(it->pid, nullptr, WNOHANG) == it->pid){
            jobsList.erase(it++);
        }
        else{
            ++it;
        }
    }
}

JobsList::JobEntry * JobsList::getJobById(int jobId) {

    for (auto & it : jobsList){
        if(it.job_id == jobId){
            return &it;
        }
    }
    return nullptr;
}

JobsList::JobEntry* JobsList::getJobByPid(pid_t pid) {

    for (auto & it : jobsList){
        if(it.pid == pid){
            return &it;
        }
    }
    return nullptr;
}

JobsList::JobEntry * JobsList::getLastStoppedJob() {
    JobEntry* je = nullptr;
    for(auto & it : jobsList){
        if(it.stopped)
            je = &it;
    }
    return je;
}

void JobsList::removeJobById(int jobId) {
    for (auto& it : jobsList){
        if (it.job_id == jobId){
            jobsList.remove(it);
            return;
        }
    }
}

int JobsList::numOfJobs() const {
    return jobsList.size();
}

void JobsList::killAllJobs() {
    for (auto& it : jobsList){
        string message = to_string(it.pid);
        message += ": ";
        message += it.cmd;
        message += "\n";
        write_with_error(message);
        kill(it.pid, SIGKILL);
    }
}



void JobsCommand::execute() {
    SmallShell::getInstance().jobsList.printJobsList();
}



void KillCommand::execute() {
    string error = "smash error: kill: invalid arguments\n";
    if(num_of_args != 3){
        write_with_error(error);
        return;
    }
    if (string((get_arg(1))).find('-') != 0){
        write_with_error(error);
        return;
    }
    try{
        stoi(get_arg(1));
        stoi(get_arg(2));
    }
    catch (std::invalid_argument& e) {
        write_with_error(error);
        return;
    }
    catch (std::out_of_range& e) {
        write_with_error(error);
        return;
    }

    JobsList::JobEntry* jobEntry = SmallShell::getInstance().jobsList.getJobById(stoi(get_arg(2)));
    if (jobEntry == nullptr){
        error = "smash error: kill: job-id ";
        error += to_string(stoi(get_arg(2)));
        error += " does not exist\n";
        write_with_error(error);
        return;
    }
    //signals SIGINT and SIGCONT won'y be tested

    /*
     * no need to check according to @238_f2 in piazza
     */
//    if ((-1)*stoi(get_arg(1)) < 1 || (-1)*stoi(get_arg(1)) > 32){
//        write_with_error(string("smash error: kill: invalid arguments\n"));
//        return;
//    }
    if(kill(jobEntry->pid, ((-1)*stoi(get_arg(1)))) == -1){
        perror("smash error: kill failed");
    }

    string message = "signal number ";
    message += to_string(stoi(get_arg(1))*(-1)) += " was sent to pid ";
    message += to_string(jobEntry->pid) += "\n";

    write_with_error(message);

}



ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line) {}

void ExternalCommand::execute() {
    bool pipe_grp = (SmallShell::getInstance().getPid() == getpid());
    bool isBackground = _isBackgroundComamnd(cmd_line_command);
    string copy_cmd(cmd_line_command);
    if (isBackground){
        _removeBackgroundSign(cmd_line_command);
    }
    pid_t pid = fork();
    if (pid == 0) { //child
        const char **args_t = new const char*[4];
        args_t[0] = "/bin/bash";
        args_t[1] = "-c";
        args_t[2] = cmd_line_command;
        args_t[3] = nullptr;
        //changing the groupID of the forked process
        if(pipe_grp){
            setpgrp();
        }
        execv(args_t[0], const_cast<char**>(args_t));
    }
    else if (pid == -1){
        perror("smash error: fork failed");
    }
    else{
        if(in_pipe){
            int stat;
            while(waitpid(pid, &stat, WUNTRACED) != pid || WIFSTOPPED(stat)){

            }
        }
        else{
            if(isBackground){
                waitpid(pid, nullptr, WNOHANG);
                SmallShell::getInstance().jobsList.addJob(copy_cmd.c_str(), pid);
            }
            else{
                SmallShell::getInstance().fg_is_pgid = false;
                SmallShell::getInstance().current_fg_pid = pid;
                waitpid(pid, nullptr,  WUNTRACED);
            }
        }
    }
}



ForegroundCommand::ForegroundCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}


void ForegroundCommand::execute() {

    SmallShell& smash = SmallShell::getInstance();
    JobsList::JobEntry* jobEntry = nullptr;
    if(get_arg(1) == nullptr){
        jobEntry = smash.jobsList.getJobById(smash.jobsList.findMaxJobId());
        if (jobEntry == nullptr){
            string error = "smash error: fg: jobs list is empty\n";
            write_with_error(error);
            return;
        }
    }
    else{
        string error = "smash error: fg: invalid arguments";
        if (get_arg(2) != nullptr){
            write_with_error(error);
            return;
        }
        try{
            stoi(get_arg(1));
        }
        catch (std::invalid_argument& e) {
            write_with_error(error);
            return;
        }
        catch (std::out_of_range& e) {
            write_with_error(error);
            return;
        }


        jobEntry = smash.jobsList.getJobById(stoi(get_arg(1)));
        if(jobEntry == nullptr){
            error = "smash error: fg: job-id ";
            error += to_string(stoi(get_arg(1)));
            error +=" does not exist\n";
            write_with_error(error)
            return;
        }
    }

    string fg_message = jobEntry->cmd;
    fg_message += " : ";
    fg_message += to_string((jobEntry->pid));
    fg_message += "\n";

    write_with_error(fg_message);
    smash.current_fg_pid = jobEntry->pid;
    jobEntry->stopped = false;

    smash.fg_is_pgid = (jobEntry->cmd.find('|') != string::npos || jobEntry->cmd.find('>') != string::npos);
    smash.fg_is_pgid ?  killpg(jobEntry->pid, SIGCONT) : kill(jobEntry->pid, SIGCONT);
    int stat;
    if(waitpid(jobEntry->pid, &stat, WUNTRACED) == jobEntry->pid){
        if(!WIFSTOPPED(stat))
            smash.jobsList.removeJobById(jobEntry->job_id);
    }

}


void BackgroundCommand::execute() {

    SmallShell& smash = SmallShell::getInstance();
    JobsList::JobEntry* jobEntry = nullptr;

    if(get_arg(1) == nullptr){
        jobEntry = smash.jobsList.getLastStoppedJob();
        if (jobEntry == nullptr){
            string error = "smash error: bg: there is no stopped jobs to resume\n";
            write_with_error(error);
            return;
        }
    }

    else{
        string error = "smash error: bg: invalid arguments\n";
        if (get_arg(2) != nullptr){
            write_with_error(error);
            return;
        }
        try{
            stoi(get_arg(1));
        }
        catch (std::invalid_argument& e) {
            write_with_error(error);
            return;
        }
        catch (std::out_of_range& e) {
            write_with_error(error);
            return;
        }


        jobEntry = smash.jobsList.getJobById(stoi(get_arg(1)));
        if(jobEntry == nullptr){
            error = "smash error: bg: job-id ";
            error += to_string(stoi(get_arg(1)));
            error +=" does not exist\n";
            write_with_error(error);
            return;
        }

        if (!(jobEntry->stopped)){
            error = "smash error: bg: job-id ";
            error += to_string(stoi(get_arg(1)));
            error += " is already running in the background\n";
            write_with_error(error);
            return;
        }
    }

    jobEntry->stopped = false;

    bool command_is_pipe = (jobEntry->cmd.find('|') != string::npos ||  jobEntry->cmd.find('>') != string::npos);
    command_is_pipe ?  killpg(jobEntry->pid, SIGCONT) : kill(jobEntry->pid, SIGCONT);

    string message(jobEntry->cmd);
    message += " : ";
    message += to_string(jobEntry->pid);
    message += "\n";
    write_with_error(message);

    if(waitpid(jobEntry->pid, nullptr, WNOHANG) == jobEntry->pid){
        smash.jobsList.removeJobById(jobEntry->job_id);
    }
}


CopyCommand::CopyCommand(const char *cmd_line) : ExternalCommand(cmd_line) {
    bg_command = _isBackgroundComamnd(cmd_line);
    string command(cmd_line);
    input_file = get_arg(1);
    output_file = get_arg(2);
    if(bg_command && output_file.find('&')){
        output_file.replace(output_file.find('&'), 1, "");
    }

}


bool CopyCommand::samePathCheck() const {

    char* real_path_input = realpath(input_file.c_str(), nullptr);

    if (real_path_input == nullptr){
        perror("smash error: realpath failed");
    }

    char* real_path_output = realpath(output_file.c_str(), nullptr);

    if (real_path_output == nullptr){
        perror("smash error: realpath failed");
    }

    if (strcmp(real_path_input, real_path_output) == 0){
        free (real_path_input);
        free (real_path_output);
        return true;
    }

    free (real_path_input);
    free (real_path_output);
    return false;
}


void CopyCommand::execute() {

    pid_t pid = fork();
    if(pid == 0){ // cp process

        setpgrp();
        int fd_input = open(input_file.c_str(), O_RDONLY);
        if (fd_input == -1){
            perror("smash error: open failed");
            return;
        }
        int fd_output = open(output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd_output == -1){
            perror("smash error: open failed");
            close(fd_input);
            return;
        }
        if (samePathCheck()){
            string message = "smash: ";
            message += string(input_file) += " was copied to ";
            message += string(output_file) += "\n";
            write_with_error(message);
            return;
        }
        char* buff[1024];
        int stat = 1;
        while(stat){
            stat = read(fd_input, buff, 1024);
            if(stat == -1){
                perror("smash error: read failed");
                close(fd_input);
                close(fd_output);
                return;
            }
            if(write(fd_output, buff, stat) != stat){
                perror("smash error: read failed");
                close(fd_input);
                close(fd_output);
                return;
            }
        }
        close(fd_input);
        close(fd_output);
        string message = "smash: ";
        message += string(input_file) += " was copied to ";
        message += string(output_file) += "\n";
        write_with_error(message);
    }
    else{
        if(in_pipe){
            int stat;
            while(waitpid(pid, &stat, WUNTRACED) != pid || WIFSTOPPED(stat)){

            }
        }
        else{
            if(_isBackgroundComamnd(cmd_line_command)){
                SmallShell::getInstance().jobsList.addJob(cmd_line_command, pid);
                waitpid(pid, nullptr, WNOHANG);
            }
            else{
                SmallShell::getInstance().fg_is_pgid = true;
                SmallShell::getInstance().current_fg_pid = pid;
                waitpid(pid, nullptr, WUNTRACED);
            }
        }
    }
}


/*
TimeOut::TimeEntry::TimeEntry(const string &cmd_line_command_in, int duration_in, pid_t pid_in) {
    cmd_line_command = cmd_line_command_in;
    pid = pid_in;
    duration = duration_in;
    timestamp = time(nullptr);
}

bool TimeOut::TimeEntry::operator==(const TimeEntry &timeEntry) const {
    return (this->pid == timeEntry.pid);
}

bool TimeOut::TimeEntry::operator<(const TimeEntry &timeEntry) const {
    return (((this->timestamp)+(this->duration)) < ((timeEntry.timestamp)+timeEntry.duration));
}

TimeOut::TimeOut() = default;

//duration should be positive and pid should be legal
void TimeOut::addItem(const string &cmd_line, int duration, pid_t pid) {
    TimeEntry timeEntry(cmd_line, duration, pid);
    timeList.push_back(timeEntry);
    timeList.sort();

    time_t time_till_next_sig = timeList.front().duration-(time(nullptr)-timeList.front().timestamp);
    if (time_till_next_sig > duration){
        alarm(duration);
    }

}

TimeOut::TimeEntry * TimeOut::firstTimeEntry() {
    if(timeList.empty()){
        return nullptr;
    }
    return &timeList.front();
}

void TimeOut::removeFirstTimeEntry() {
    timeList.pop_front();
}

TimeOutCommand::TimeOutCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {
    cmd_line | timeout X <command> -> CreateCommand -> BuiltInCommand->pid external -> fork(Exectute->)
}
*/




SmallShell::SmallShell() : prompt(def_prompt), last_pwd(""), curr_fd(STDOUT_FILENO), pid(getpid()),jobsList() ,
                           current_fg_pid(getpid()), cmd_line_fg(""), fg_is_pgid(false){

}

SmallShell::~SmallShell() {
// TODO: add your implementation
}

Command *SmallShell::CreateCommand(const char *cmd_line) {
    // For example:

    string cmd_s = string(cmd_line);
    if (cmd_s.find('|') != string::npos){
        return new PipeCommand(cmd_line);
    }

    if (cmd_s.find('>') != string::npos) {
        return new RedirectionCommand(cmd_line);
    }

    if (cmd_s.find("quit") == 0) {
        return new QuitCommand(cmd_line);
    }


    if (cmd_s.find("jobs") == 0) {
        return new JobsCommand(cmd_line);
    }

    if (cmd_s.find("fg") == 0){
        return new ForegroundCommand(cmd_line);
    }

    if (cmd_s.find("bg") == 0){
        return new BackgroundCommand(cmd_line);
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

    if (cmd_s.find("kill") == 0) {
        return new KillCommand(cmd_line);
    }

    if (cmd_s.find("cp") == 0){
        return new CopyCommand(cmd_line);
    }

    return new ExternalCommand(cmd_line);
}

void SmallShell::executeCommand(const char *cmd_line) {
    // TODO: Add your implementation here
    // for example:
    Command *cmd = CreateCommand(cmd_line);
    if (cmd == nullptr){
        return;
    }
    SmallShell::getInstance().cmd_line_fg = cmd_line;
    cmd->execute();
    delete cmd;
    SmallShell::getInstance().current_fg_pid = SmallShell::getInstance().getPid();
    if(getpid() != SmallShell::getInstance().getPid()){
        exit(0);
    }
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

