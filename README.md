# gRPC Streaming State Machine

This is a simple example of a finite state machine that receives actions from
a remote machine (client), and streams its state transitions to the actor.

The example is intended to demonstrate how clients can get updates from an
asynchronous FSM that is performing possibly-long-running tasks.

# How does it work?

The server implements an asynchronous service that is providing an API to access
a (possibly asynchronous) finite state machine (FSM). The client makes an
asynchronous call to a function/API on the server. This is the *action* that
starts the processing, a chain of transitions, on the server, and ultimately on
the FSM that the server is controlling.

## States
FSM that the server controls can be in one state at any given point in time. In
this example, the FSM has the following 5 states:
1. Initial: Both the client and the server start with the intial state.
2. Setup: The second step; carries no special meaning.
3. DataUpload: The third step; carries no special meaning.
4. DataVerify: The fourth step; carries no special meaning.
5. DataReady: The fifth, and the final step; it signifies that the server has
  completed its processing and the result is the final reply to the inital
  function call. After this state, streaming will finish.

The last member, `NumStates`, is just a placeholder to make it easier to count
total number of states. This assumes that states are contiguous, monotonically
increasing sequences. C, and C++ enums satisfy this requirement. (Aside: the
underlying value of enumerations is passed to gRPC as `uint64`, which is a
larger integer type than the underlying type of this enum, so it should be OK to
do conversions, but bounds checking is still a good habit).

## Transitions
A *transition* is a state change on the server side. That is, when server's
state changes from a given state *A* to *B*.

Server *streams* its state changes (transitions) to the client. Streaming will
continue until both parties deduce that it is the end of the stream. Each
transition is accompanied by its status code. This enables reporting failures,
as well as successes.

The end of the stream can be deduced by either checking whether the FSM has
reached its final state and there are no more possible transitions left, or an
unrecoverable error occured during one of the transitions before the FSM
finalizes its successful operation.

Client's role is to observe (in this example, just print) the transitions of a
remote FSM that it has initiated.

## Making the function call
Both the client and the server starts at an intial state. In this example, that
state is also named `Initial`. The call is asynchronously made by the client,
and handled by the server.

Client makes an asynchronous gRPC call, and performs one read from the
completion queue. From that point, the client will expect a reply from the
server.

## Responding the function call
The server starts its FSM. It reports how many transitions it has, and
transitions to the next step.

This is kind of meaningless in this example. Please note that the example has a
a fixed number of steps that are specified in an enum, but this is not a
requirement imposed by gRPC. It is done merely to show that the FSM state can be
streamed, and client can observe knows set of states. States can be dynamically
set, if necessary.

When server receives a request while it is in `Initial` state, it might choose
to record the request id so that both parties can easily discriminate individual
requests.

## Streaming the rest of the states from the server
The server streams its transitions (periodically, in this case) until it reaches
the final state, at which it calls `Finish` on the responder.

The server then starts cleaning up resource that are used during handling the
function call.

## Receiving the last state on the client
When client receives the last transition, it also calls `Finish`.

The client then starts cleaning up resources that are used during making the
function call.

# Performance and resource usage
The official gRPC performance guide says:

  Do not use Sync API for performance sensitive servers. If performance and/or
  resource consumption are not concerns, use the Sync API as it is the simplest
  to implement for low-QPS services.

