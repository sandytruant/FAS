#include "PageRankFAS.h"
#include <boost/graph/copy.hpp>
#include <boost/graph/graph_utility.hpp>
#include <boost/graph/reverse_graph.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <chrono>
#define SEED 42

const LineVertex INVALID_VERTEX = std::numeric_limits<LineVertex>::max();

bool dfs_cycle(const Graph &g, Vertex v, boost::container::vector<bool> &visited, boost::container::vector<bool> &on_stack) {
    visited[v] = true;
    on_stack[v] = true;

    for (const auto &ei : boost::make_iterator_range(boost::out_edges(v, g))) {
        const auto &u = boost::target(ei, g);
        if (!visited[u]) {
            if (dfs_cycle(g, u, visited, on_stack))
                return true;
        } else if (on_stack[u]) {
            return true;
        }
    }

    on_stack[v] = false;
    return false;
}

bool PageRankFAS::isCyclic(Graph &g) {
    boost::container::vector<bool> visited(boost::num_vertices(g), false);
    boost::container::vector<bool> on_stack(boost::num_vertices(g), false);
    for (const auto &v : boost::make_iterator_range(boost::vertices(g))) {
        if (!visited[v]) {
            if (dfs_cycle(g, v, visited, on_stack))
                return true;
        }
    }
    return false;
}

boost::container::vector<EdgePair> PageRankFAS::getFeedbackArcSet(Graph &g) {
    boost::container::vector<EdgePair> feedback_arcs;
    // 提前预留feedback_arcs空间
    feedback_arcs.reserve(boost::num_edges(g) / 4);
    srand(SEED);
    SPDLOG_INFO("Starting PageRankFAS...");

    int count = 0;
    bool flag = boost::num_vertices(g) > 10000;
    auto start = std::chrono::steady_clock::now();
    int duration = 0;

    while (isCyclic(g)) {
        // 打印图的节点数
        auto end = std::chrono::steady_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        if (!flag) {
            SPDLOG_INFO("Graph is cyclic. Current FAS number: {}, Time Elapsed: {} s, Computing FAS...",
                        feedback_arcs.size(), duration / 1000.0);
        } else {
            if (count % 10 == 0) {
                SPDLOG_INFO("Graph is cyclic. Current FAS number: {}, Time Elapsed: {} s, Computing FAS...",
                            feedback_arcs.size(), duration / 1000.0);
            }
        }
        // 求图G的强连通分量
        boost::container::vector<emhash8::HashSet<Vertex>> sccs;
        computeStronglyConnectedComponents(g, sccs);        

        // 为每个强连通分量创建线图
        for (auto &scc: sccs) {
            // 只有一个节点，直接跳过
            if (scc.size() == 1) {
                continue;
            }
            EdgeToVertexMap edge_to_vertex_map;
            VertexToEdgeMap vertex_to_edge_map;
            LineGraph lg;
            // 计算强连通分量的线图
            boost::container::vector<bool> visited(boost::num_vertices(g), false);
            // 创建线图的顶点集
            for (const auto &v: scc) {
                for (const auto &ei: boost::make_iterator_range(boost::out_edges(v, g))) {
                    const auto &u = boost::target(ei, g);
                    if (scc.count(u)) {
                        const auto &e = std::make_pair(g[v], g[u]);
                        const auto &z = boost::add_vertex(e, lg);
                        edge_to_vertex_map[e] = z;
                        vertex_to_edge_map[z] = ei;
                    }
                }
            }

            // 随机选取scc中的一个节点作为起点
            // auto s1 = std::chrono::steady_clock::now();
            const auto &v = *std::next(scc.begin(), rand() % scc.size());
            getLineGraph(g, lg, v, INVALID_VERTEX, visited, scc, edge_to_vertex_map);
            // auto s2 = std::chrono::steady_clock::now();
            // auto d = std::chrono::duration_cast<std::chrono::milliseconds>(s2 - s1);
            // SPDLOG_INFO("Lg Time Elapsed: {} ms", d.count());

            // 计算线图的PageRank值
            // auto t1 = std::chrono::steady_clock::now();
            boost::container::vector<float> pagerank;
            computePageRank(lg, pagerank);
            // auto t2 = std::chrono::steady_clock::now();
            // auto d2 = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
            // SPDLOG_INFO("pr Time Elapsed: {} ms", d2.count());
            
            // 选取PageRank值最大的边
            const auto &max_pagerank = std::max_element(pagerank.begin(), pagerank.end());
            const auto &max_pagerank_index = std::distance(pagerank.begin(), max_pagerank);
            const auto &max_pagerank_edge = lg[max_pagerank_index];

            // 将选取的边加入feedback_arcs
            feedback_arcs.emplace_back(max_pagerank_edge);

            // 删除选取的边, 线图中的edge_pair存储了原图中两个节点的id对，所以可以直接删除，使用vertexToEdgeMap
            const auto &e = vertex_to_edge_map[max_pagerank_index];
            boost::remove_edge(e, g);
        }
        count++;
    }
    // 释放多余的空间
    feedback_arcs.shrink_to_fit();
    SPDLOG_INFO("Successfully compute FAS, FAS number: {}, Time Elapsed: {} s", feedback_arcs.size(), duration / 1000.0);
    // 打印节点数
    SPDLOG_INFO("Graph has {} nodes and {} edges.", boost::num_vertices(g), boost::num_edges(g));
    return feedback_arcs;
}

