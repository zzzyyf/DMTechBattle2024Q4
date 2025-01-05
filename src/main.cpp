#include "Batcher.h"
#include "Client.h"
#include "Server.h"

#include "argparse/argparse.hpp"
#include "asio/io_context.hpp"

#include <chrono>
#include <cstdint>

using namespace dm;

std::string host;
std::string port;
int parallel;
int n_conn;
bool is_client;

std::vector<std::thread> threads;

const uint64_t total_request = 10'000'000;

bool startClient(asio::io_context &context);
bool startClientV2(asio::io_context &context);
bool startServer(asio::io_context &context);

int main(int argc, char *argv[])
{
    argparse::ArgumentParser program("dmtb2024q4", "1.0", argparse::default_arguments::none);

    program.add_argument("-h", "--host")
        .help("server host address")
        .default_value(std::string("127.0.0.1"));

    program.add_argument("-p", "--port")
        .help("server port")
        .default_value(std::string("24004"));

    program.add_argument("--parallel")
        .help("client parallelism")
        .scan<'i', int>()
        .default_value(512);

    program.add_argument("-c, --connection")
        .help("number of client connections")
        .scan<'i', int>()
        .default_value(32);

    program.add_argument("--client")
        .help("run as client instead of server")
        .default_value(false)
        .implicit_value(true);

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    host = program.get("-h");
    port = program.get("-p");
    parallel = program.get<int>("--parallel");
    n_conn = program.get<int>("-c");
    is_client = program.get<bool>("--client");

    asio::io_context io_context;

    // bool rslt = is_client ? startClient(io_context) : startServer(io_context);
    bool rslt = is_client ? startClientV2(io_context) : startServer(io_context);
    if (!rslt)
    {
        std::cerr << "failed to start!" << std::endl;
        return 1;
    }

    return 0;
}

bool startClientV2(asio::io_context &context)
{
    std::srand(std::time(nullptr));

    try {
        context.run();
    }
    catch (std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        exit(1);
    }

    std::vector<Batcher *> batchers;
    std::vector<std::thread> threads;

    for (int i = 0; i < n_conn; i++)
    {
        Batcher *batcher = new Batcher(parallel / n_conn);
        if (!batcher->init())
            return false;

        if (!batcher->connect(context, host, port))
        {
            return false;
        }

        batchers.emplace_back(batcher);
    }

    std::cout << "client start sending " << total_request << " requests, parallel=" << parallel << std::endl;
    auto t1 = std::chrono::steady_clock::now();

    std::atomic<bool> status = true;
    uint32_t remainder = total_request % n_conn;
    for (int i = 0; i < n_conn; i++)
    {
        threads.emplace_back([&, i](){
            uint32_t count = total_request / n_conn;
            if (i == 0) {
                count += remainder;
            }
            bool rslt = batchers[i]->process(count);
            if (!rslt) {
                status = false;
                return;
            }

            batchers[i]->join();
        });
    }

    for (int i = 0; i < n_conn; i++) {
        threads[i].join();
    }
    if (!status) {
        return false;
    }

    auto t2 = std::chrono::steady_clock::now();
    std::cout << "finish " << total_request << " requests with " << parallel << " threads cost " << (t2 - t1).count() / 1000000. << " ms." << std::endl;

    return true;
}


bool startClient(asio::io_context &context)
{
    std::srand(std::time(nullptr));

    auto t1 = std::chrono::steady_clock::now();

    int avg_count = total_request / parallel;
    int remainder = total_request % parallel;

    for (int i = 0; i < parallel; i++)
    {
        threads.emplace_back([&, i](){
            Client client;
            bool rslt = client.connect(context, host, port);
            if (!rslt) {
                exit(1);
            }

            rslt = client.process(i > 0 ? avg_count : avg_count + remainder);
            if (!rslt) {
                exit(1);
            }

            return true;
        });
    }

    try {
        context.run();
    }
    catch (std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        for (auto &thread : threads)
        {
            thread.join();
        }
        exit(1);
    }

    for (auto &thread : threads)
    {
        thread.join();
    }

    auto t2 = std::chrono::steady_clock::now();

    std::cout << "finish " << total_request << " requests with " << parallel << " threads cost " << (t2 - t1).count() / 1000000. << " ms." << std::endl;

    return true;
}


bool startServer(asio::io_context &context)
{
    Server server(context, host, port);
    server.start();

    try {
        context.run();
    }
    catch (std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        exit(1);
    }

    return true;
}
