#ifndef SERVER_HPP
#define SERVER_HPP

#include "libraries.hpp"

#include "networksec/tcpSocket.hpp"
#include "handlersec/task.hpp"
#include "handlersec/worker.hpp"

#define WORKERCOUNT 2



namespace Gut {

	class Server {
	private: 
		TcpSocket* serverSocket;
		std::queue<Task> taskQueue; //main taks queue, server pushes client
		// requests in and handlers complete the tasks and push to the an adequate outgoing messages queue

		Worker* workers[WORKERCOUNT] = {};

		//safe access to the task queue
		std::mutex taskMutex;
		std::condition_variable taskCV;

		//safe access to the message queue
		std::mutex messageMutex;

		//send responses to clients
		void sendResponses(fd_set& writefds); 
		//checks which clients sent requests and adds it to task queue
		void checkRequests(ClientSet& clients, fd_set& readfds);
		//appends outgoing messages to clients OutBuffers
		void proccesOutMessages();
		//flush the client buffer (as much as os allows)
		bool flushClient(Client& client);
		//kick a client out
		void kick(Client& client);
		ClientSet::iterator kick(ClientSet::iterator it);

	public:
		Server();
		~Server();
		//initialize server socket, statr listening and intialize other subsystems
		void serverStart();
		//mian loop
		void serverRun();
		//push new task to the task queue
		void pushTask(Task&& task);		
		//allows workers to add messages to the outgoing messages queue
		void addMessage(Message&& message);
	};
}
#endif