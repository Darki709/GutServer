#ifndef WORKER_HPP
#define WORKER_HPP

#include "../libraries.hpp"
#include "task.hpp"

namespace Gut{

	//class to abstract aworker thread reading from the shared task queue and executing tasks
	class Worker{
		private:
			std::queue<Task>* taskQueue; 
			Task getNewTask();
			void pushMessage(Message&& message);
		public:
			Worker(std::queue<Task>& queue);
			~Worker();
			void start();
			void stop();
	};
}


#endif