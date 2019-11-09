#include "graph.hpp"
#include "upcxx/upcxx.hpp"
#include <chrono>
#include <ctime> 
#include <climits>
#include <stdlib.h> 
#include <time.h>
#include "sequence.hpp"

using namespace upcxx;

struct nonNegF{bool operator() (VertexId a) {return (a>=0);}};

void sync_round_dense(Graph& g, int* dist_next, bool* frontier_next) {  
    for (int i = 0; i < rank_n(); i++) {
        broadcast(dist_next+g.rank_start_node(i), g.rank_num_nodes(i), i).wait();
        broadcast(frontier_next+g.rank_start_node(i), g.rank_num_nodes(i), i).wait();
    }
    barrier();
}

VertexId bfs_dense(Graph& g, global_ptr<int> dist_dist, global_ptr<int> dist_next_dist, global_ptr<bool> frontier_dist, global_ptr<bool> frontier_next_dist, int level) {
    int* dist = dist_dist.local();
    int* dist_next = dist_next_dist.local();
    bool* frontier = frontier_dist.local();
    bool* frontier_next = frontier_next_dist.local();

    auto time_1 = std::chrono::system_clock::now();
    
    for (VertexId u = g.rank_start; u < g.rank_end; u++) {
        // update next round of dist
        dist_next[u] = dist[u];
        frontier_next[u] = false;

        // ignore if distance is set already
        if (dist_next[u] != INT_MAX) continue;
        VertexId* neighbors = g.in_neighbors(u).local(); 

        for (EdgeId j = 0; j < g.in_degree(u); j++) {
            VertexId v = neighbors[j];

            if (!frontier[v]) continue;

            dist_next[u] = level;
            frontier_next[u] = true;
        }
    }
    barrier();
    auto time_2 = std::chrono::system_clock::now();
    std::chrono::duration<double> delta_time = time_2 - time_1;
    if (rank_me()==0) cout << "Compute time " << delta_time.count() << endl;
    sync_round_dense(g, dist_next, frontier_next);
    auto time_3 = std::chrono::system_clock::now();
    delta_time = time_3 - time_2;
    if (rank_me()==0) cout << "Sync time " << delta_time.count() << endl;
    VertexId frontier_size = sequence::sumFlagsSerial(frontier_next, g.num_nodes);
    return frontier_size;
}
/*
void bfs_sparse(Graph& g, dist_object<global_ptr<int>>& dist, dist_object<global_ptr<bool>>& frontier, dist_object<global_ptr<bool>>& next_frontier, int level) {
    auto dist_local = dist->local();
    auto frontier_local = frontier->local();
    auto next_frontier_local = next_frontier->local();

    global_ptr<bool> root_next_frontier = next_frontier.fetch(0).wait();
    global_ptr<int> root_dist = dist.fetch(0).wait();

    future<> fut_all = make_future();
    for (int n = g.rank_start; n < g.rank_end; n++) {
        // skip any that's not in frontier
        if (!frontier_local[n]) continue;
        for (int v : g.out_neighbors(n)) {
            if (dist_local[v] == INT_MAX) {
                fut_all = when_all(fut_all, rput(level, root_dist+v));
                fut_all = when_all(fut_all, rput(true, root_next_frontier+v));
            }
        }
    }
    barrier();
    fut_all.wait();
    upcxx::broadcast(next_frontier_local, g.num_nodes, 0).wait();
    upcxx::broadcast(dist_local, g.num_nodes, 0).wait();
    barrier();
    
    
}
*/
void sparse_to_dense(VertexId* frontier_sparse, VertexId frontier_size, bool* frontier_dense) {
    for (VertexId i = 0; i < frontier_size; i++) {
        frontier_dense[frontier_sparse[i]] = true;
    }
}

void dense_to_sparse(bool* frontier_dense, VertexId num_nodes, VertexId* frontier_sparse) {
    for (int i = 0; i < num_nodes; i++) {
        if (frontier_dense[i]) {
            frontier_sparse[i] = i;
        } else {
            frontier_sparse[i] = -1;
        }
    }
    sequence::filter(frontier_sparse, frontier_sparse, num_nodes, nonNegF());
}

