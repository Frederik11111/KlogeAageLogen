#!/bin/python3

##
## The following is a Python implementation of the Peer decribed in the 
## assignment text. It meets the requirements and is designed to be reasonablly 
## easy to follow. However, it is intentially not an optimal setup and the 
## design structure demonstrated here may not be ideal for a simmilar program 
## in C.
##

import argparse
import hashlib
import math
import os
import random
import socket
import socketserver
import string
import struct
import threading
import time

# Max length of any individual message. Note this has been arbitrarily set for
# this assignment and is not meant to reflect any specfic real world limit.
MSG_MAX = 8196

# The fixed lengths of various message attributes
LEN_IP = 16
LEN_PORT = 4
LEN_REQUEST_LENGTH = 4
LEN_COMMAND_LENGTH = 4
LEN_RESPONSE_LENGTH = 4
LEN_STATUS = 4
LEN_BLOCK_ID = 4
LEN_BLOCKS_COUNT = 4
LEN_BLOCK_HASH = 32
LEN_TOTAL_HASH = 32
LEN_SIGNATURE = 32
LEN_SALT = 16

# Command codes
COMMAND_REGISTER = 1
COMMAND_RETREIVE = 2
COMMAND_INFORM = 3

# Response status codes
STATUS_OK = 1
STATUS_PEER_EXISTS = 2
STATUS_PEER_MISSING = 3
STATUS_BAD_PASSWORD = 4
STATUS_BAD_REQUEST = 5
STATUS_OTHER = 6
STATUS_MALFORMED = 7

# Setup shared variables
retrieving_mutex = threading.Lock()
currently_retreiving = []
network_mutex = threading.Lock()
network = []


def get_sha256(data: bytes):
    """
    Function to get the SHA256 hash of some given data.
    
    data(bytes): The data to hash.

    Returns the hash of the given data as a str.
    """
    return hashlib.sha256(data).digest() 


def get_random_salt():
    """
    Function to generate a random salt of a length given by the LEN_SALT global
    variable. The salt will be a collection of A-z letters and 0-9.

    Returns the salt as a str
    """
    return ''.join(random.choices(string.ascii_letters+string.digits, k=LEN_SALT))


def assemble_signature(password: str, salt: str):
    """
    Funtion to create a signature out of the provided arguments.

    password(str): The pre-salted password
    salt(str): A string to apply to the user-remembered password

    The password and salt are combined and then hashed according to sha256. 
    Returns signature as bytes
    """
    salted = f"{password}{salt}"
    signature = hashlib.sha256()
    signature.update(str.encode(salted))

    return signature.digest()


class PeerToPeerServer(socketserver.ThreadingTCPServer):
    def __init__(self, my_ip: str, my_port: int, my_signature: bytes, 
            request_handler_class: socketserver.StreamRequestHandler,
            printing: bool):
        """
        Constructor for PeerToPeerServer.

        my_ip(str): The IP address of the peer being defined
        my_port(int): The port of the peer being defined.
        my_signature(bytes): The peers user-remembered password having been
            salted and hashed
        request_handler_class(socketserver.StreamRequestHandler): A handler to
            respond to any messages sent to this server.
        printing(bool): A flag for if the server side will print or not. 

        This server inherits from the socketserver.ThreadingTCPServer. Its only
        significant difference is adding a simple note of the ip, port and 
        signature.
        
        The server also includes a toggle for if it prints or not, as may be 
        useful for a clearer system to run.
        """

        if printing:
            print(f"Starting server at: {my_ip}:{my_port}")

        super().__init__((my_ip, my_port), request_handler_class)

        self.ip = my_ip
        self.port = my_port
        self.signature = my_signature
        self.printing = printing


