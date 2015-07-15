// EtherTest.cpp : Defines the entry point for the console application.
//

#define WIN32_LEAN_AND_MEAN

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <conio.h>

#define INTERNAL_BUFFER_SIZE (8*1024)
#define MSG_SIZE             (2*sizeof(int))

static void Usage(char const *cmd = NULL);

static void sendOp(int sock, char const *host, const char *port, const char *size_txt);
static void hearOp(int sock, char const *host, const char *port, const char *size_txt);
static void saidOp(int sock, char const *host, const char *port, const char *size_txt);
static void echoOp(int sock, char const *host, const char *port);
static bool getAddressInformation(const char *ip_address, const char * port, ADDRINFO* &ai);
static int  getMsg(const char *buffer, int size);
static int  MySend(int sock, char const *buf, int len, int flag);
static int  ProcessSwitch(int &argc, const char* argv[]);
bool getStats();

static int max_buffer = 0x7FFFFFFF;

int main(int argc, const char* argv[])
{
    WSADATA wsaData;
    int err = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (err != NO_ERROR)
    {
        printf("WSA start up error %d\r\n", err);
    }

    ProcessSwitch(argc, argv);

    int sock = -1;
    
    if (argc < 2)
    {
        Usage();
    }
    else if (!strcmp(argv[1], "-i"))
    {
        printf("EtherTest 1.0.0\r\n");
    }
    else if (!strcmp(argv[1], "-h"))
    {
        Usage(argv[2]);
    }
    else if (!strcmp(argv[1], "--help"))
    {
        Usage();
    }
    else if (argc < 4)
    {
        Usage(argv[2]);
    }
    else if (!stricmp(argv[1], "send"))
    {
        if (INVALID_SOCKET == (sock = socket(PF_INET, SOCK_STREAM, IPPROTO_IP)))
        {
            printf("Failed to create the socket\r\n");
        }
        else
        {
            sendOp(sock, argv[2], argv[3], argv[4]);
        }
    }
    else if (!stricmp(argv[1], "hear"))
    {
        if (INVALID_SOCKET == (sock = socket(PF_INET, SOCK_STREAM, IPPROTO_IP)))
        {
            printf("Failed to create the socket\r\n");
        }
        else
        {
            hearOp(sock, argv[2], argv[3], argv[4]);
        }
    }
    else if (!stricmp(argv[1], "said"))
    {
        if (INVALID_SOCKET == (sock = socket(PF_INET, SOCK_STREAM, IPPROTO_IP)))
        {
            printf("Failed to create the socket\r\n");
        }
        else
        {
            saidOp(sock, argv[2], argv[3], argv[4]);
        }
    }
    else if (!stricmp(argv[1], "echo"))
    {
        if (INVALID_SOCKET == (sock = socket(PF_INET, SOCK_STREAM, IPPROTO_IP)))
        {
            printf("Failed to create the socket\r\n");
        }
        else
        {
            echoOp(sock, argv[2], argv[3]);
        }
    }
    else
    {
        // Unknown command
        Usage(argv[1]);
    }
    
    if (sock >= 0) closesocket(sock);

    WSACleanup();
	
    printf("Done\r\n");
    char ch; scanf("%c", &ch);

	return 0;
}


static void sendOp(int sock, char const *host, const char *port, const char *size_txt)
{
    ADDRINFO *ai = NULL;
    int size = 10000;
    int err;

    if (size_txt != NULL)
    {
        // Get the user specified size
        size = atoi(size_txt);
    }

    if (!getAddressInformation(host, port, ai))
    {
        printf("Illegal address %s:%s\r\n", host, port);
    }
    else if ((err = connect(sock, ai->ai_addr, (int)ai->ai_addrlen)) == SOCKET_ERROR)
    {
        printf("Failed to connect to %s:%s [%d]\r\n", host, port, err);
    }
    else
    {
        // Build a block of data to send
        char *buffer = (char*)malloc(size);
        for (int i = 0; (i < size); i++)
        {
            buffer[i] = (char)i;
        }

        // Send the data
        if (size != MySend(sock, buffer, size, 0))
        {
            printf("Not all sent\r\n");
        }
        printf("> %d\r\n", size);
        getStats();

        // Read the responce
		int rsvp;
		int total = 0;
		do 
		{
			if (0 > (rsvp = recv(sock, buffer+total, size-total, 0)))
			{
				printf("Error on read [%d]\r\n", rsvp);
			}
			else
			{
				total += rsvp;
				printf("< %d\r\n", rsvp);
				getStats();
			}
		}
		while ((rsvp > 0) && (total < size));

        // Check the size of the responce
        if (size != total)
        {
            printf("Not all received\r\n");
        }

        // Check the data comes back
        for (int i = 0; (i < total); i++)
        {
            if (buffer[i] != (char)i)
            {
                printf("Data corrupt\r\n");
                break;
            }
        }
    }

    if (ai != NULL) freeaddrinfo(ai); // Free the address info chain.
}


