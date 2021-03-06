/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++ ) {
		NULLADDR[i] = 0;
	}
	this->memberNode = member;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
	/*
	 * This function is partially implemented and may require changes
	 */
	int id = *(int*)(&memberNode->addr.addr);
	int port = *(short*)(&memberNode->addr.addr[4]);

	memberNode->bFailed = false;
	memberNode->inited = true;
	memberNode->inGroup = false;
    // node is up!
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = TFAIL;
	memberNode->timeOutCounter = -1;
    initMemberListTable(memberNode);

    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
	MessageHdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if ( 0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
    }
    else {
        size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long) + 1;
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // create JOINREQ message
        msg = new MessageHdr();
        msg->msgType = JOINREQ;
        msg->members_list_vector = memberNode->memberList;
        msg->addr = &memberNode->addr;

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, msgsize);

        free(msg);
    }

    return 1;

}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode(){
   /*
    * Your code goes here
    */
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
    	return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
    	return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
    	ptr = memberNode->mp1q.front().elt;
    	size = memberNode->mp1q.front().size;
    	memberNode->mp1q.pop();
    	recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size ) {
	/*
	 * Your code goes here
	 */
    // Suppose SENDER == A and RECEIVER == B
    MessageHdr* msg = (MessageHdr*) data;
    if(msg->msgType == JOINREQ){
        // 1. add A to B's membership list 
        push_member_to_memberList(msg);
        // 2. send JOINREP to A
        Address* toaddr = msg->addr;
        MessageHdr* msg_JOINREP = new MessageHdr();
        msg_JOINREP->msgType = JOINREP;
        msg_JOINREP->members_list_vector = memberNode->memberList;
        msg_JOINREP->addr = &memberNode->addr;
        emulNet->ENsend(&memberNode->addr, toaddr, (char*)msg_JOINREP, sizeof(MessageHdr));
    }

    if(msg->msgType == JOINREP){
        // 1. add A to B's membership list
        push_member_to_memberList(msg); 
        // 2. set memberNode->inGroup = true
        memberNode->inGroup = true;
    }
    else if(msg->msgType == PING){
        int id = 0;
        short port;
        memcpy(&id, &msg->addr->addr[0], sizeof(int));
        memcpy(&port, &msg->addr->addr[4], sizeof(short));
        MemberListEntry* src_member = check_member_list(id, port);
        // if A exists in B's membership list, then update A's heartbeat and t.s. in B's membership list
        if(src_member != nullptr) {
            src_member->heartbeat++;
            src_member->timestamp = par->getcurrtime();
        }
        // else add A to B's membership list
        else {
            push_member_to_memberList(msg);
        }
        // iterate over all nodes in A's (sender's) membership list
        for(int i=0; i<msg->members_list_vector.size(); i++) {
            if( msg->members_list_vector[i].id > 10 || msg->members_list_vector[i].id < 0) {
                assert(false);
            }
            MemberListEntry* node = check_member_list(msg->members_list_vector[i].id, msg->members_list_vector[i].port);  
            // if node C exists in both A's and B's membership lists
            // then update C's heartbeat and t.s. in B's membership list if needed
            if(node != nullptr){
                if(msg->members_list_vector[i].heartbeat > node->heartbeat){
                    node->heartbeat = msg->members_list_vector[i].heartbeat;
                    node->timestamp = par->getcurrtime();
                }
            }
            // else if C exists in A's membership list but not in B's
            // then add C to B's membership list
            else{
                push_member_to_memberList(&msg->members_list_vector[i]);
            }
        }
    }
    delete msg;
    return true;
}

/**
 * FUNCTION NAME: push_member_to_memberList 
 * 
 * DESCRIPTION: If a node does not exist in the memberList, it will be pushed to the memberList. 
 */
void MP1Node::push_member_to_memberList(MessageHdr* msg) {
    int id = 0;
    short port;
    memcpy(&id, &msg->addr->addr[0], sizeof(int));
    memcpy(&port, &msg->addr->addr[4], sizeof(short));
    long heartbeat = 1;
    long timestamp =  this->par->getcurrtime();
    // if A is already present in B's membership list
    // then return i.e. do not add A to B's membership list
    if (check_member_list(id, port) != nullptr) {
        return;
    }
    // else if A is missing in B's membership list, then it needs to be added
    MemberListEntry mle(id, port, heartbeat, timestamp);
    memberNode->memberList.push_back(mle);
    Address* added = new Address();
    memcpy(&added->addr[0], &id, sizeof(int));
    memcpy(&added->addr[4], &port, sizeof(short));
    log->logNodeAdd(&memberNode->addr, added);
    delete added;
}

/**
 * FUNCTION NAME: push_member_to_memberList 
 * 
 * DESCRIPTION: If a node is present in the sender's memberList but missing in the receiver's, then add it to the receiver's memberList 
 */
void MP1Node::push_member_to_memberList(MemberListEntry* e) {
    Address* address = new Address();
    memcpy(&address->addr[0], &e->id, sizeof(int));
	memcpy(&address->addr[4], &e->port, sizeof(short));
    // do not add B itself to B's membership list
    if (*address == memberNode->addr) {
        delete address;
        return;
    }
    // if (current time) - (t.s. of C in A's membership list) < TREMOVE
    // i.e. if C need not be removed from the cluster
    // then add C to B's membership list
    if (par->getcurrtime() - e->timestamp < TREMOVE) {
        log->logNodeAdd(&memberNode->addr, address);
        MemberListEntry new_entry = *e;
        memberNode->memberList.push_back(new_entry);
    }
    delete address;
}

/**
 * FUNCTION NAME: check_member_list 
 * 
 * DESCRIPTION: If the node exists in the memberList, the function will return that node. Otherwise, the function will return nullptr. 
 */
MemberListEntry* MP1Node::check_member_list(int id, short port) {
    for (int i = 0; i < memberNode->memberList.size(); i++){
        if(memberNode->memberList[i].id == id && memberNode->memberList[i].port == port)
            return &memberNode->memberList[i];
    } 
    return nullptr;
}


/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {
	/*
	 * Your code goes here
	 */
    // update heartbeat of current node
    memberNode->heartbeat++;
    for (int i = memberNode->memberList.size()-1 ; i >= 0; i--) {
        // if (current time) - (t.s. of B in A's membership list) >= TREMOVE
        // i.e. if B needs to be removed from the cluster
        if(par->getcurrtime() - memberNode->memberList[i].timestamp >= TREMOVE) {
            Address* removed_addr = new Address();
            memcpy(&removed_addr->addr[0], &memberNode->memberList[i].id, sizeof(int));
	        memcpy(&removed_addr->addr[4], &memberNode->memberList[i].port, sizeof(short));
            log->logNodeRemove(&memberNode->addr, removed_addr);
            memberNode->memberList.erase(memberNode->memberList.begin()+i);
            delete removed_addr;
        }
    }

    // Send PING to the members of memberList
    for (int i = 0; i < memberNode->memberList.size(); i++) {
        Address* address = new Address();
        memcpy(&address->addr[0], &memberNode->memberList[i].id, sizeof(int));
        memcpy(&address->addr[4], &memberNode->memberList[i].port, sizeof(short));
        MessageHdr* msg_PING = new MessageHdr();
        msg_PING->msgType = PING;
        msg_PING->members_list_vector = memberNode->memberList;
        msg_PING->addr = &memberNode->addr;
        emulNet->ENsend( &memberNode->addr, address, (char*)msg_PING, sizeof(MessageHdr));
        delete address;
    }
    return;
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
	return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
	memberNode->memberList.clear();
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;    
}
