# CompSys A3 solution README
This system is considerably more complicated to run and understand than 
previous assignment handouts. In order to properly test your system you will 
need to start different peers in varying orders and states. Multiple peers will
need to started at the same time in some cases. To aid this two 
identical Python peers have been provided. Do note that these are identical
copies of the same file, so any change you apply to one will not automatically
be applied to the other.

You may add additional peers as you wish, either a third (or more) python peer
or multiple copies of your C implementation. 

## To run a Python peer from within either the *python/first_peer* directory or *python/second_peer* directory:
    python3 ./peer.py <IP> <PORT>

for example:

    python3 ./peer.py 127.0.0.1 12345

## To compile the C peer from within the *src* directory:
    make

## To run C peer from within the *src* directory:
    ./peer <IP> <PORT>

for example:

    ./peer 127.0.0.1 12345

## To clean C peer from within the *src* directory:
    make clean

## To check correctness of results from within the *src* directory:
    diff tiny.txt ../python/first_peer/tiny.txt && diff hamlet.txt ../python/first_peer/hamlet.txt

The above command should produce no output if everything has worked correctly