static void hearOp(int sock, char const *host, const char *port, const char *size_txt)
{
    ADDRINFO *ai = NULL;
    int size = 10000;
    int err;

    if (size_txt != NULL)
    {
        size = atoi(size_txt);
    }

    if (!getAddressInformation(host, port, ai))
    {
        printf("Illegal address %s:%s\r\n", host, port);
    }
    else if ((err = connect(sock, ai->ai_addr, (int)ai->ai_addrlen)) == SOCKET_ERROR)
    {
        printf("Failed to connect to %s:%s [%d]\r\n", host, port, err);
    }
    else
    {
        // Build a request message
        char *msg = (char*)malloc(MSG_SIZE);
		((int*)msg)[0] = 0xDEADBEEF;
		((int*)msg)[1] = size;

        // Send the message
        if (2*sizeof(int) != MySend(sock, msg, MSG_SIZE, 0))
        {
            printf("Not all sent\r\n");
        }
        printf("> %d\r\n", 2*sizeof(int));
        getStats();
				
        char *buffer = (char*)malloc(size);

        // Read the responce
		int rsvp;
		int total = 0;
		do 
		{
			if (0 > (rsvp = recv(sock, buffer+total, size-total, 0)))
			{
				printf("Error on read [%d]\r\n", rsvp);
			}
			else
			{
				total += rsvp;
				printf("< %d\r\n", rsvp);
				getStats();
			}
		}
		while ((rsvp > 0) && (total < size));

        // Check the size
        if (size != total)
        {
            printf("Not all received\r\n");
        }

        // Check the data comes back
        for (int i = 0; (i < size); i++)
        {
            if (buffer[i] != (char)i)
            {
                printf("Data corrupt\r\n");
                break;
            }
        }

        free(buffer);
    }
    
    if (ai != NULL) freeaddrinfo(ai); // Free the address info chain.
}


static void saidOp(int sock, char const *host, const char *port, const char *size_txt)
{
    ADDRINFO *ai = NULL;
    int size = 10000;
    int err;

    if (size_txt != NULL)
    {
        size = atoi(size_txt);
    }

    if (!getAddressInformation(host, port, ai))
    {
        printf("Illegal address %s:%s\r\n", host, port);
    }
    else if ((err = connect(sock, ai->ai_addr, (int)ai->ai_addrlen)) == SOCKET_ERROR)
    {
        printf("Failed to connect to %s:%s [%d]\r\n", host, port, err);
    }
    else
    {
        // Build a request message
        char *msg = (char*)malloc(MSG_SIZE);
		((int*)msg)[0] = 0xDEAFBABE;
		((int*)msg)[1] = size;

        // Send the message
        if (2*sizeof(int) != MySend(sock, msg, MSG_SIZE, 0))
        {
            printf("Not all sent\r\n");
        }
        printf("> %d\r\n", 2*sizeof(int));
        getStats();
				
        char *buffer = (char*)malloc(size);
        for (int i = 0; (i < size); i++)
        {
            buffer[i] = (char)i;
        }

        // Send the data
        printf("> %d\r\n", size);
        if (size != MySend(sock, buffer, size, 0))
        {
            printf("Not all sent\r\n");
        }
        getStats();

        free(buffer);
    }
    
    if (ai != NULL) freeaddrinfo(ai); // Free the address info chain.
}