using AdjacentEdgesCache = emhash8::HashMap<Vertex, boost::container::vector<std::pair<LineVertex, Vertex>>>;

void getAdjacentEdges(const Graph &g, Vertex u, const emhash8::HashSet<Vertex> &scc,
                      const EdgeToVertexMap &edge_to_vertex_map, AdjacentEdgesCache &cache) {
    if (cache.find(u) != cache.end()) {
        return;
    }

    boost::container::vector<std::pair<LineVertex, Vertex>> adjacent_edges;
    for (const auto &ej : boost::make_iterator_range(boost::out_edges(u, g))) {
        const auto &k = boost::target(ej, g);
        if (!scc.count(k)) {
            continue;
        }
        adjacent_edges.push_back({edge_to_vertex_map.at({g[u], g[k]}), k});
    }
    cache[u] = std::move(adjacent_edges);
}

void PageRankFAS::getLineGraph(const Graph &g, LineGraph &lineGraph, Vertex v, LineVertex prev,
                               boost::container::vector<bool> &visited, const emhash8::HashSet<Vertex> &scc,
                               EdgeToVertexMap &edge_to_vertex_map) {
    std::stack<std::pair<Vertex, LineVertex>> dfsStack;
    AdjacentEdgesCache adjEdgesCache;
    dfsStack.push({v, prev});

    while (!dfsStack.empty()) {
        auto curr = dfsStack.top();
        dfsStack.pop();
        const Vertex &curr_vertex = curr.first;
        const LineVertex &curr_prev = curr.second;

        if (visited[curr_vertex]) {
            continue;
        }

        visited[curr_vertex] = true;
        getAdjacentEdges(g, curr_vertex, scc, edge_to_vertex_map, adjEdgesCache);
        const auto &adjacent_edges = adjEdgesCache[curr_vertex];

        for (const auto &[z, u] : adjacent_edges) {
            if (curr_prev != INVALID_VERTEX && curr_prev != z) {
                boost::add_edge(curr_prev, z, lineGraph);
            }

            if (!visited[u]) {
                dfsStack.push({u, z});
            } else {
                getAdjacentEdges(g, u, scc, edge_to_vertex_map, adjEdgesCache);
                const auto &u_adjacent_edges = adjEdgesCache[u];

                for (const auto &[uk, k] : u_adjacent_edges) {
                    if (uk != z) {
                        boost::add_edge(z, uk, lineGraph);
                    }
                }
            }
        }
    }
}

void computePageRankWorker(const LineGraph &lineGraph, const boost::container::vector<float> &old_pagerank, boost::container::vector<float> &pagerank, int start, int end) {
    for (auto vi = start; vi < end; ++vi) {
        float rank_sum = 0.0f;
        for (const auto &ei : boost::make_iterator_range(boost::in_edges(vi, lineGraph))) {
            const auto &u = boost::source(ei, lineGraph);
            const auto &n_out_edges = boost::out_degree(u, lineGraph);
            if (n_out_edges > 0) {
                rank_sum += old_pagerank[u] / n_out_edges;
            }
        }
        pagerank[vi] = rank_sum;
    }
}