class RequestHandler(socketserver.StreamRequestHandler):
    """
    Custom handler to handle any inbound messages. Any input message will 
    ALWAYS receive an appropriate response, unless the inpue message is 
    informing the server of a new network member
    """

    def handle(self):
        """
        Function to initially handle any requests received. 
        
        This will read a request, perform some curseory validation before 
        calling more specific handling functions. Nothing is returned.
        """
        try:
            # Read request
            bytes_message = self.request.recv(MSG_MAX)

            if self.server.printing:
                print(f"Got: {bytes_message} ({len(bytes_message)})")

            # Extract request attributes
            ip, port, signature, request_command, request_length, request = \
                parse_message(bytes_message)

            # Update the timestamps of the network
            network_mutex.acquire()
            try:
                to_remove = []
                now = time.time()
                message_from = f"{ip}:{port}"
                for i in range(len(network)):
                    address, _, _, timestamp = network[i]
                    # Don't do anything to our own entry 
                    if timestamp == -1:
                        continue
                    # Update timestamp for who has made the connection
                    if address == message_from:
                        network[i][3] = now
                    # Flag any network nodes we havn't heard from in a while
                    if now-timestamp > 60:
                        to_remove.append(network[i])
                
                # Remove flagged network nodes
                for remove in to_remove:
                    network.remove(remove)
                    if self.server.printing:
                        print(f"Removing {remove[0]} from network")

            except:
                pass
            network_mutex.release()

            # Validate message length
            if len(request) != request_length:
                self.handle_error(
                    STATUS_MALFORMED,
                    "Malformed request. Length of request does not match "
                    "given length"
                )
                return

            # Validate command
            if request_command not in \
                    [COMMAND_REGISTER, COMMAND_RETREIVE, COMMAND_INFORM]:
                self.handle_error(
                    STATUS_MALFORMED,
                    "Request does not define valid command"    
                )
                return

            # Determine how to process request
            if request_command == COMMAND_REGISTER:
                if self.server.printing:
                    print(f'Got registration message from {ip}:{port}')
                self._register(ip, port, signature)
            elif request_command == COMMAND_INFORM:
                if self.server.printing:
                    print(f'Got inform message from {ip}:{port}')
                self._add_to_network(request)
            else:
                if self.server.printing:
                    print(f'Got request message from {ip}:{port}')
                self._handle_request(ip, port, signature, request)

        # Always generate a response, this is the fallback for if all other
        # validation and handling fails. This is acceptable as a last resort,
        # but were possible more helpful feedback and responses should be 
        # generated.
        except Exception as e:
           self.handle_error(STATUS_OTHER, f"Something went wrong. {e}")


    def _register(self, ip:str, port: int, signature: bytes):
        """
        Function to handle a 'register new user' type request from another 
        peer.

        ip(str): The ip of the peer registering with the network
        port(int): The port of the peer registering with the network
        signature(bytes): The signature of the peer reigstering with the 
            network
        
        The peer is registered, unless a peer at that address already exists. A 
        response is always generated, either the data file or an error message 
        explaining what went wrong. Once the reply has been sent, all other 
        peers on the network are informed that a new peer has joined.
        """
        # Validate new peer
        if not ip:
            self.handle_error(
                STATUS_BAD_REQUEST,
                "Cannot register empty username"
            )
            return

        if not port:
            self.handle_error(
                STATUS_BAD_REQUEST,
                "Cannot register empty port"
            )
            return

        if not signature:
            self.handle_error(
                STATUS_BAD_REQUEST,
                "Cannot register empty signature"
            )
            return

        new_address = f"{ip}:{port}"

        # Register a new peer. Note that as we aquire a mutex we need to make 
        # sure to release it in all circumstances
        network_mutex.acquire()
        try:
            if new_address in [a for a, _, _, _ in network]:
                network_mutex.release()
                self.handle_error(
                    STATUS_PEER_EXISTS,
                    f"Cannot register peer '{new_address}', already exists"
                )
                return
            
            # Don't save the given signature directly. Salt and hash it first.
            # We'll need to give this salt and signature to the other peers 
            # already on the network so if the new peer talks to them it is 
            # authorised correctly.            
            salt = get_random_salt()
            saveable_sig = assemble_signature(signature, salt)

            network.append((new_address, saveable_sig, salt, time.time()))

            # Construct a reply containing the entire network for the peer
            payload = bytearray()
            for peer, saved_signature, salt, _ in network:
                ip, port = peer.split(':')
                payload.extend(
                    bytes(ip[:LEN_IP].ljust(LEN_IP, '\x00'), 'utf-8'))
                payload.extend(struct.pack('!I', int(port)))
                payload.extend(saved_signature)
                payload.extend(bytes(salt, 'utf-8'))
            network_mutex.release()
        except Exception as e:
            network_mutex.release()
            raise e

        # Send a response
        self._build_and_send_responses(STATUS_OK, payload)
        print(f"Registered new peer {new_address}")
        print(f"Network is: {', '.join([a for a, _, _, _ in network])}")

        # Now inform the rest of the network of the new peer. Note that as we 
        # aquire a mutex we need to make sure to release it in all 
        # circumstances
        my_address = f"{self.server.ip}:{self.server.port}"
        network_mutex.acquire()
        try:
            for peer, _, _, _ in network:
                if peer not in [my_address, new_address]:
                    peer_ip, peer_port = peer.split(':')

                    msg = bytearray()
                    msg.extend(
                        bytes(ip[:LEN_IP].ljust(LEN_IP, '\x00'), 'utf-8'))
                    msg.extend(struct.pack('!I', int(port)))
                    msg.extend(signature)

                    # We don't mind if this message breaks, its just a nice to 
                    # have update so we won't bother listening for an 
                    # acknowledging reply
                    server = RemoteServer(peer_ip, int(peer_port))
                    server.send_to_server(
                        assemble_message(
                            self.server.ip, 
                            self.server.port,
                            self.server.signature,
                            COMMAND_INFORM, 
                            msg), 
                        expect_reply=False)

            network_mutex.release()
        except Exception as e:
            network_mutex.release()
            raise e
            
        return


    def _add_to_network(self, bytes_message: bytes):
        """
        Funtion to add another peer to the network. 

        bytes_message(bytes): The informing message, containg the ip, port,
            network-saved signature and salt of the peer to be added.
        """

        # Determine the different parts of the message
        ip = bytes_message[
            0
            :LEN_IP
        ].decode('utf-8').strip('\x00')
        port = struct.unpack('!I', bytes_message[
            LEN_IP
            :LEN_IP+LEN_PORT
        ])[0]
        signature = bytes_message[
            LEN_IP+LEN_PORT
            :LEN_IP+LEN_PORT+LEN_SIGNATURE
        ]
        salt = bytes_message[
            LEN_IP+LEN_PORT+LEN_SIGNATURE
            :LEN_IP+LEN_PORT+LEN_SIGNATURE+LEN_SALT
        ]

        address = f"{ip}:{port}"

        # Add peer to network. Note that as we aquire a mutex we need to make 
        # sure to release it in all circumstances
        network_mutex.acquire()
        try:
            if address in [a for a, _, _, _ in network]:
                network_mutex.release()
                return
            
            network.append((address, signature, salt, time.time()))
            print(f"Informed of new peer {address}")
            network_mutex.release()

        except Exception as e:
            network_mutex.release()
            raise e

        return


    def _handle_request(self, ip:str, port: int, signature: bytes, 
                        request:bytes):
        """
        Function to handle a 'get file' type request.

        ip(str): The IP of the requesting peer
        port(int): The port of the requesting peer
        signature(bytes): The signature of the requesting peer
        request(bytes): The file to get being requested

        A response is always generated, either the data file or an error 
        message explaining what went wrong.
        """

        # Check that the requesting peer has already registered.     
        match = False
        address = f"{ip}:{port}"
        for peer_address, peer_signature, peer_salt , _ in network:
            if peer_address == address:
                match = True

                # If we've found a matching peer in our network, check that the
                # given signature matches our previously stored one
                testable_signature = assemble_signature(signature, peer_salt)

                if peer_signature != testable_signature:
                    self.handle_error(
                        STATUS_BAD_PASSWORD,
                        f"User passwords do not match")
                    return
                break
        
        # Refuse to serve a file if the requesting peer has not already 
        # registered
        if not match:
            self.handle_error(
                STATUS_PEER_MISSING,
                f"Cannot process requests from unregistered peer at {address}")
            return            

        # Get the file path in the request
        get_path = request.decode("utf-8")
        absolute_path = os.path.abspath(get_path)

        # Do not serve files above us in the file directory
        if not absolute_path.startswith(os.getcwd()):
            self.handle_error(
                STATUS_BAD_REQUEST,
                f"Cannot access file at {get_path}")
            return
        
        # Report a request for missing data
        if not os.path.exists(absolute_path):
            self.handle_error(
                STATUS_BAD_REQUEST,
                f"Requested content {get_path} does not exist")
            return

        data = ""

        # Report request for nonsense data
        if not os.path.isfile(absolute_path):
            self.handle_error(
                STATUS_BAD_REQUEST,
                f"Request URI {get_path} is not a file")
            return

        # Check that we aren't being asked to provide a file we have not yet 
        # finished retrieving. Note that as we aquire a mutex we need to make 
        # sure to release it in all circumstances
        retrieving_mutex.acquire()
        if get_path in currently_retreiving:
            self.handle_error(
                STATUS_BAD_REQUEST,
                f"Requested content {get_path} is not yet complete.")
            retrieving_mutex.release()
            return
        retrieving_mutex.release()

        # Get file data as bytes
        with open(absolute_path) as requested_file:
            data = requested_file.read()
        if type(data) != bytes:
            data = bytes(data, "utf-8")

        # Send a response
        print(f'Sending requested data from {get_path}')
        self._build_and_send_responses(STATUS_OK, data)
        return


    def handle_error(self, status:int, msg_str: str):
        """
        Function to handle any errors that are encountered during request 
        handling and response processing. 
        
        status(int): A status code describing the error encountered.
        msg_str(str): A more descriptive response detailing exactly what went 
            wrong

        Will print a message to the server command line, and return an 
        appropriate response to the requesting client.        
        """
        print(msg_str)
 
        self._build_and_send_responses(status, bytes(msg_str, "utf-8"))

        return


    def _build_and_send_responses(self, status:int, to_send: bytes):
            """
            Function to build a response and send it.

            status(int): The response status code. Should reflect the content 
                of the message itself
            to-send(bytes): The response message body.

            The provided attributes are assembled into a bytes message 
            according to the protocol described in the handout. Various 
            attributes such as the payload length are dynamically calculated. 
            If the message is longer than the set message limit, then the 
            payload is broken into blocks and sent seperately until all blocks 
            have been sent.
            """
            # Get chechsum of the total message data to send
            total_checksum = get_sha256(to_send)
            # Calculate how long the payload can be, as we have a set limit of 
            # how many bytes can be sent, and a header that must be attatched 
            # to each message.
            sendable_length = MSG_MAX-LEN_RESPONSE_LENGTH-LEN_STATUS \
                -LEN_BLOCK_ID-LEN_BLOCKS_COUNT-LEN_BLOCK_HASH-LEN_TOTAL_HASH

            blocks_count = math.ceil(len(to_send) / sendable_length)
            this_block = 0
            blocks = []

            # handle empty message
            if len(to_send) == 0:
                blocks_count = 1
                blocks.append((0, bytes("", "utf-8")))

            # loop to determine all the blocks in the payload
            while len(to_send) > 0:

                this_payload = to_send[:sendable_length]
                blocks.append((this_block, this_payload))

                # Determine if more blocks to send
                to_send = to_send[sendable_length:]
                this_block = this_block + 1

            # Shuffle the blocks to reply in a random order
            random.shuffle(blocks)

            # loop to send one or more blocks of payload
            for block_num, block_payload in blocks:

                # Assemble an individual payload block
                payload = assemble_reply(
                    status, block_num, blocks_count, total_checksum, 
                    block_payload
                )

                print(f"Sending reply {block_num+1}/{blocks_count} with payload "
                    f"length of {len(block_payload)} bytes")

                # Send the block
                self.request.sendall(payload)


