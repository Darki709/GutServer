#include "server.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <cstring>

Gut::Server& Gut::Server::getInstance()
{
	static Server instance;
	return instance;
}

Gut::Server::Server()
{
	serverSocket = new Gut::TcpSocket();
	for (int i = 0; i < WORKERCOUNT; i++)
	{
		workers[i] = new Worker(this);
	}
	std::cout << "Server instance created" << std::endl;
}

Gut::Server::~Server()
{
	// signal workers to stop
	running = false;

	// wake all workers blocked on taskCV
	taskCV.notify_all();

	// destroy workers (they must join internally)
	for (int i = 0; i < WORKERCOUNT; i++)
	{
		delete workers[i];
		workers[i] = nullptr;
	}

	// close the streamer thread
	Streamer::getInstance().shutDown();

	// close server socket last
	delete serverSocket;
}

void Gut::Server::serverStart()
{
	Streamer::getInstance();
	std::cout <<"streamer started" << std::endl;
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
	//wkae up python stock data fetcher
	Gut::Stock_helper::getInstance();
	std::cout << "Python stock helper started" << std::endl;
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
			FD_SET(pair.second->getSocket(), &readfds);
		}
		// build write set only for clients with pending outgoing messages
		for (auto &[socket, client] : clients)
		{
			if (!client->getOutBuffer().empty())
			{
				FD_SET(socket, &writefds);
			}
		}
		// block thread and wait for incoming or outgoing messages
		//std::cout << "waiting on select" << std::endl;
		int activity = select(0, &readfds, &writefds, nullptr, &tv);
		if (activity == SOCKET_ERROR)
		{
			std::cout << "select() failed: " << WSAGetLastError() << std::endl;
			continue;
		}
		else
		{
			//std::cout << "checking for new clinets" << std::endl;
			// check for new connections
			if (FD_ISSET(serverSocket->getSocket(), &readfds))
			{
				acceptClients();
			}
			//std::cout << "checking for new requests" << std::endl;
			checkRequests(clients, readfds);
			//std::cout << "sending messages" << std::endl;
			sendResponses(writefds);
		}
	}
}
/*==================================*/

void Gut::Server::pushTask(std::unique_ptr<Task> task)
{	
	if(task != nullptr) 
	{
		std::lock_guard<std::mutex> lock(taskMutex);
		taskQueue.push(std::move(task));
		taskCV.notify_one();
	}
}

void Gut::Server::sendResponses(fd_set &writefds)
{
	// ready clientbuffers with messages
	proccesOutMessages();
	for (auto it = clients.begin(); it != clients.end();)
	{
		std::shared_ptr<Client> client_ptr = it->second;

		if (FD_ISSET(client_ptr->getSocket(), &writefds))
		{
			if (!flushClient(client_ptr))
			{
				// client has network problem, it's his problem not the server's );
				it = kick(it);
				continue;
			}
		}
		++it;
	}
}

bool Gut::Server::flushClient(std::shared_ptr<Client> client_ptr)
{
	// client has nothing to send so ecerything is ok
	if (client_ptr->getOutBuffer().empty())
		return true;

	// initiate send
	int sent = send(
		client_ptr->getSocket(),
		client_ptr->getOutBuffer().data(),
		(int)client_ptr->getOutBuffer().size(),
		0);

	// check status of sent
	if (sent > 0)
	{
		// remove the first [sent] characters
		client_ptr->popOutBuffer(sent);
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
		try
		{
			if (it != clients.end())
			{
				MessageCodec::encode(msg, *it->second);
				it->second->pushOutBuffer(msg.getContent());
			}
		}
		catch (std::runtime_error e)
		{
			// client's keys are corrupted
			kick(it);
			std::cout << e.what() << std::endl;
		}
		local.pop();
	}
}

