#include "worker.hpp"
#include "../program/server.hpp"
#include <iostream>

namespace Gut
{

	Worker::Worker(Server *srv)
		: server(srv) {}

	Worker::~Worker()
	{
		if (thread.joinable())
			thread.join();
	}

	void Worker::start()
	{
		thread = std::thread(&Worker::run, this);
	}

	void Worker::run()
	{
		while (true)
		{
			// BLOCKS until task OR shutdown
			std::unique_ptr<Task> task = server->popTask();

			if (!task)
				return; // server shutting down

			try
			{
				std::cout << "new task is being proccessed" << std::endl;
				//allows tasks to push more then one message
				task->setServer(server);
				std::optional<Message> result = task->execute();
				if(result)
					server->addMessage(std::move(result.value()));
			}
			catch (Errors err)
			{
				// task failed, worker lives
				std::cout << "Worker: task execution failed: Errors-" << err << std::endl;
			}
			catch(std::runtime_error err){
				std::cout << "Worker: task execution failed" << err.what() << std::endl;
			}
			catch(...){
				std::cout << "Worker: task execution failed due to unknown bug" << std::endl;
			}
		}
	}

}