class RemoteServer(object):
    def __init__(self, ip: str, port: int):
        """
        Constructor for custom RemoteServer object

        ip(str): IP address for remote server
        port(int): Port number of remote server
        """
        self.ip = ip
        self.port = port


    def send_to_server(self, request: bytes, expect_reply=True):
        """
        Function to send a message to the defined server

        request(bytes): The message to send
        expect_reply(bool): Optional parameter to specify if we should wait for
            a response to our message or not. Default is True.

        This function assumes the server is already listening at the address 
        specified by the constructor. The provided request is sent to the 
        server and a response is always expected if expect_reply is True, even 
        in the case of errors. The response message data is always returned. In
        the case of multiple blocks of response, all blocks are combined into a
        single return value.
        """
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as conn_socket:
            # Connect and send message
            conn_socket.connect((self.ip, self.port))
            conn_socket.sendall(request)

            # If we don't expect a reply then we're done here
            if not expect_reply:
                return STATUS_OK, ""
                
            responding = True
            all_blocks = None

            # Note loop here for processing messages of several blocks. 
            # Students may wish to have two seperate implementations, one for 
            # blocked messages and one for message that can be entirely read 
            # within a single block. That is acceptable, but they should be 
            # encouraged to solve it propperly (e.g. one function for both)
            while responding:
                # Read just enough bytes to get the length of this response
                inital_read = conn_socket.recv(LEN_RESPONSE_LENGTH)
                response_length = struct.unpack('!I', inital_read)[0]

                # Now read the expected number of bytes
                response_bytes = conn_socket.recv(
                    response_length+LEN_STATUS+LEN_BLOCK_ID+LEN_BLOCKS_COUNT
                    +LEN_BLOCK_HASH+LEN_TOTAL_HASH)

                # Parse the individual message attributes
                status, this_block, total_blocks, block_checksum, \
                    total_checksum, response = \
                        parse_reply(response_bytes, response_length)

                # Setup container for all blocks to be assembled together
                if all_blocks == None:
                    all_blocks = [None] * total_blocks

                # Report any unexpected feedback
                if status != STATUS_OK:
                    msg = f"Got unexpected status code: {status}"
                    print(msg)
                    if len(response) > 500:
                            response = response[:500]
                            print("Limiting response print to 500 chars")
                    print(response)
                    all_blocks[this_block] = msg
                    break

                # Report hash inconsistencies
                if get_sha256(response) != block_checksum:
                    msg = f"Block {this_block}/{total_blocks} checksums for " \
                        "a block did not match"
                    print(msg)
                    all_blocks[this_block] = msg
                    break

                # Add this response to the all_blocks container
                all_blocks[this_block] = response

                # Report how many more blocks we are expecting
                count = len(all_blocks) - all_blocks.count(None)
                print(f"block: {this_block} ({count}/{len(all_blocks)})")

                # If we have all the blocks, then finish the loop
                if all_blocks.count(None) == 0:
                    responding = False
            
            # This means we encountered an error somewhere
            if responding:
                return STATUS_OTHER, msg
            else:
                # Check hash of all the data 
                all_data = b"".join(all_blocks)
                if get_sha256(all_data) != total_checksum:
                    print("Total checksum for all data did not match")

                return status, all_data


