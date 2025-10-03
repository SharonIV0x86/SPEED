# SPEED
Secure Process-To-Process Encrypted Exchange and Delivery.


## Work In Progress
Currently we are focusing on the C++ binding for the MVP and then we will move to Java.

### To compile C++ Code.
Move to ``speed-cpp`` folder.
```sh
cd speed-cpp
```
Compile the ``main.cpp`` and run the ``app`` binary.
```sh
g++ -Iinclude main.cpp src/*.cpp -o app
```
If you want to contribute make sure the code is clang formatted using.
```sh
clang-format -i src/* && clang-format -i include/*
```

## Identified Tasks
Below are some identified tasks which need to be done.
- [ ] Add logic for traversing ``speed_dir_/access_registry``.
- [ ] Add logic for checking for existence and creation of ``access_registry`` folder.

# Process Arrival/Startup Flow for the first time
Below i have explained all the steps in great detail.
Assume that the process here in the picture is ``P1``.
## Step 1
This is the first step in which the folder of the current process is created for communication. Assume that the name of the process itself is ``P1``. So whenever created for the first time the process will try to create the ``P1`` folder in the specified ``SPEED`` directory. If the process folder with the same process already exists it will be deleted and a new one will be created.

## Step 2
Once the process folder is created in ``step 1`` we have to build the global registry. Traverse the whole ``access_registry`` folder and pick up the files strictly ending with ``.oregistry`` and add them to the ``global-registry``. 

## Step 3
Now the current process P1 itself has to put its access file in the ``access_registry``. So it puts a file named ``P1.iregistry`` intermediate file, and writes the name of the process ``P1`` to that file. Once the writing stage it completed the registry file is renamed to ``P1.oregistry`` indicating that the file has been written and can be processed. 
## Step 4
In this step ``P1`` compares its ``access_list_`` with the ``global_registry_`` and validates whether the process names specified in the ``access_list_`` are available to be connected in the ``global_registry_`` or not.
##  Step 5
Now in this step we actually send connection request to the processes which are available in both the ``access_list_`` and the ``global_registry_``. The process ``P1`` puts connection request message with type ``CON_REQ`` in the respective folders of all the processes which are specified in the ``access_list_`` and are also in the ``global_registry_``.
## Step 6
Once the connection requests are sent, the ``P1`` waits for the ``CON_ACPT`` from processes. Assume that the ``P1`` sends connection request ``CON_REQ`` to process ``P2`` now when ``P2`` recieves this connection request ``P2`` first checks whether ``P1`` is specified in its ``access_list_`` or not and if is, the ``P2`` sends out a ``CON_ACPT``.
## Step 7 
After the ``P1`` sends out the connection requests it waits for 500ms to get all the ``CON_ACPT`` from all the processes. And as it gets ``CON_ACPT`` from the processes they also gets put into the ``connected_list_`` of ``P1`` process indicating that the connection is successful.
## Step 8 (final)
Now once the connections are established the communication can finally take place.

# Process Message Sending Flow.
Assume that ``P1`` process wants to send message to ``P2``, ``P3`` and ``P4``.
## Step 1
User or Dev calls the function

```cpp
speed.sendMessage("Hello", ["P2", "P3"]);
```

Now the SPEED will try to send the message ``"Hello"`` to both ``P2`` and ``P3``.
## Step 1
Now the main thing here is to validate whether the processes P2 and P3 are available to connect or not and whether we have access of them are not.

The first thing that happens is that the SPEED checks with the ``global_registry_`` to ensure that the P2 and P3 processes have arrived in the ``global_registry_`` or not.
If they are found in the ``global_registry_`` then well and good and if not then next things happen in ``step 2``
## Step 2
Assume that ``P2`` is in the ``global_registry_`` but ``P3`` is not. This can most likely means that when ``P3`` arrived ``P1``(current process) was not notified. So the ``global_registry_`` snapshot that ``P1`` has is outdated. 
So ``P1`` again traverses the ``access_registry`` folder and incrementally builds its ``global_registry_``. And assume that ``P1`` finds ``P3`` in the global registry. 

## Step 3
So ``P3`` is available to be connected but its not connected yet, so ``P1`` sends out a connection request (``CON_REQ``) to ``P3``. Now assume that ``P3`` accepts the connection request and sends a ``CON_ACPT`` signal to ``P1``. Now the ``P1`` and ``P3`` are both connection and ``P1`` adds the ``P3`` to its ``connected_list_``. And in the vice versa ``P3`` adds ``P1`` to its ``connected_list_``.

## Step 4
Now at this stage both ``P2`` and ``P3`` are available to be sent out the messages to.
So that's what happens ``P1`` sends out the ``"Hello"`` message to both ``P2`` and ``P3`` and they recieve it and process it.