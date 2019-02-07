# Iterative File Transfer TCP Server
This code is developed for the Distributed Programming I course held at the Politecnico of Torino.

# How it works:

The TCP sequential server (listening to the port specified as the first parameter of the command line, as a decimal integer), after having established a TCP connection with a client, accepts file transfer requests from the client and sends the requested files back to the client, following the protocol specified below. 
The files available for being sent by the server are the ones accessible in the server file system from the working directory of the server. 
The developed client can connect to a TCP server (to the address and port number specified as first and second command-line parameters, respectively). After having established the connection, the client requests the transfer of the files whose names are specified on the command line as third and subsequent parameters, and stores them locally in its working directory. After having transferred and saved locally a file, the client print a message to the standard output about the performed file transfer, including the file name, followed by the file size (in bytes, as a decimal number) and timestamp of last modification (as a decimal number).

The protocol for file transfer works as follows: to request a file the client sends to the server the three ASCII characters “GET” followed by the ASCII space character and the ASCII characters of the file name, terminated by the ASCII carriage return (CR) and line feed (LF):