def to_server(my_ip: str, my_port: int, my_signature: bytes, peer_ip: str, 
              peer_port: int, code:int, request=""):
    """
    Function to send a message to another peer. This assumes that peer has 
    already setup a listening port to accept inbound communications.

    my_ip(str): The IP of the sending peer 
    my_port(int): The port of the sending peer
    my_signature(bytes): The signature of the sending peer
    peer_ip(str): The IP of the peer being sent to
    peer_port(int): The port of the peer being sent to
    code(int): The command code for the message
    request(str): The message body to send

    The payload is sent to the server, with any response either printed or
    written directly to file depending on the request. No value is returned.
    """

    print(f"Connecting to server at {peer_ip}:{peer_port}")

    # Add any file we're retrieving to a list. This will ensure we don't serve
    # an incomplete file to another peer.
    if code == COMMAND_RETREIVE:
        retrieving_mutex.acquire()
        currently_retreiving.append(request)
        retrieving_mutex.release()

    # Connect to the remote server
    server = RemoteServer(peer_ip, peer_port)
    status, data = server.send_to_server(
        assemble_message(
            my_ip, 
            my_port,
            my_signature,
            code, 
            bytes(request, "utf-8")
        )
    )

    # Once we have that file, then remove it from the list.
    if code == COMMAND_RETREIVE:
        retrieving_mutex.acquire()
        currently_retreiving.remove(request)
        retrieving_mutex.release()

    print(f"Completed server interaction code {code} with status {status}")

    # Print some feedback or write to files if appropriate. Note that as we 
    # aquire a mutex we need to make sure to release it in all circumstances
    if status == STATUS_OK:
        if code == COMMAND_REGISTER:
            network_mutex.acquire()
            try:
                for i in range(0, len(data), LEN_IP+LEN_PORT+LEN_SIGNATURE+LEN_SALT):
                    ip = data[
                        i
                        :i+LEN_IP
                    ].decode('utf-8').strip('\x00')
                    port = struct.unpack('!I', data[
                        i+LEN_IP
                        :i+LEN_IP+LEN_PORT
                    ])[0]
                    signature = data[
                        i+LEN_IP+LEN_PORT
                        :i+LEN_IP+LEN_PORT+LEN_SIGNATURE
                    ]
                    salt = data[
                        i+LEN_IP+LEN_PORT+LEN_SIGNATURE
                        :i+LEN_IP+LEN_PORT+LEN_SIGNATURE+LEN_SALT
                    ]

                    address = f'{ip}:{port}'

                    if address not in [a for a, _, _, _ in network]:
                        network.append((address, signature, salt, time.time()))

                print(f"Got network: {', '.join([a for a, _, _, _ in network])}")
                network_mutex.release()

            except Exception as e:
                network_mutex.release()
                raise e

        elif code == COMMAND_RETREIVE:
            with open(request, "wb") as binary_file:
                binary_file.write(data) 

            print(f'Retrieved data written to {request}')

        else:
            print(f"Got response: {data.decode('utf-8')}")


