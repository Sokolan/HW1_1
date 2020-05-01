#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"



int main(int argc, char* argv[]) {
    static SmallShell& smash = SmallShell::getInstance();

    if(signal(SIGTSTP , ctrlZHandler)==SIG_ERR) {
        perror("smash error: failed to set ctrl-Z handler");
    }
    if(signal(SIGINT , ctrlCHandler)==SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
    }



//    struct sigaction sa;
//    sa.sa_handler = alarmHandler;
//    sigemptyset(&sa.sa_mask);
//    sa.sa_flags = SA_RESTART;
//
//    if(sigaction(SIGALRM , &sa, nullptr) == -1) {
//        perror("smash error: failed to set SIGALRM handler");
//    }




    //TODO: setup sig alarm handler
    while(true) {
        std::cout << smash.getPrompt() << "> "; // TODO: change this (why?)
        std::string cmd_line;
        std::getline(std::cin, cmd_line);
        smash.jobsList.removeFinishedJobs();
        smash.executeCommand(cmd_line.c_str());
    }
    return 0;
}