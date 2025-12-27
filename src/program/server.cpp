#include "server.hpp"


Gut::Server& Gut::Server::getInstance(){
	static Server instance;
	return instance;
}

Gut::Server::Server()
{
	serverSocket = new Gut::TcpSocket();
	for (int i = 0; i < WORKERCOUNT; i++)
	{
		workers[i] = new Worker(taskQueue);
	}
	std::cout << "Server instance created" << std::endl;
}

Gut::Server::~Server()
{
	delete serverSocket;
	for (int i = 0; i < WORKERCOUNT; i++)
	{
		delete workers[i];
	}
}

void Gut::Server::serverStart()
{
	try
	{
		serverSocket->bind();
		serverSocket->listen();
		for (int i = 0; i < WORKERCOUNT; ++i)
		{
			workers[i]->start();
		}
	}
	catch (...)
	{
		std::cout << "Failed to create and bind server socket (TCP)" << std::endl;
		throw;
	}
}

/*===============================*/
// main server loop
void Gut::Server::serverRun()
{
	while (true)
	{
		fd_set readfds;
		fd_set writefds;
		timeval tv{0, 20000}; // 20 ms, maximum block time by select()

		FD_ZERO(&readfds);
		FD_SET(serverSocket->getSocket(), &readfds);
		FD_ZERO(&writefds);
		// add clients to check for readability
		for (auto &pair : clients)
		{
			FD_SET(pair.first, &readfds);
		}
		// build write set only for clients with pending outgoing messages
		for (auto &[socket, client] : clients)
		{
			if (!client.getOutBuffer().empty())
			{
				FD_SET(socket, &writefds);
			}
		}
		// block thread and wait for incoming or outgoing messages
		int activity = select(0, &readfds, &writefds, nullptr, &tv);
		if (activity == SOCKET_ERROR)
		{
			std::cout << "select() failed: " << WSAGetLastError() << std::endl;
			continue;
		}
		else
		{
			// check for new connections
			if (FD_ISSET(serverSocket->getSocket(), &readfds))
			{
				acceptClients();
			}
			checkRequests(clients, readfds);
			sendResponses(writefds);
		}
	}
}
/*==================================*/

void Gut::Server::pushTask(std::unique_ptr<Task> task)
{
	{
		std::lock_guard<std::mutex> lock(taskMutex);
		taskQueue.push(std::move(task));
	} // mutex releases automatically
	std::cout << "New task from client ID: " << task->getClient().credentials.userId << std::endl;
	taskCV.notify_one();
}

void Gut::Server::sendResponses(fd_set &writefds)
{
	// ready clientbuffers with messages
	proccesOutMessages();
	for (auto it = clients.begin(); it != clients.end();)
	{
		Client &client = it->second;

		if (FD_ISSET(client.getSocket(), &writefds))
		{
			if (!flushClient(client))
			{
				// client has network problem, it's his problem not the server's );
				it = kick(it);
				continue;
			}
		}
		++it;
	}
}

bool Gut::Server::flushClient(Client &client)
{

	// client has nothing to send so ecerything is ok
	if (client.getOutBuffer().empty())
		return true;

	// initiate send
	int sent = send(
		client.getSocket(),
		client.getOutBuffer().data(),
		(int)client.getOutBuffer().size(),
		0);

	// check status of sent
	if (sent > 0)
	{
		// remove the first [sent] characters
		client.popOutBuffer(sent);
		return true;
	}

	// checks error
	if (sent == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK)
			return true;
	}

	return false; // fatal error
}

void Gut::Server::proccesOutMessages()
{
	std::queue<Message> local;

	{
		std::lock_guard<std::mutex> lock(messageMutex);
		// take ownership of the outgoing messages from the workers
		std::swap(local, messageQueue);
	}

	while (!local.empty())
	{
		Message &msg = local.front();

		auto it = clients.find(msg.getRecipient());
		try{
			if (it != clients.end())
			{
				MessageCodec::encode(msg, it->second); //encrypts the message nad frames it according to protocol
				it->second.pushOutBuffer(msg.getContent());
			}
		}catch(std::runtime_error e){
			//client's keys are corrupted
			kick(it);
			std::cout << e.what() << std::endl;
		}
		local.pop();
	}
}

//recieves bytes from clients and checks if there are full messages ready to parse to tasks
void Gut::Server::checkRequests(ClientSet &clients, fd_set &readfds)
{
	// check clients for requests and push to their buffers
	for (auto& [socket, client] : clients)
	{
		SOCKET clientSocket = socket;
		if (FD_ISSET(clientSocket, &readfds))
		{
			try
			{
				String msg = serverSocket->receive(clientSocket);
				if (msg.length() > 0)
				{
					client.pushInBuffer(msg);
				}
				const String &clientBuffer = client.getInBuffer();
				while (clientBuffer.length() >= 4)
				{
					uint32_t msgLength;
					// get length of message and format for host machine
					memcpy(&msgLength, clientBuffer.data(), 4);
					msgLength = ntohl(msgLength);

					// check if full message was recieved
					if (clientBuffer.length() < 4 + msgLength)
						break;

					msg = clientBuffer.substr(4, msgLength);
					//decode the message
					Message message(msg, clientSocket);
					MessageCodec::decode(message, client);
					// add task
					pushTask(std::move(TaskFactory::createTask(std::move(message), client)));
					
					// remove the extracted message from buffer
					client.popOutBuffer(4 + msgLength);
				}
			}
			catch (int err)
			{
				if (err = WSAECONNRESET || err == WSAENOTCONN || err == WSAESHUTDOWN)
				{
					kick(client);
				}
			}
			catch(std::runtime_error err){
				kick(client);
			}
		}
	}
}

Gut::ClientSet::iterator Gut::Server::kick(ClientSet::iterator it)
{
	Client &client = it->second;
	SOCKET sock = client.getSocket();

	// Close the socket
	closesocket(sock);

	// Remove client from map and return next iterator, used to kick while looping over the client set
	return clients.erase(it);
}

void Gut::Server::kick(Client &client)
{

	SOCKET sock = client.getSocket();

	// Close socket
	closesocket(sock);

	// remove from client map
	clients.erase(sock);
}

Gut::Client& Gut::Server::getClient(SOCKET socket) {
    auto it = clients.find(socket);

    if (it == clients.end()) {
        //checks if client exists
        throw CLIENTNOTFOUND;
    }

    return it->second; //Returns the Gut::Client&
}

void Gut::Server::acceptClients() {
	SOCKET socket;
	while((socket = serverSocket->accept()) != INVALID_SOCKET){
		clients.emplace(socket, Client{socket});
	}
	if(socket != WSAEWOULDBLOCK) {
		std::cout << WSAGetLastError() << std::endl;
	}
}