def assemble_message(ip: str, port: int, signature: bytes, code:int, 
                     request: bytes):
    """
    Funtion to create a message out of the provided arguments.
    
    ip(str): IP of sending peer
    port(int): Port of the sending peer
    signature(bytes): Signature of the sending peer
    code(int): A command code describing the message being sent
    request(bytes): The message body to be sent
    
    A bytearray of the necessary parts is assembled and returned. 
    """    
    payload = bytearray()
    payload.extend(bytes(ip[:LEN_IP].ljust(LEN_IP, '\x00'), 'utf-8'))
    payload.extend(struct.pack('!I', port))
    payload.extend(signature)
    payload.extend(struct.pack('!I', code))
    payload.extend(struct.pack('!I', len(request)))
    payload.extend(request)

    return payload


def parse_message(bytes_message: bytes):
    """
    Function to parse a recieved message

    bytes_message(bytes): The message to parse.

    Individual elements are extracted and returned as a tuple.
    """
    ip = bytes_message[
        0
        :LEN_IP
    ]
    port = struct.unpack('!I', bytes_message[
        LEN_IP
        :LEN_IP+LEN_PORT
    ])[0]
    signature = bytes_message[
        LEN_IP+LEN_PORT
        :LEN_IP+LEN_PORT+LEN_SIGNATURE
    ]
    code = struct.unpack('!I', bytes_message[
        LEN_IP+LEN_PORT+LEN_SIGNATURE
        :LEN_IP+LEN_PORT+LEN_SIGNATURE+LEN_COMMAND_LENGTH
    ])[0]
    payload_length = struct.unpack('!I', bytes_message[
        LEN_IP+LEN_PORT+LEN_SIGNATURE+LEN_COMMAND_LENGTH
        :LEN_IP+LEN_PORT+LEN_SIGNATURE+LEN_COMMAND_LENGTH+LEN_REQUEST_LENGTH
    ])[0]
    payload = bytes_message[
        LEN_IP+LEN_PORT+LEN_SIGNATURE+LEN_COMMAND_LENGTH+LEN_REQUEST_LENGTH
        :LEN_IP+LEN_PORT+LEN_SIGNATURE+LEN_COMMAND_LENGTH+LEN_REQUEST_LENGTH+payload_length
    ]

    ip = ip.decode('utf-8').strip('\x00')

    return ip, port, signature, code, payload_length, payload