static void echoOp(int sock, char const *host, const char *port)
{
    ADDRINFO *ai = NULL;
    int err;

    // Clear any input
    while (kbhit()) getch();

    if (!getAddressInformation(host, port, ai))
    {
        printf("Illegal address %s:%s\r\n", host, port);
    }
    else if ((err = bind(sock, ai->ai_addr, (int)ai->ai_addrlen)) == SOCKET_ERROR)
    {
        printf("Failed to bind the socket to %s:%s [%d]\r\n", host, port, err);
    }        
    else if ((err = listen(sock, 1)) == SOCKET_ERROR) 
    {
        printf("Failed to listen on the socket [%d]\r\n", err);
    }
    else
    {
        // Wait for the user to terminate the server
        while(!kbhit())
        {
            // Accept a client connection

            SOCKADDR_STORAGE from;
            int from_length = sizeof(from);
            int rsvp = accept(sock, (LPSOCKADDR)&from, &from_length);

            // Check for a valid connection
            if (rsvp == INVALID_SOCKET)
            {
                printf("Failed to accept connection\r\n");
            }
            else
            {
                int   size = 0;
                int   request;
                char *msg;

                static char buffer[INTERNAL_BUFFER_SIZE];

                // Wait for a key to be pressed
                bool more = true;
                while(more)
                {
                    size = recv(rsvp, buffer, INTERNAL_BUFFER_SIZE, 0);getStats();
                    printf("< %d\r\n", size);

                    // Is this a request message
                    switch(getMsg(buffer, size))
                    {
                    default:
                    case 0:
                        if (size > 0)
                        {
                            // Echo the data back
                            printf("> %d\r\n", size);
                            if (size != MySend(rsvp, buffer, size, 0))
                            {
                                printf("Not all sent\r\n");
                            }
                            getStats();
                        }
					    else
					    {
                            // Brocken connection
                            more = false;
					    }
                        break;

                    case 0xDEADBEEF:
						request = ((int*)buffer)[1];

                        // Build a block of data to send
						msg = (char*)malloc(request);
                        for (int i = 0; (i < request); i++)
                        {
                            msg[i] = (char)i;
                        }

                        // Send the data
                        printf("HEAR> %d\r\n", size);
                        if (request != MySend(rsvp, msg, request, 0))
                        {
                            printf("Not all sent\r\n");
                        }
                        getStats();

                        free (msg);
                        break;

                    case 0xDEAFBABE:
						request = ((int*)buffer)[1];

                        // Build a block of data to read
						msg = (char*)malloc(request);
                        size = recv(rsvp, msg, request, 0);getStats();
                        printf("SAID< %d\r\n", size);

                        // Check the size
                        if (size != request)
                        {
                            printf("Not all received\r\n");
                        }

                        // Check the data comes back
                        for (int i = 0; (i < size); i++)
                        {
                            if (msg[i] != (char)i)
                            {
                                printf("Data corrupt\r\n");
                                break;
                            }
                        }

                        free (msg);
                        break;
                    }

                    // Check if the user has pressed a key
                    if (kbhit())
                    {
                        more = false;
                    }
                }

                closesocket(rsvp);
            }
        }
    }
    
    if (ai != NULL) freeaddrinfo(ai); // Free the address info chain.
}

static int getMsg(const char *buffer, int size)
{
	if ((size >= 2*sizeof(int)) && (buffer[0] != 0x00) && (buffer[1] != 0x01) && (buffer[2] != 0x02) && (buffer[3] != 0x03))
    {
        return ((int*)buffer)[0];
    }

    return 0;
}


static int MySend(int sock, char const *buf, int len, int flag)
{
    int idx = 0;
    int out;

    while (idx < len)
    {
        int out = (len-idx);

        if (out > max_buffer)
        {
            out = max_buffer;
        }

        if (0 >= (out = send(sock, buf+idx, out, 0)))
        {
            break;
        }

        if ((len-idx) > max_buffer)
        {
            int time = clock();
            while ((clock() - time) < 1);
        }

        idx += out;
    }

    return idx;
}



