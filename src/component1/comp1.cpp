#include "comp1.hpp"
#include <zmq.hpp>
#include <zmq_addon.hpp>
#include <string>
#include <iostream>
//#include <windows.h>
#include <unistd.h>
#include <fstream>
#include <sstream>

#include "src/pb2/phase2.pb.h"
#include "src/pb2/sentMessage.pb.h"

using namespace std;
using namespace google::protobuf;

void printMsg(const Message& msg, string preText = "") //pulled from example
{
    const Reflection* reflection = msg.GetReflection();
    const Descriptor* descriptor = msg.GetDescriptor();

    cout << preText << "Message: " << descriptor->full_name() << endl;
    stringstream ss;
    ss << preText << "  ";
    preText = ss.str();

    vector<const FieldDescriptor*> fields;

    reflection->ListFields(msg,&fields);

    for (unsigned int i = 0; i < fields.size(); ++i)
    {
        string strName;
        string strValue;
        bool toPrint = true;

        const Message* tempMsg;
        const FieldDescriptor* curFd = fields.at(i);

        switch (curFd->type())
        {
        case FieldDescriptor::TYPE_MESSAGE:
            toPrint = false;
            tempMsg = &reflection->GetMessage(msg,curFd);
            printMsg(*tempMsg, preText);
            break;
        case FieldDescriptor::TYPE_BYTES:
        case FieldDescriptor::TYPE_STRING:
            strName = curFd->full_name();
            strValue = reflection->GetString(msg,curFd);
            break;
        default:
            break;
        }

        if (toPrint)
        {
            cout << preText << strName << " = " << strValue << endl;
        }
    }
}

void genSavedData()    //pulled from example
{
  
	Person p = Person();
    p.set_name("Mr. Sender");
    p.set_id(999);
	
    ofstream mainOut("log/main.pbe",ios_base::binary|ios_base::trunc);
    p.SerializeToOstream(&mainOut);
    mainOut.close();


    DescribedMessage dMsg;
    dMsg.set_full_name(p.descriptor()->full_name());
    string rawMainMsg;
    p.SerializeToString(&rawMainMsg);
    dMsg.set_message(rawMainMsg);

    ofstream dOut("log/describedMsg.pbe",ios_base::binary|ios_base::trunc);
    dMsg.SerializeToOstream(&dOut);
    dOut.close();

    cout << "\nPrinting the saved main msg\n";
    printMsg(p);
}

int Comp1::c1method( int input){
    //  Prepare our context and socket
    zmq::context_t context (1);
    zmq::socket_t socket (context, ZMQ_REQ);

    std::cout << "Connecting to hello world server." << std::endl;
    socket.connect ("tcp://0.0.0.0:8123");
    //socket.connect ("ipc:///tmp/zmqipc"); //ipc not supported on windows

    Person p = Person();
    p.set_name("Luke Swift");
    p.set_id(999);
	p.set_debug("Unread");
    std::string str;
    p.SerializeToString(&str);
    
	DescribedMessage dMsg;
	
	dMsg.set_full_name(p.descriptor()->full_name());
	dMsg.set_message(str);
	dMsg.set_debug_msg("Unread");
	std::string msg;
	dMsg.SerializeToString(&msg);
	int sz = msg.length();
	
	std::cout << "Debug: " << dMsg.debug_msg();

    //  Do 10 requests, waiting each time for a response
    for (int request_nbr = 0; request_nbr != 10; request_nbr++) {

        std::cout << "\nSending Hello " << request_nbr << "." << std::endl;

		//before message sent
		p.set_debug("Unread");
		std::cout << "Person debug: " << p.debug() << endl;
		
		//send the message
		zmq::message_t query(sz);
		memcpy(query.data(), msg.c_str(), sz);
		socket.send (query);

        //  Get the reply.
        zmq::message_t reply;
        socket.recv (&reply);
		dMsg.ParseFromArray(reply.data(),reply.size());
		p.ParseFromString(dMsg.message());
		
		//read reply
		std::cout << "Person debug: " << p.debug();
		std::cout << "\nDebug: " << dMsg.debug_msg();

	usleep(500000);
    }

  return input * 2;

}