def assemble_reply(status: int, this_block: int, blocks: int, 
        total_checksum: bytes, reply: bytes):
    """
    Function to assemble reply message.
    
    status(int): Status code of the response
    this_block(int): The identifying number of this block
    blocks(int): The totol number of blocks to send for this entire reply to be
        sent
    total_checksum(bytes): A checksum of the entire data to send
    reply(bytes): The actual message body itself

    Individual elements are combined and returned as a byte array. Note that 
    that checksum for his block as well as the message length are automatically
    calculated.
    """
    payload = bytearray()
    payload.extend(struct.pack('!I', len(reply)))
    payload.extend(struct.pack('!I', status))
    payload.extend(struct.pack('!I', this_block))
    payload.extend(struct.pack('!I', blocks))
    payload.extend(get_sha256(reply))
    payload.extend(total_checksum)    
    payload.extend(reply)

    return payload


def parse_reply(bytes_message: bytes, response_length: int):
    """
    Function to parse a reply message.

    bytes_message(bytes): The entire message to parse
    response_length(int): The length of just the message body

    Individual elements are extracted and returned as a tuple.
    """
    status = struct.unpack('!I', bytes_message[
        0
        :LEN_STATUS
    ])[0]
    this_block = struct.unpack('!I', bytes_message[
        LEN_STATUS
        :LEN_STATUS+LEN_BLOCK_ID
    ])[0]
    total_blocks = struct.unpack('!I', bytes_message[
        LEN_STATUS+LEN_BLOCK_ID
        :LEN_STATUS+LEN_BLOCK_ID+LEN_BLOCKS_COUNT
    ])[0]
    block_checksum = bytes_message[
        LEN_STATUS+LEN_BLOCK_ID+LEN_BLOCKS_COUNT
        :LEN_STATUS+LEN_BLOCK_ID+LEN_BLOCKS_COUNT+LEN_BLOCK_HASH
    ]
    total_checksum = bytes_message[
        LEN_STATUS+LEN_BLOCK_ID+LEN_BLOCKS_COUNT+LEN_BLOCK_HASH
        :LEN_STATUS+LEN_BLOCK_ID+LEN_BLOCKS_COUNT+LEN_BLOCK_HASH
            +LEN_TOTAL_HASH
    ]
    response = bytes_message[
        LEN_STATUS+LEN_BLOCK_ID+LEN_BLOCKS_COUNT+LEN_BLOCK_HASH
            +LEN_TOTAL_HASH
        :LEN_STATUS+LEN_BLOCK_ID+LEN_BLOCKS_COUNT+LEN_BLOCK_HASH
            +LEN_TOTAL_HASH+response_length
    ]

    return status, this_block, total_blocks, block_checksum, \
        total_checksum, response


