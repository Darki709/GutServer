#ifndef WORKER_HPP
#define WORKER_HPP

#include "../libraries.hpp"
#include "task.hpp"

namespace Gut {
    class Server; // forward declaration

    class Worker {
    private:
        std::thread thread;
        Server* server; // pointer to Server, forward-declared
        bool stopFlag = false;

        void run(); // main worker loop

    public:
        explicit Worker(Server* srv);
        ~Worker();

        void start();
        void stop();
    };
}

#endif
