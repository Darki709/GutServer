#ifndef WORKER_HPP
#define WORKER_HPP

#include "../libraries.hpp"
#include "stock_db_helper.hpp"
#include "task.hpp"


namespace Gut {
    class Server;
	class Stock_helper; // forward declaration
	 // forward declaration

	/**
	 * interface to pass into taskd so they can any worker owned resources
	 * **/
	class ThreadResources {
	public:
	};

    class Worker : public ThreadResources {
    private:
        std::thread thread;
        Server* server; // pointer to Server, forward-declared
        void run(); // main worker loop

    public:
        explicit Worker(Server* srv);
        ~Worker();

        void start();
        void stop();
    };
}

#endif