def run_client(my_ip, my_port, my_signature):
    """
    Function to automate client-side interactions in an easilly repeatable 
    manner

    my_ip(str): IP address for this peer
    my_port(int): Port for this peer
    my_signature(bytes): Signature for this peer

    This function is used as a thread definition to run the client side of the
    peers interactions. It asks for user input on which IP and port to connect
    to, in order to register with a pre-established network. Once registration 
    has been completed the client will enter an infinite loop of prompting the 
    user for files to retrieve. Each of these file requests will be sent to a
    random peer on the network that is not the requesting peer.
    """
    peer_ip = input("Peer IP: ")
    peer_port = int(input("Peer Port: "))

    to_server(
        my_ip, my_port, my_signature, peer_ip, peer_port, COMMAND_REGISTER
    )

    network_mutex.acquire()
    try:
        if not len(network):
            print(f"No network was detected, shutting down client thread.") 
            return
    except Exception as e:
        network_mutex.release()
        print(f"Something went drastically wrong. {e}")
        exit()
    network_mutex.release()

    while 1:
        print("Type the name of a file to be retrieved, or 'quit' to quit:")    
        user_input = input()
        if user_input == 'quit':
            print(f"Shutting down client thread.")
            exit()
        else:
            target_ip, target_port = get_random_peer(my_ip, my_port)
            to_server(
                my_ip, my_port, my_signature, target_ip, target_port, 
                COMMAND_RETREIVE, user_input
            )


