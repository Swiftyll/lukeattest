#include "comp2.hpp"
#include <zmq.hpp>
#include <zmq_addon.hpp>
#include <string>
#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <cassert>
#include <fstream>
#include <fcntl.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor_database.h>
#include <google/protobuf/dynamic_message.h>


using namespace google::protobuf;

void readReply(zmq::message_t &recMsg);		

void *worker_routine (void *arg)
{
    zmq::context_t *context = (zmq::context_t *) arg;
    zmq::socket_t socket (*context, ZMQ_REP);
    socket.connect ("inproc://workers");

    bool ok = true;

    while (true) {
        //  Wait for next request from client
        zmq::multipart_t mp_request;
        zmq::message_t recMsg;
        ok = mp_request.recv(socket);

        assert(mp_request.size() == 1);
        assert(ok);
		recMsg = mp_request.pop(); 

		//do work
		readReply(recMsg);		
		std::cout << "Message Received" << endl;
        // Send reply back to client
        zmq::message_t reply (6);
        memcpy ((void *) reply.data (), "World", 6);
        socket.send (recMsg);
    }
    return (NULL);
}

void readReply(zmq::message_t &recMsg){
	
//  cfile is a c file descriptor (not to be confused with a protobuf FileDescriptor object)
    int cfile = open("allProto.desc", O_RDONLY);
	
    FileDescriptorSet fds;

//  Parse a FileDescriptorSet object directly from the file
//  Has the format of a protobuf Message - subclass FileDescriptorSet, defined in <google/protobuf/descriptor.pb.h>
//  https://developers.google.com/protocol-buffers/docs/reference/cpp/google.protobuf.descriptor.pb#FieldOptions_CType.details
    fds.ParseFromFileDescriptor(cfile);
		
//  Use FileDescriptorSet method to print to screen
  //  fds.SerializeToOstream(&cout);
	
	close(cfile);
	
// A DescriptorPool is required: provides methods to identify and manage message types at run-time
// DescriptorPool can be populated from a SimpleDescriptorDatabase, which can be populated with FileDescriptorProto objects
    SimpleDescriptorDatabase sddb;
    for ( int i = 0; i < fds.file_size() ; i++ ){
	   //Iterate over the "file" collection in the FileDescriptorSet
	   //Populate the sddb
       sddb.Add(fds.file(i));
    }
	
// Now construct the DescriptorPool
    DescriptorPool dp(&sddb);

// DynamicMessageFactory is constucted from a populated DescriptorPool
// DescriptorPool, Descriptor, FieldDescriptor etc.: see descriptor.h  - 
// https://developers.google.com/protocol-buffers/docs/reference/cpp/google.protobuf.descriptor
    DynamicMessageFactory dmf(&dp);
	
    const Descriptor* desc;
	const Descriptor* payload_desc;
	
    desc = dp.FindMessageTypeByName("DescribedMessage");

// Example of dynamically creating a message from a Descriptor, retrieved by name string
    Message *msg = dmf.GetPrototype(desc)->New();
	msg->ParseFromArray(recMsg.data(),recMsg.size());
		
// Messages with required fields - Need populated. 
// Requires FieldDescriptor objects to access
    const FieldDescriptor* nameField = desc->FindFieldByName("full_name");
	const FieldDescriptor* dataField = desc->FindFieldByName("message");

	
// Reflection object provides R/W access to dynamic message fields, using FieldDescriptors
// https://developers.google.com/protocol-buffers/docs/reference/cpp/google.protobuf.message#Message.GetReflection.details
// Good example of Reflection at top of that page

    const Reflection *msgRefl = msg->GetReflection();

//  Make payload message
	payload_desc = dp.FindMessageTypeByName(msgRefl->GetString(*msg, nameField));
	Message *payload_msg = dmf.GetPrototype(payload_desc)->New();
	payload_msg->ParseFromString(msgRefl->GetString( *msg, dataField));
	
// Reflection of payload message
	const Reflection *main_msgRefl = payload_msg->GetReflection();
	
// Payload fielddescriptors
	const FieldDescriptor* main_debugField = payload_desc->FindFieldByName("debug");
	main_msgRefl->SetString (payload_msg,main_debugField,"Read");
	std::cout << "Payload Read" << endl;
	
// Put the payload data back into the envelope 
	string payload_data;
	payload_msg->SerializeToString(&payload_data);
	msgRefl->SetString(msg, dataField, payload_data);
	
// Now that required fields are populated, the dynamic message can be serialized and printed out.
    string data;
    msg->SerializeToString(&data);


// put data back into message to be replied 
	memcpy(recMsg.data(), data.c_str(), data.length());

// Useful examples of dynamic protobuf logic here : http://www.programmershare.com/2803644/
// (English not very good)
// 3.4 also describes dynamic compilation Uses google::protobuf::compiler Importer class
// Another link : dynamic stuff : 
//  http://stackoverflow.com/questions/11996557/how-to-dynamically-build-a-new-protobuf-from-a-set-of-already-defined-descriptor
//  https://bitbucket.org/g19fanatic/prototest/src/dfd73a577dcc9bb51cbb3e99319cad352bfc60a8/src/main.cpp?at=master&fileviewer=file-view-default
}

