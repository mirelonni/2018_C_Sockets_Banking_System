# Banking System on Unix Sockets

I have implemented a rudimentary banking system in C using sockets and multiplexing the input, between stdin, UDP and TCP.
The normal use case of commands are sent through TCP and the unlocking of the card is done through UDP.

If the response is not the expected one the server sends to the client the following error codes:

* -1 : Client not logged in
* -2 : Session already open
* -3 : Wrong pin
* -4 : Not existing card number
* -5 : Card locked
* -6 : Operation failed
* -7 : Unlock failed
* -8 : Insufficient funds
* -9 : Operation canceled
* -10 : Function error

The commands that you can give to the server from the different clients(sometimes referred in the comments as terminals) are as follows:

1.  
  - __login <cardNumber> <pin>__
  - cardNumber should be a valid one if not the server will return the "-4" error code.
  - If the pin is wrong the server will return the "-3" error code.
  - If the client inputs the wrong pin 3 times in a row on the same account the server locks the card and return the "-5" error code, even if the correct pin in inputted.
  - If a client is already logged in on that terminal the terminal returns the "-2" error code.
  - If the client is already logged in on a terminal the server returns the "-2" error code.
2.  
  - __logout__
  - For the operation to work the client needs to be logged in, otherwise the server will return "-1" error code.
3.  
  - __listbalance__
  - For the operation to work the client needs to be logged in, otherwise the server will return "-1" error code.
4.  
  - __transfer <cardNumberWhereToTransfer> <sum>__
  - cardNumberWhereToTransfer should be a valid one if not the server will return the "-4" error code.
  - If there are enough funds from the account that initializes the transfer the server will ask for a confirmation, other wise the server will return the "-8" error code.
  - __<[y/n]response>__
  - At the confirmation message the client has to chose if they proceed by entering [y/n], if the client types anything that doesn't start with 'y' the server considers that the answer is 'n' and returns the "-9" error code.
5.  
  - __unlock__
  - The server receives "unlock <cardNumber>" the cardNumber being the last cardNumber entered on said terminal.
  - If the card is not locked the server returns the "-6" error code.
  - __<secretPassword>__
  - The server receives "<cardNumber> <secretPassword>" the cardNumber being the last cardNumber entered on said terminal.
  - If the secretPassword is the correct one the server unlocks the cardNumber, otherwise the server returns the "-7" error code and resets the unlock attempt.
6.  
  - __quit__
  - quit can also be a server command which switches of all the terminals.

The "user_data_file" is structured as follows:

_N_ - the number of clients in that file

_firstName_ _lastName_ _cardNumber_ _pin_ _secretPassword_ _balance_ - client information

```bash
make
./server <port_server> <user_data_file>

./client <IP_server> <port_server>
./client <IP_server> <port_server>
...
```
