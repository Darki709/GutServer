#include "server.hpp"

Gut::Server::Server() {
	serverSocket = new Gut::TcpSocket();
	for(int i = 0; i < WORKERCOUNT; i++){
		workers[i] = new Worker(taskQueue);
	}
	std::cout << "Server instance created" << std::endl;
}

Gut::Server::~Server() {
	delete serverSocket;
	for(int i = 0; i < WORKERCOUNT; i++){
		delete workers[i];
	}
}

void Gut::Server::serverStart() {
	try {
		serverSocket->bind();
		serverSocket->listen();
		for(int i = 0; i < WORKERCOUNT; ++i){
			workers[i]->start();
		}
	} catch (...) {
		std::cout << "Failed to create and bind server socket (TCP)" << std::endl;	
		throw;
	}
}


/*===============================*/
//main server loop
void Gut::Server::serverRun() {
	ClientSet& clients = serverSocket->getClients();
    while (true) {
		fd_set readfds;
		fd_set writefds;
		timeval tv { 0, 20000 }; // 20 ms, maximum block time by select()

		FD_ZERO(&readfds);
		FD_SET(serverSocket->getSocket(), &readfds);
		FD_ZERO(&writefds);
		//add clients to check for readability
        for(auto& pair : serverSocket->getClients()) {
            FD_SET(pair.first, &readfds);
		}
		//build write set only for clients with pending outgoing messages
		for(auto& [socket, client] : serverSocket->getClients()){
			if(!client.getOutBuffer().empty()){
				FD_SET(socket, &writefds);
			}
		}
		//block thread and wait for incoming or outgoing messages
		int activity = select(0, &readfds, &writefds, nullptr, &tv);
		if(activity == SOCKET_ERROR) {
			std::cout << "select() failed: " << WSAGetLastError() << std::endl;
			continue;
		}
		else{
			//check for new connections
			if(FD_ISSET(serverSocket->getSocket(), &readfds)) {
				serverSocket->acceptClients();
			}
			checkRequests(clients, readfds);
			sendResponses(writefds);
		}       
    }
}
/*==================================*/

void Gut::Server::pushTask(Task&& task) {
	{
	std::lock_guard<std::mutex> lock(taskMutex);
	taskQueue.push(std::move(task));
	}//mutex releases automatically
	std::cout << "New task from client ID: " << task.getClientId() << std::endl;
	taskCV.notify_one();
}

void Gut::Server::sendResponses(fd_set& writefds) {
	//ready clientbuffers with messages
	proccesOutMessages();
	ClientSet& clients = serverSocket->getClients();
	for (auto it = clients.begin(); it != clients.end();) {
        Client& client = it->second;

        if (FD_ISSET(client.getSocket(), &writefds)) {
            if (!flushClient(client)) {
				//client has network problem, it's his problem not the server's );
            	it = kick(it);
                continue;
            }
        }
		++it;
    }
}

bool Gut::Server::flushClient(Client& client) {

	//client has nothing to send so ecerything is ok
    if (client.getOutBuffer().empty())
        return true;


	//initiate send
    int sent = send(
        client.getSocket(),
        client.getOutBuffer().data(),
        (int)client.getOutBuffer().size(),
        0
    );

	//check status of sent
    if (sent > 0) {
		//remove the first [sent] characters
        client.popOutBuffer(sent);
        return true;
    }

	// checks error
    if (sent == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK)
            return true;
    }

    return false; // fatal error
}


void Gut::Server::proccesOutMessages(){
	std::queue<Message> local;

	{
		std::lock_guard<std::mutex> lock(messageMutex);
		//take ownership of the outgoing messages from the workers
		std::swap(local, serverSocket->getMessages());

	}

	ClientSet& clients = serverSocket->getClients();
	while (!local.empty()) {
        Message& msg = local.front();
		
        auto it = clients.find(msg.getRecipient().getSocket());
        if (it != clients.end()) {
			it->second.pushOutBuffer(msg.getContent());         
        }

        local.pop();
    }
}


//fix this logic to support partial recieves
void Gut::Server::checkRequests(ClientSet& clients, fd_set& readfds) {
	//check clients for requests
			for(auto& pair : clients) {
				SOCKET clientSocket = pair.first;
				if(FD_ISSET(clientSocket, &readfds)) {
					String* msg = serverSocket->receive(clientSocket);
					if(msg != nullptr) {						
						pushTask(std::move(Task::createTask(clients.at(clientSocket), *msg))); //make sure to the delete message
					}
				}
			}
}