static void Usage(char const *cmd)
{
    if (cmd == NULL)
    {
        printf("Usage -i\r\n");
        printf("Usage -h [<cmd>]\r\n");
        printf("Usage echo port 127.0.0.1 [-b <size>]\r\n");
        printf("Usage send port 127.0.0.1 [<size>] [-b <size>]\r\n");
        printf("Usage hear port 127.0.0.1 [<size>] [-b <size>]\r\n");
        printf("Usage said port 127.0.0.1 [<size>] [-b <size>]\r\n");
        printf("\r\n");
        printf("   -i         : Application version.\r\n");
        printf("   -h [<cmd>] : Application help.\r\n");
        printf("   -b <size>  : This sets the maximum size of a write. All larger block transfers\r\n");
        printf("                are brocken down into writes of this size with a 1ms pause between.\r\n");
        printf("\r\n");
    }
    else if (!stricmp(cmd, "echo"))
    {
        printf("Usage echo port 127.0.0.1 [-b <size>]\r\n");
        printf("\r\n");
        printf("The 'echo' command creates a server process that responds\r\n");
        printf("to client requests. Only one connection at a time is permitted.\r\n");
        printf("But when the client disconnects then a new connection can be\r\n");
        printf(" made.\r\n");
        printf("\r\n");
        printf("Refer to the client commands to see what operations are\r\n");
        printf("permitted.\r\n");
    }
    else if (!stricmp(cmd, "send"))
    {
        printf("Usage send port 127.0.0.1 [<size>] [-b <size>]\r\n");
        printf("The 'send' command is a client operation. The client sends a\r\n");
        printf("block of data to the server that then echos it back as it is\r\n");
        printf("received. The received data is checked for size and content.\r\n");
        printf("\r\n");
        printf("If the size is not specified then 10000 bytes are sent.\r\n");
    }
    else if (!stricmp(cmd, "hear"))
    {
        printf("Usage hear port 127.0.0.1 [<size>] [-b <size>]\r\n");
        printf("The 'hear' command is a client operation. The client sends a\r\n");
        printf("request for data to the server. The server then sends the data\r\n");
        printf("as a single block of data. The client the check the received\r\n");
        printf("data for size and content\r\n");
    }
    else if (!stricmp(cmd, "said"))
    {
        printf("Usage said port 127.0.0.1 [<size>] [-b <size>]\r\n");
        printf("The 'said' command is a client operation. The client sends a\r\n");
        printf("message to the server telling it not to echo a the block of data.\r\n");
        printf("The client then sends the data as a single block of data.\r\n");
    }
    else
    {
        printf("Unknown  command [%s]\r\n\r\n", cmd);
        Usage();
    }
}

// Any call to this function should have a resulting call to freeaddrinfo if true is returned.
static bool getAddressInformation(const char *ip_address, const char * port, ADDRINFO* &ai)
{
    ADDRINFO hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;

    const int result = getaddrinfo(ip_address,
                                   port,
                                   &hints,
                                   &ai);

    return (result == 0);
}



static int ProcessSwitch(int &argc, const char* argv[])
{
    const char **in  = argv+1;
    const char **out = argv+1;

    while (*in != NULL)
    {
        if (!strcmp(*in, "-b"))
        {
            in   += 1;
            argc -= 1;

            if (*in != NULL)
            {
                max_buffer = atoi(*in);
                in   += 1;
                argc -= 1;
                
                printf("Max write %d\r\n", max_buffer);
            }
        }
        else
        {
            *(out++) = *(in++);
        }
    }
    *out = *in;

    return argc;
}



// Read the number of retransmitted segments from the TCP layer
bool getStats()
{
    MIB_TCPSTATS TCPStats;
    DWORD dwRetVal = 0;

#if 1
    if ((dwRetVal = GetTcpStatistics(&TCPStats)) == NO_ERROR) 
    {
        printf("Ret: %d : Err: %d \r\n", TCPStats.dwRetransSegs, TCPStats.dwInErrs);
        return true;
    }
    else 
    {
        printf("Failed to get the connection stats \r\n");
        return false;
    }
#else
	return true;
#endif
}