// recieves bytes from clients and checks if there are full messages ready to parse to tasks
void Gut::Server::checkRequests(ClientSet &clients, fd_set &readfds)
{
	for (auto it = clients.begin(); it != clients.end();) // Manual iterator
	{
		auto &[socket, client] = *it;
		if (FD_ISSET(socket, &readfds))
		{
			try
			{
				String raw = serverSocket->receive(socket);
				std::cout << raw << std::endl;
				if (raw.length() > 0)
				{
					client->pushInBuffer(raw);
					std::cout << "[RECV] Socket " << socket << " | Received: " << raw.length()
							  << " bytes | Total Buffer: " << client->getInBuffer().length() << std::endl;
				}
				while (client->getInBuffer().length() >= 4)
				{
					uint32_t len;
					memcpy(&len, client->getInBuffer().data(), 4);
					len = ntohl(len);

					std::cout << "[FRAME] Socket " << socket << " | Header says next msg is: " << len << " bytes" << std::endl;

					if (client->getInBuffer().length() < 4 + len)
					{
						std::cout << "[WAIT] Need " << (4 + len) - client->getInBuffer().length() << " more bytes for full message." << std::endl;
						break;
					}


					//removes the length bytes before creating the message
					String msgContent = client->getInBuffer().substr(4, len);
					Message message(msgContent, socket);

					printRawMessage(msgContent);

					std::cout << "[CRYPTO] Attempting Decode..." << std::endl;
					MessageCodec::decode(message, *client);
					std::cout << "[CRYPTO] Decode Successful." << std::endl;
					printRawMessage(message.getContent());

					pushTask(TaskFactory::createTask(std::move(message), client));
					std::cout << "New task from client: " << socket << std::endl;

					client->popInBuffer(4 + len);
				}
				++it; // Only increment if we didn't kick
			}
			catch(std::runtime_error e){
				std::cout << e.what() << std::endl;
				it = kick(it);
			}
			catch (...) {
				std::cout << "exception caught" << std::endl;
                it = kick(it);
            }
		}
		else
		{
			++it;
		}
	}
}

Gut::ClientSet::iterator Gut::Server::kick(Gut::ClientSet::iterator it)
{
	SOCKET sock = it->second->getSocket();
	
	//remove client from streaming list if he is registered
	Streamer::getInstance().removeClient(sock);

	// Close the socket
	closesocket(sock);

	// Remove client from map and return next iterator, used to kick while looping over the client set
	return clients.erase(it);
}

void Gut::Server::kick(std::shared_ptr<Client> client_ptr)
{
	SOCKET sock = client_ptr->getSocket();

	//remove client from streaming list if he is registered
	Streamer::getInstance().removeClient(sock);

	// Close socket
	closesocket(sock);

	// remove from client map
	clients.erase(sock);
}

std::shared_ptr<Gut::Client> Gut::Server::getClient(SOCKET socket)
{
	auto it = clients.find(socket);

	if (it == clients.end())
	{
		// checks if client exists
		throw CLIENTNOTFOUND;
	}

	return it->second;
}

void Gut::Server::acceptClients()
{
	SOCKET socket;
	while ((socket = serverSocket->accept()) != INVALID_SOCKET)
	{
		clients.emplace(socket, std::make_shared<Client>(socket));
	}
	if (socket != WSAEWOULDBLOCK)
	{
		std::cout << WSAGetLastError() << std::endl;
	}
}

std::unique_ptr<Gut::Task> Gut::Server::popTask()
{
	std::unique_lock<std::mutex> lock(taskMutex);

	taskCV.wait(lock, [this]()
				{ return !running || !taskQueue.empty(); });

	if (!running)
		return nullptr;

	auto task = std::move(taskQueue.front());
	taskQueue.pop();
	return task;
}

void Gut::Server::addMessage(Message &&msg)
{
	std::lock_guard<std::mutex> lock(messageMutex);
	messageQueue.push(std::move(msg));
}

bool Gut::Server::taskQueueEmpty()
{
	std::lock_guard<std::mutex> lock(taskMutex);
	return taskQueue.empty();
}

void Gut::printRawMessage(const std::string& content) {
    std::cout << "Raw message (" << content.size() << " bytes): ";
    for (unsigned char c : content) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(c) << " ";
    }
    std::cout << std::dec << std::endl; // back to decimal
}
