#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <utility>
#include <vector>
#include <list>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define HISTORY_MAX_RECORDS (50)
using namespace std;


class Command {

 public:
    char** args;
    int num_of_args;

    char* cmd_line_command;
    explicit Command(const char* cmd_line);
    char* get_arg(int i) const;
    virtual ~Command();
    virtual void execute() = 0;
    //virtual void prepare();
    //virtual void cleanup();
};

class BuiltInCommand : public Command {
 public:
    explicit BuiltInCommand(const char* cmd_line);
    virtual ~BuiltInCommand() {};
};

class ExternalCommand : public Command {
 public:
  ExternalCommand(const char* cmd_line);
    virtual ~ExternalCommand() {}
    void execute() override;
};

class PipeCommand : public Command {
    string command1;
    string command2;
    bool bg;
    bool std_err;
 public:
    explicit PipeCommand(const char* cmd_line);
    ~PipeCommand() override = default;
    void execute() override;
};

class RedirectionCommand : public Command {
    string redirection;
    string command;
    bool append;
    bool isBackground;
 public:
    explicit RedirectionCommand(const char* cmd_line);
    virtual ~RedirectionCommand() {}
    void execute() override;
    //void prepare() override;
    //void cleanup() override;
};

class ChangeDirCommand : public BuiltInCommand {
// TODO: Add your data members
    string lastPwd;
public:
    explicit ChangeDirCommand(const char* cmd_line, string plastPwd);
    virtual ~ChangeDirCommand() {}
    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
 public:
    GetCurrDirCommand(const char* cmd_line);
    virtual ~GetCurrDirCommand() {}
    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
 public:
    ShowPidCommand(const char* cmd_line);
    virtual ~ShowPidCommand() {}
    void execute() override;
};

class JobsList;

//class TimeOut;

class QuitCommand : public BuiltInCommand {
    bool kill_arg;
public:
    explicit QuitCommand(const char* cmd_line);
    virtual ~QuitCommand() {}
    void execute() override;
};


class JobsList {
public:
  class JobEntry {
  public:
      pid_t pid;
      int job_id;
      time_t time_added;
      bool stopped;
      string cmd;

      explicit JobEntry(pid_t pid_in, int job_id_in, time_t time_added_in, bool stopped_in, string  cmd_in) : pid(pid_in),
                                    stopped(stopped_in), job_id(job_id_in), time_added(time_added_in), cmd(std::move(cmd_in)) {};
      bool operator<(JobEntry& jobEntry) const {return job_id < jobEntry.job_id;}
      bool operator==(const JobEntry& jobEntry) const{
          return (this->pid == jobEntry.pid);
      }
  };
private:
    list<JobEntry> jobsList;
public:
    JobsList();
    ~JobsList() = default;;
    void addJob(const char* cmd, pid_t pid,bool isStopped = false);
    void printJobsList();
    void killAllJobs();
    void removeFinishedJobs();
    JobEntry * getJobById(int jobId);
    JobEntry* getJobByPid(pid_t pid);
    void removeJobById(int jobId);
//    JobEntry * getLastJob(int* lastJobId);
    JobEntry *getLastStoppedJob();
    int findMaxJobId() const;
    int numOfJobs() const;
};

class JobsCommand : public BuiltInCommand {
 // TODO: Add your data members
 public:
    explicit JobsCommand(const char* cmd_line) : BuiltInCommand(cmd_line){};
    virtual ~JobsCommand() {}
    void execute() override;
};

class KillCommand : public BuiltInCommand {
 // TODO: Add your data members
 public:
    explicit KillCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
    virtual ~KillCommand() {}
    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
 // TODO: Add your data members
 public:
    explicit ForegroundCommand(const char* cmd_line);
    virtual ~ForegroundCommand() {}
    void execute() override;
};

class BackgroundCommand : public BuiltInCommand {

 public:
    explicit BackgroundCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
    virtual ~BackgroundCommand() {}
    void execute() override;
};
/*
class TimeOutCommand : public BuiltInCommand{
    string command;
    time_t duration;

public:
    explicit TimeOutCommand(const char* cmd_line);
    virtual ~TimeOutCommand() {}
    void execute() override;
};
*/

class CopyCommand : public ExternalCommand {
    string input_file;
    string output_file;
    bool bg_command;
    bool samePathCheck() const;
 public:
    CopyCommand(const char* cmd_line);
    virtual ~CopyCommand() {}
    void execute() override;
};

class ChangePrompt : public BuiltInCommand {
public:
    ChangePrompt(const char* cmd_line);
    virtual ~ChangePrompt() {}
    void execute() override;
};

/*
 TODO: fix TimeOut
class TimeOut{
public:
    class TimeEntry{
    public:
        string cmd_line_command;
        time_t timestamp;
        int duration;
        pid_t pid;

        TimeEntry(const string& cmd_line_command_in, int duration_in, pid_t pid_in);
        bool operator==(const TimeEntry& timeEntry) const;
        bool operator<(const TimeEntry& timeEntry) const;
    };
private:
    list<TimeEntry> timeList;
public:
    TimeOut();
    void addItem(const string& cmd_line, int duration, pid_t pid);
    TimeEntry* firstTimeEntry();
    void removeFirstTimeEntry();
};
 */

class SmallShell {

private:
    // TODO: Add your data members
    SmallShell();

    const string def_prompt = "smash";
    string prompt;
    string last_pwd;
    int curr_fd;
    pid_t pid;
    const char* cmd_line_fg;
public:

//    TimeOut timeOut;
//    bool fg_is_redirect;

    bool fg_is_pgid;
    pid_t current_fg_pid;
    JobsList jobsList;


    static Command *CreateCommand(const char *cmd_line);

    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    ~SmallShell();

    static void executeCommand(const char *cmd_line);

    void changePrompt(const string& new_prompt);

    void changeCurrFd(int new_fd);

    int getCurrFd() const;

    void changeLastPwd(string new_pwd);

    string getLastPwd() const;

    string getPrompt() const;

    pid_t getPid() const;

    const char* getCmdLine() const;

    void addJob(const char* cmd, pid_t pidIn, bool isStopped);
};




#endif //SMASH_COMMAND_H_
