#pragma once
#include <iostream>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <stack>
#include <algorithm>
using namespace std;

class ResourceManager {
private:
    int num_threads;
    int num_resources;

    vector<int> assignedTo;
    vector<int> waitingFor;
    vector<bool> killed;
    vector<int> kill_count;

    mutex mtx;
    condition_variable cv;

    void tarjanDFS(int u, vector<int>& adj, vector<int>& ids, 
                   vector<int>& low, stack<int>& st, 
                   vector<bool>& onStack, int& timer, 
                   vector<vector<int>>& sccs) {
        ids[u] = low[u] = ++timer;
        st.push(u);
        onStack[u] = true;

        int v = adj[u];
        if (v != -1) {
            if (ids[v] == -1) {
                tarjanDFS(v, adj, ids, low, st, onStack, timer, sccs);
                low[u] = min(low[u], low[v]);
            } else if (onStack[v]) {
                low[u] = min(low[u], ids[v]);
            }
        }

        if (low[u] == ids[u]) {
            vector<int> current_scc;
            while (true) {
                int node = st.top();
                st.pop();
                onStack[node] = false;
                current_scc.push_back(node);
                if (node == u) break;
            }
            if (current_scc.size() > 1) {
                sccs.push_back(current_scc);
            }
        }
    }

public:
    ResourceManager(int n, int m) : num_threads(n), num_resources(m) {
        assignedTo.assign(num_resources, -1);
        waitingFor.assign(num_threads, -1);
        killed.assign(num_threads, false);
        kill_count.assign(num_threads, 0);
    }

    bool lock(int t_id, int r_id) {
        unique_lock<mutex> lock(mtx);
        
        while (assignedTo[r_id] != -1 && assignedTo[r_id] != t_id && !killed[t_id]) {
            waitingFor[t_id] = r_id;
            cv.wait(lock);
        }

        if (killed[t_id]) {
            waitingFor[t_id] = -1;
            return false;
        }

        waitingFor[t_id] = -1;
        assignedTo[r_id] = t_id;
        return true;
    }

    void unlock(int t_id, int r_id) {
        lock_guard<mutex> lock(mtx);
        if (assignedTo[r_id] == t_id) {
            assignedTo[r_id] = -1;
            cv.notify_all();
        }
    }

    vector<vector<int>> detectDeadlocks() {
        lock_guard<mutex> lock(mtx);

        vector<int> adj(num_threads, -1);
        for (int i = 0; i < num_threads; ++i) {
            if (waitingFor[i] != -1 && assignedTo[waitingFor[i]] != -1) {
                adj[i] = assignedTo[waitingFor[i]]; 
            }
        }

        vector<int> ids(num_threads, -1);
        vector<int> low(num_threads, -1);
        vector<bool> onStack(num_threads, false);
        stack<int> st;
        int timer = 0;
        vector<vector<int>> deadlocks;

        for (int i = 0; i < num_threads; ++i) {
            if (ids[i] == -1 && !killed[i]) {
                tarjanDFS(i, adj, ids, low, st, onStack, timer, deadlocks);
            }
        }
        return deadlocks;
    }

    void killAndRecover(int victim_t_id) {
        lock_guard<mutex> lock(mtx);
        killed[victim_t_id] = true;
        kill_count[victim_t_id]++;
        
        for (int i = 0; i < num_resources; ++i) {
            if (assignedTo[i] == victim_t_id) {
                assignedTo[i] = -1;
            }
        }
        cv.notify_all(); 
    }

    int getKillCount(int t_id) {
        return kill_count[t_id];
    }
    
    void reviveThread(int t_id) {
        lock_guard<mutex> lock(mtx);
        killed[t_id] = false;
        waitingFor[t_id] = -1;
    }
};