def get_random_peer(my_ip, my_port):
    """
    Function to select a random peer from the network

    my_ip(str): IP address of the selecting peer
    my_port(int): Port of the selecting peer
     
    A peer is randomly selected from all those on the network that is not the 
    selecting peer. The selected IP and port are returned as a tuple.
    """
    potential_peers = []
    my_address = f"{my_ip}:{my_port}"

    # Note that as we aquire a mutex we need to make sure to release it in all
    # circumstances
    network_mutex.acquire()
    try:
        for peer in [a for a, _, _, _ in network]:
            if peer != my_address:
                potential_peers.append(peer)

        network_mutex.release()
    except Exception as e:
        network_mutex.release()
        raise e
    
    peer = random.choice(potential_peers).split(':')

    return peer[0], int(peer[1])


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "my_ip",
        help="IP address to host this peer on. This will be passed to other "
        "peers. e.g. 123.123.123.123")
    parser.add_argument(
        "my_port",
        help="Port to host this peer on. This will be passed to other peers. "
        "e.g.: 12345")    
    parser.add_argument(
        "-d",
        help="Optional flag to prompt the server side of the peer to print " \
        "out its progress.", action="store_true")
    args = parser.parse_args()

    if args.d:
        print("Server thread will print to stdout")
    else:
        print("Server thread printing suppressed")

    password = input("Create password for this peer: ")

    # Generate a salt to be applied to our password. This _should_ be random 
    # but for initial development you might find it easier to debug with a 
    # hard-coded value, in which case uncomment the following line and comment
    # out the line after that
    #salt = "0123456789ABCDEF"
    salt = get_random_salt()
    
    signature = assemble_signature(password, salt)

    network.append((f"{args.my_ip}:{int(args.my_port)}", signature, salt, -1))

    # Start client interaction. Note that we do not wait for this thread
    # to complete before moving on to starting the server, both should be 
    # running at the same time.
    mythread = threading.Thread(
        target=run_client, 
        args=[args.my_ip, int(args.my_port), signature],
        daemon=True)
    mythread.start()

    # Start the serving thread as well, which should also run forever
    with PeerToPeerServer(args.my_ip, int(args.my_port), signature, RequestHandler, args.d) \
            as peer_to_peer_server:
        try:
            peer_to_peer_server.serve_forever()
        finally:
            peer_to_peer_server.server_close()
