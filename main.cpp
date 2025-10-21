// compiler: g++ 15
#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include <format>
#include <iomanip>
#include <execution>
#include <thread>
using namespace std;

template <class F>
double timeit(F &&f, int repeat = 1)
{
    using namespace chrono;
    auto start = high_resolution_clock::now();
    while (repeat--)
        f();
    auto end = high_resolution_clock::now();
    double ms = duration_cast<duration<double, milli>>(end - start).count();
    cout << fixed << setprecision(3) << ms << " ms\n";
    return ms;
}


vector<int> getRandomInts(size_t n, int min, int max)
{
    random_device r;
    seed_seq seeds{r(), r(), r(), r(), r(), r()};
    mt19937 gen(seeds);
    uniform_int_distribution<int> dist(min, max);

    vector<int> v;
    v.reserve(n);
    for (size_t i = 0; i < n; ++i)
        v.push_back(dist(gen));
    return v;
}

void demo_allof_no_policy()
{
    cout << "\nExperiment 1: std::all_of (no policy)\n";
    vector<size_t> sizes = {1000000, 10000000, 50000000};
    int threshold = 101;
    int trials = 100;

    for (auto n : sizes)
    {
        auto data = getRandomInts(n, 0, 100);
        cout << "Size: " << n << " elements, trials=" << trials << ","<< " time ";
        bool result = false;

        timeit([&]()
               { result = all_of(data.begin(), data.end(), [&](int x)
                                 { return std::sqrt(x * x + 10) < threshold; }); },
               trials);

    }
}

void demo_allof_policies()
{
    cout << "\nExperiment 2: std::all_of with execution policies\n";
    vector<size_t> sizes = {1000000, 10000000, 50000000};
    int threshold = 101;
    int trials = 100;

    for (auto n : sizes)
    {
        auto data = getRandomInts(n, 0, 100);
        cout << "\nSize: " << n << " elements\n";
        cout << string(40, '-') << endl;

        bool result = false;

        cout << "seq: ";
        timeit([&]()
               { result = all_of(execution::seq, data.begin(), data.end(), [&](int x)
                                 {return std::sqrt(x * x + 10) < threshold;}); },
               trials);
        cout << "parallel: ";
        timeit([&]()
               { result = all_of(execution::par, data.begin(), data.end(), [&](int x)
                                 { return std::sqrt(x * x + 10) < threshold; }); },
               trials);

        cout << "unseq: ";
        timeit([&]()
               { result = all_of(execution::unseq, data.begin(), data.end(), [&](int x)
                                 {return std::sqrt(x * x + 10) < threshold; }); },
               trials);

        cout << "par_unseq: ";
        timeit([&]()
               { result = all_of(execution::par_unseq, data.begin(), data.end(), [&](int x)
                                 { return std::sqrt(x * x + 10) < threshold; }); },
               trials);
    }
}

bool parallel_all_of(const vector<int> &data, int threshold, int K)
{
    size_t n = data.size();
    if (K <= 0 || n == 0)
        return true;

    size_t block_size = n / K;
    vector<bool> results(K, true);
    vector<thread> threads;

    for (int i = 0; i < K; ++i)
    {
        size_t start = i * block_size;
        size_t end = (i == K - 1) ? n : (i + 1) * block_size;

        threads.emplace_back([&, i, start, end]()
                             {
                                 results[i] = all_of(data.begin() + start, data.begin() + end, [&](int x)
                                                     { return std::sqrt(x * x + 10) < threshold; }); });
    }

    for (auto &t : threads)
        t.join();

    return all_of(results.begin(), results.end(), [](bool x)
                  { return x; });
}

void demo_parallel_custom()
{
    cout << "\nExperiment 3: custom parallel all_of (varying K)\n";
    size_t n = 50'000'000;
    int threshold = 101;
    auto data = getRandomInts(n, 0, 100);

    unsigned int hw = thread::hardware_concurrency();
    cout << "Hardware threads: " << hw << "\n\n";

    vector<int> K_values = {1, 2, 4, 8, 12, 16};

    vector<double> times;

    for (int K : K_values)
    {
        double ms = timeit([&]()
                           { bool result = parallel_all_of(data, threshold, K);
                             (void)result; });
        times.push_back(ms);
    }

    cout << left << setw(8) << "K"
         << setw(15) << "Time (ms)"
         << setw(12) << "Speedup" << endl;
    cout << string(35, '-') << endl;

    for (size_t i = 0; i < K_values.size(); ++i)
    {
        double speedup = times[0] / times[i];
        cout << left << setw(8) << K_values[i]
             << setw(15) << fixed << setprecision(3) << times[i]
             << "x" << fixed << setprecision(2) << speedup << endl;
    }

    auto min_it = min_element(times.begin(), times.end());
    int best_K = K_values[min_it - times.begin()];

    cout << "\nBest speed at K = " << best_K << endl;
    cout << "Number of hardware threads: " << hw << endl;
    cout << "K to hardware thread ratio: "
     << fixed << setprecision(2)
     << static_cast<double>(best_K) / hw << endl;

}

int main()
{
    demo_allof_no_policy();
    demo_allof_policies();
    demo_parallel_custom();
    return 0;
}