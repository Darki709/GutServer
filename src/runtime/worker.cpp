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
				std::optional<Message> result = task->execute();
				if(result)
					server->addMessage(std::move(result.value()));
			}
			catch (...)
			{
				// task failed, worker lives
				std::cerr << "Worker: task execution failed\n";
			}
		}
	}

}