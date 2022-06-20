CC = gcc
CXX = g++
INCLUDE_OPENCV = `pkg-config --cflags --libs opencv`

RECEIVER = receiver.cpp
SENDER = sender.cpp
AGENT = agent.c
RECV = receiver
SEND = sender
AG = agent

all: sender receiver agent
  
sender: $(SENDER)
	$(CXX) $(SENDER) -o $(SEND) $(INCLUDE_OPENCV)
receiver: $(RECEIVER)
	$(CXX) $(RECEIVER) -o $(RECV) $(INCLUDE_OPENCV)
agent: $(AGENT)
	$(CC) $(AGENT) -o $(AG) 

.PHONY: clean

clean:
	rm $(RECV) $(SEND) $(AG)