void PageRankFAS::computePageRank(const LineGraph &lineGraph, boost::container::vector<float> &pagerank) {
    const int max_iterations = 5;
    const int num_threads = boost::thread::hardware_concurrency(); // 获取硬件支持的线程数

    pagerank.resize(boost::num_vertices(lineGraph), 1.0f / boost::num_vertices(lineGraph));
    boost::container::vector<float> old_pagerank(boost::num_vertices(lineGraph));

    boost::asio::thread_pool thread_pool(num_threads); // 使用 Boost.Asio 创建线程池

    for (unsigned int iter = 0; iter < max_iterations; iter++) {
        old_pagerank.swap(pagerank); // 使用 swap 函数提高访存效率

        int num_vertices_per_thread = boost::num_vertices(lineGraph) / num_threads;
        for (unsigned int i = 0; i < num_threads; ++i) {
            int start = i * num_vertices_per_thread;
            int end = (i == num_threads - 1) ? boost::num_vertices(lineGraph) : (i + 1) * num_vertices_per_thread;
            boost::asio::post(thread_pool, boost::bind(computePageRankWorker, boost::ref(lineGraph), boost::ref(old_pagerank), boost::ref(pagerank), start, end));
        }

        thread_pool.join(); // 等待线程池中的所有任务完成

        // 如果小于阈值提前结束
        float diff = 0.0f;
        for (unsigned int i = 0; i < boost::num_vertices(lineGraph); ++i) {
            diff += std::abs(pagerank[i] - old_pagerank[i]);
        }
        if (diff < 1e-6) {
            break;
        }
    }

    thread_pool.stop(); // 停止线程池
}

void tarjan(Graph& G, Vertex v, boost::container::vector<int>& dfn, boost::container::vector<int>& low, boost::container::vector<bool>& on_stack, boost::container::vector<int>& scc, boost::container::vector<Vertex>& stack, int& index, int& scc_count) {
    dfn[v] = low[v] = ++index;
    on_stack[v] = true;
    stack.emplace_back(v);

    for (const auto &e : boost::make_iterator_range(boost::out_edges(v, G))) {
        const auto &w = boost::target(e, G);
        if (scc[w] == -1) {
            if (low[w] == -1) {
                tarjan(G, w, dfn, low, on_stack, scc, stack, index, scc_count);
                low[v] = std::min(low[v], low[w]);
            } else if (on_stack[w]) {
                low[v] = std::min(low[v], dfn[w]);
            }
        }
    }

    if (low[v] == dfn[v]) {
        auto w = stack.back();
        stack.pop_back();
        on_stack[v] = false;
        scc[v] = scc_count;
        while (w != v) {
            scc[w] = scc_count;
            w = stack.back();
            stack.pop_back();
            on_stack[w] = false;
        }
        scc_count++;
    }
}

void PageRankFAS::computeStronglyConnectedComponents(Graph& g, boost::container::vector<emhash8::HashSet<Vertex>>& sccs) {
    // 利用 Tarjan 算法计算给定图中的强连接分量
    boost::container::vector<int> dfn(boost::num_vertices(g), -1);
    boost::container::vector<int> low(boost::num_vertices(g), -1);
    boost::container::vector<bool> on_stack(boost::num_vertices(g), false);
    boost::container::vector<int> scc(boost::num_vertices(g), -1);
    boost::container::vector<Graph::vertex_descriptor> stack;

    int index = 0;
    int scc_count = 0;
    for (auto v : boost::make_iterator_range(boost::vertices(g))) {
        if (scc[v] == -1) {
            tarjan(g, v, dfn, low, on_stack, scc, stack, index, scc_count);
        }
    }

    emhash8::HashSet<Vertex> tmp;
    sccs.resize(scc_count, tmp);
 
    for (auto v : boost::make_iterator_range(boost::vertices(g))) {
        sccs[scc[v]].insert(v);
    }
}