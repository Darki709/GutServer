#ifndef SERVER_HPP
#define SERVER_HPP

#include "libraries.hpp"

#include "../stateless/tcpSocket.hpp"
#include "../logic/taskFactory.hpp"
#include "../runtime/worker.hpp"
#include "../runtime/client.hpp"
#include "../core/message.hpp"
#include "../logic/task.hpp"
#include "../stateless/messageCodec.hpp"

#define WORKERCOUNT 2



namespace Gut {

	typedef std::unordered_map<SOCKET, std::shared_ptr<Client>> ClientSet;
	class Server {
	private: 
		TcpSocket* serverSocket;

		ClientSet clients;

		std::queue<Message> messageQueue;//queue where workers push messages in so the server can proccess and send them
		std::queue<std::unique_ptr<Gut::Task>> taskQueue; //main tasks queue, server pushes client
		// requests in and handlers complete the tasks and push to the an adequate outgoing messages queue

		//keep track of all te workers
		Worker* workers[WORKERCOUNT] = {};


		//loops over new connection requests
		void acceptClients();
		//send responses to clients
		void sendResponses(fd_set& writefds); 
		//checks which clients sent requests and adds it to task queue
		void checkRequests(ClientSet& clients, fd_set& readfds);
		//appends outgoing messages to clients OutBuffers
		void proccesOutMessages();
		//flush the client buffer (as much as os allows)
		bool flushClient(std::shared_ptr<Client> client);
		//kick a client out
		void kick(std::shared_ptr<Client> client);
		ClientSet::iterator kick(ClientSet::iterator it);

		//safe access to the task queue
		std::mutex taskMutex;
		std::condition_variable taskCV;

		//safe access to the message queue
		std::mutex messageMutex;

		bool running{true};
	public:

		static Server& getInstance(); //singleton contructor
		
		std::shared_ptr<Client> getClient(SOCKET socket);

		Server();
		~Server();
		//initialize server socket, statr listening and intialize other subsystems
		void serverStart();
		//mian loop
		void serverRun();
		//push new task to the task queue
		void pushTask(std::unique_ptr<Task> task);
		//pop task
		std::unique_ptr<Task> popTask();		
		//allows workers to add messages to the outgoing messages queue
		void addMessage(Message&& message);

		
		bool taskQueueEmpty();
	};
}
#endif