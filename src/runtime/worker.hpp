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
		virtual Stock_helper* getStockHelper() = 0;
	};

    class Worker : public ThreadResources {
    private:
        std::thread thread;
		Stock_helper* stockHelper;
        Server* server; // pointer to Server, forward-declared
		Stock_helper* getStockHelper() override;
        void run(); // main worker loop

    public:
        explicit Worker(Server* srv);
        ~Worker();

        void start();
        void stop();
    };
}

#endif