int* bfs(Graph &g, VertexId root) {
    // https://github.com/sbeamer/gapbs/blob/master/src/pr.cc
    global_ptr<int> dist_dist = new_array<int>(g.num_nodes); int* dist = dist_dist.local();
    global_ptr<int> dist_next_dist = new_array<int>(g.num_nodes); int* dist_next = dist_next_dist.local();

    global_ptr<int> frontier_sparse_dist = new_array<int>(g.num_nodes); int* frontier_sparse = frontier_sparse_dist.local();
    global_ptr<int> frontier_sparse_next_dist = new_array<int>(g.num_nodes); int* frontier_sparse_next = frontier_sparse_next_dist.local();

    global_ptr<bool> frontier_dense_dist = new_array<bool>(g.num_nodes); bool* frontier_dense = frontier_dense_dist.local();
    global_ptr<bool> frontier_dense_next_dist = new_array<bool>(g.num_nodes); bool* frontier_dense_next = frontier_dense_next_dist.local();

    for (int i = 0; i < g.num_nodes; i++) {
        dist[i] = INT_MAX;
        dist_next[i] = INT_MAX;
    }

    bool is_sparse_mode = true;

    frontier_sparse[0] = root;
    VertexId frontier_size = 1;
    dist[root] = 0; // initialize everyone to INF except root
    dist_next[root] = 0;

    int level = 0;
    const int threshold_fraction_denom = 20;

    while (frontier_size != 0) {
        level++; 
        bool should_be_sparse_mode = false; // frontier_size < (g.num_nodes / threshold_fraction_denom);

        if (DEBUG && rank_me() == 0) cout << "Round " << level << " | " << "Frontier: " << frontier_size << " | Sparse? " << should_be_sparse_mode << endl;
        auto time_before = chrono::system_clock::now();

        if (should_be_sparse_mode) {
            if (!is_sparse_mode) {
                dense_to_sparse(frontier_dense, g.num_nodes, frontier_sparse);
            }
            is_sparse_mode = true;
            // TODO: WORRY ABOUT THIS LATER
            // frontier_size = bfs_sparse(g, dist, dist_next, )
        } else {
            if (is_sparse_mode) {
                sparse_to_dense(frontier_sparse, frontier_size, frontier_dense);
            }
            is_sparse_mode = false;
            frontier_size = bfs_dense(g, dist_dist, dist_next_dist, frontier_dense_dist, frontier_dense_next_dist, level);

            swap(frontier_dense_next_dist, frontier_dense_dist);
            swap(frontier_dense_next, frontier_dense);
        }
        swap(dist_next_dist, dist_dist);
        swap(dist_next, dist);

        barrier();
        auto time_after = chrono::system_clock::now();
        chrono::duration<double> delta = (time_after - time_before);
        if (DEBUG && rank_me() == 0) cout << "Time: " << delta.count() << endl;
    }

    delete_array(dist_next_dist); delete_array(frontier_sparse_dist); delete_array(frontier_sparse_next_dist); delete_array(frontier_dense_dist); delete_array(frontier_dense_next_dist);

    return dist; 
}

/*
bool verify(Graph& g, int root, vector<int> dist_in) {
    vector<int> dist(g.num_nodes);

    if (dist.size() != dist_in.size())
        return false;

    for (int i = 0; i < g.num_nodes; i++)
        dist[i] = INT_MAX; // set INF

    dist[root] = 0;
    int level = 1;
    vector<int> frontier, next_frontier;
    frontier.push_back(root);
    while (frontier.size() != 0) {
        for (int i = 0; i < frontier.size(); i++) {
            int u = frontier[i];
            vector<int> neighbors = g.out_neighbors(u);
            for (int j = 0; j < g.out_degree(u); j++) {
                int v = neighbors[j];
                if (dist[v] == INT_MAX) {
                    next_frontier.push_back(v);
                    dist[v] = level;
                }
            }
        }
        frontier = next_frontier; 
        next_frontier.clear();
        level += 1; 
    }

    for (int i = 0; i < dist.size(); i++) {
        if (dist[i] != dist_in[i]) {
            return false;
        }
    }
    return true;
}*/

int main(int argc, char *argv[]) {
    if (argc != 3) {
        cout << "Usage: ./bfs <path_to_graph> <num_iters>" << endl;
        exit(-1);
    }

    init();

    Graph g = Graph(argv[1]);
    int num_iters = atoi(argv[2]);

    barrier(); 
    srand(time(NULL));
    float current_time = 0.0;
    for (int i = 0; i < num_iters; i++) {
        dist_object<int> root(rand() % g.num_nodes);
        int root_local = 0; //root.fetch(0).wait();
        auto time_before = std::chrono::system_clock::now();
        int* dist = bfs(g, root_local);
        /*if (rank_me() == 0) {
            for (int i = 0; i < g.num_nodes; i++)
                cout << dist[i] << endl;
        }*/
        auto time_after = std::chrono::system_clock::now();
        std::chrono::duration<double> delta_time = time_after - time_before;
        current_time += delta_time.count();
    }

    if (rank_me() == 0) {
        std::cout << current_time / num_iters << std::endl;
        /* if (!verify(g, root, dist)) {
            std::cerr << "Verification not correct" << endl;
        } */
    }
    barrier();
    finalize();
}