void descriptorTests(){
// Objective is to make this Server class independent of generated protobuf structures
// Move dependency from compile/link-time to run-time.
//
// Read the allProto.desc file descriptor set 
// File generated by protoc --descriptor_set_out=allProto.desc path/to/proto-files.proto (performed by make)
//
//  c++ fstream method for opening the file
//    fstream in("allProto.desc", ios::in | ios::binary);
//    io::IstreamInputStream raw_in(&in);

//  cfile is a c file descriptor (not to be confused with a protobuf FileDescriptor object)
    int cfile = open("allProto.desc", O_RDONLY);
    FileDescriptorSet fds;
//  Parse a FileDescriptorSet object directly from the file
//  Has the format of a protobuf Message - subclass FileDescriptorSet, defined in <google/protobuf/descriptor.pb.h>
//  https://developers.google.com/protocol-buffers/docs/reference/cpp/google.protobuf.descriptor.pb#FieldOptions_CType.details
    fds.ParseFromFileDescriptor(cfile);
//  Use FileDescriptorSet method to print to screen
    fds.SerializeToOstream(&cout);


// Re-open file. Example of an alternative means for reading 
// FileInputStream
    cfile = open("allProto.desc", O_RDONLY);
    io::FileInputStream fis(cfile);
// Another means may be to use a CodedInputStream, but don't need the extra features at this time
//    e.g.  io::CodedInputStream cis(fis); See http://www.mail-archive.com/protobuf@googlegroups.com/msg03417.html
    const void* buffer;
    int size;
    while(fis.Next(&buffer, &size)){
      cout.write((const char*)buffer, size);
      cout << endl << size << endl;
    }
// End of alternative read example
    close(cfile);
	
	

// fds is a protobuf message containing a collection of FileDescriptorProto objects (Message instances)
// There are (several?) ways to convert such objects into live Descriptor objects

// How many files in the set?
    cout << "Num protos " << fds.file_size() << endl;
	
    FileDescriptorProto  fdp;
// Example of indexing and examining properties
    fdp = fds.file(1);
    cout << fdp.name() << endl;

// A DescriptorPool is required: provides methods to identify and manage message types at run-time
// DescriptorPool can be populated from a SimpleDescriptorDatabase, which can be populated with FileDescriptorProto objects
    SimpleDescriptorDatabase sddb;
    for ( int i = 0; i < fds.file_size() ; i++ ){
	   //Iterate over the "file" collection in the FileDescriptorSet
	   //Populate the sddb
       sddb.Add(fds.file(i));
    }
	// Now construct the DescriptorPool
    DescriptorPool dp(&sddb);

	// DynamicMessageFactory is constucted from a populated DescriptorPool
	// DescriptorPool, Descriptor, FieldDescriptor etc.: see descriptor.h  - 
	// https://developers.google.com/protocol-buffers/docs/reference/cpp/google.protobuf.descriptor
    DynamicMessageFactory dmf(&dp);
    const Descriptor* desc;
    desc = dp.FindMessageTypeByName("Person");
	// Example of dynamically creating a message from a Descriptor, retrieved by name string
    Message *msg = dmf.GetPrototype(desc)->New();

	// Messages with required fields - Need populated. 
	// Requires FieldDescriptor objects to access
    const FieldDescriptor* idField = desc->FindFieldByName("id");
    const FieldDescriptor* nameField = desc->FindFieldByName("name");

	// Reflection object provides R/W access to dynamic message fields, using FieldDescriptors
	// 
	// https://developers.google.com/protocol-buffers/docs/reference/cpp/google.protobuf.message#Message.GetReflection.details
	// Good example of Reflection at top of that page
    const Reflection *msgRefl = msg->GetReflection();
    msgRefl->SetInt32( msg, idField, 8123);
    msgRefl->SetString( msg, nameField, "Does it work?");

	//Now that required fields are populated, the dynamic message can be serialized and printed out.
    string data;
   // msg->SerializeToString(&data);
    cout << data << endl;

	// Useful examples of dynamic protobuf logic here : http://www.programmershare.com/2803644/
	// (English not very good)
	// 3.4 also describes dynamic compilation Uses google::protobuf::compiler Importer class
	// Another link : dynamic stuff : 
    //  http://stackoverflow.com/questions/11996557/how-to-dynamically-build-a-new-protobuf-from-a-set-of-already-defined-descriptor
	//  https://bitbucket.org/g19fanatic/prototest/src/dfd73a577dcc9bb51cbb3e99319cad352bfc60a8/src/main.cpp?at=master&fileviewer=file-view-default

}

int Comp2::c2method( int input){


    //  Prepare our context and sockets
    zmq::context_t context (1);
    zmq::socket_t clients (context, ZMQ_ROUTER);
    clients.bind ("tcp://0.0.0.0:8123");
    zmq::socket_t workers (context, ZMQ_DEALER);
    workers.bind ("inproc://workers");

    //  Launch pool of worker threads
    for (int thread_nbr = 0; thread_nbr != 10; thread_nbr++) {
        pthread_t worker;
        pthread_create (&worker, NULL, worker_routine, (void *) &context);
    }
    //  Connect work threads to client threads via a queue
    zmq::proxy ((void *)clients, (void *)workers, NULL);

  return input * 4;

}
