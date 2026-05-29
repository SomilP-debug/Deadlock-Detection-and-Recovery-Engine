#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <algorithm>
#include "ResourceManager.hpp"
using namespace std;

enum class Action { LOCK, UNLOCK, SLEEP };

struct Command {
    Action type;
    int value;
};

bool system_running = true;
map<int, vector<Command>> workloads;
int max_thread_id = 0;
int max_resource_id = 0;

void parseWorkload(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Could not open " << filename << "\n";
        exit(1);
    }

    string line;
    while (getline(file, line)) {
        if (line.empty()) continue;

        size_t colon_pos = line.find(':');
        if (colon_pos == string::npos) continue;

        string thread_part = line.substr(0, colon_pos);
        int t_id = stoi(thread_part.substr(thread_part.find(' ') + 1));
        max_thread_id = max(max_thread_id, t_id);

        string commands_part = line.substr(colon_pos + 1);
        stringstream ss(commands_part);
        string token;

        while (getline(ss, token, ',')) {
            token.erase(0, token.find_first_not_of(" \t"));
            stringstream token_ss(token);
            string action_str;
            int val;
            
            token_ss >> action_str >> val;

            Command cmd;
            cmd.value = val;
            
            if (action_str == "LOCK") {
                cmd.type = Action::LOCK;
                max_resource_id = max(max_resource_id, val);
            } else if (action_str == "UNLOCK") {
                cmd.type = Action::UNLOCK;
            } else if (action_str == "SLEEP") {
                cmd.type = Action::SLEEP;
            }

            workloads[t_id].push_back(cmd);
        }
    }
}

void workerProcess(int t_id, ResourceManager* rm, const vector<Command>& script) {
    bool task_completed = false;

    while (system_running && !task_completed) {
        rm->reviveThread(t_id);
        bool killed_in_action = false;

        cout << "[Thread " << t_id << "] Starting workload (Kill-Count : " << rm->getKillCount(t_id) << ")...\n";

        for (const auto& cmd : script) {
            if (!system_running) break;

            if (cmd.type == Action::LOCK) {
                if (!rm->lock(t_id, cmd.value)) {
                    killed_in_action = true;
                    break;
                }
            } else if (cmd.type == Action::UNLOCK) {
                rm->unlock(t_id, cmd.value);
            } else if (cmd.type == Action::SLEEP) {
                this_thread::sleep_for(chrono::milliseconds(cmd.value));
            }
        }

        if (killed_in_action) {
            cout << ">>> [Thread " << t_id << "] Workload aborted! Retrying in 100ms...\n";
            this_thread::sleep_for(chrono::milliseconds(100));
        } else {
            task_completed = true;
            cout << "<<< [Thread " << t_id << "] SUCCESS! Workload fully committed.\n";
        }
    }
}

void watchdogDaemon(ResourceManager* rm) {
    while (system_running) {
        this_thread::sleep_for(chrono::milliseconds(500));
        
        auto deadlocks = rm->detectDeadlocks();
        
        for (const auto& cycle : deadlocks) {
        
            cout << "[WATCHDOG] DEADLOCK DETECTED! Threads involved: ";
            for (int t : cycle) cout << t << " ";
            cout << "\n";

            int victim = cycle[0];
            for (int t : cycle) {
                if (rm->getKillCount(t) < rm->getKillCount(victim)) {
                    victim = t;
                } else if (rm->getKillCount(t) == rm->getKillCount(victim)) {
                    if (t > victim) {
                        victim = t; 
                    }
                }
            }
            
            cout << "[REAPER] Terminating Thread " << victim << " to break cycle.\n";
          
            
            rm->killAndRecover(victim);
        }
    }
}

int main() {
    cout << "Starting Config-Driven OS Simulator...\n";

    parseWorkload("workload.txt");
    
    int num_threads = max_thread_id + 1;
    int num_resources = max_resource_id + 1;
    
    cout << "Loaded config: " << num_threads << " Threads, " << num_resources << " Resources.\n\n";

    ResourceManager rm(num_threads, num_resources);
    vector<thread> threads;

    for (const auto& [t_id, script] : workloads) {
        threads.emplace_back(workerProcess, t_id, &rm, script);
    }

    thread watchdog(watchdogDaemon, &rm);

    this_thread::sleep_for(chrono::seconds(5));
    
    system_running = false;
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
    if (watchdog.joinable()) watchdog.join();

    cout << "Simulation Shutdown Complete.\n";
    return 0;
}
