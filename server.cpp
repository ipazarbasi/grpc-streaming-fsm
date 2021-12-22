#include "longrunningservice.grpc.pb.h"
#include "state_machine_types.h"
#include <chrono>
#include <future>
#include <grpcpp/grpcpp.h>

using namespace grpc;
using namespace org::ismailp::longrunningtask;
using namespace state_machine;

namespace {

std::ostream &printOutput(std::ostream &stm) {
  stm << "server: ";
  return stm;
}

class AsyncServer {
  LongRunningService::AsyncService service;
  std::unique_ptr<grpc::ServerCompletionQueue> cq;
  std::unique_ptr<grpc::Server> server;
  std::future<void> task;

  void processMessages();

  /// Represents one connection (request).
  struct Connection : public BasicStateMachine<Connection> {
    ServerContext ctx;
    LongRunningReq req;
    LongRunningResp resp;
    ServerAsyncWriter<LongRunningResp> responder;
    grpc::ServerCompletionQueue *cq;

    explicit Connection(LongRunningService::AsyncService &service,
                        ServerCompletionQueue *cq)
        : responder(&ctx), cq(cq) {
      service.RequestDoSomething(&ctx, &req, &responder, cq, cq, this);
    }

    std::ostream &printOutputImpl(std::ostream &stm) const {
      return ::printOutput(stm);
    }

    bool handleRequest(LongRunningService::AsyncService &service) {
      constexpr std::size_t numStates =
          std::underlying_type_t<state_machine::State>(
              state_machine::State::NumStates);

      switch (state) {
      case State::Initial: {
        printOutput(std::cout) << "Req id: " << req.id() << "\n";
        // Set the number of possible states. This could be bound to a more
        // dynamic structure, like a vector or map of states.
        resp.set_numtasks(numStates);
        responder.Write(resp, this);
        break;
      }
      case State::Setup:
        FALLTHROUGH
      case State::DataUpload:
        FALLTHROUGH
      case State::DataVerify:
        FALLTHROUGH
      case State::DataReady:
        // Since the server doesn't really do any operation, all states end up
        // calling this, wasting CPU cycles for some time, and then moving on.
        doTheOperation();
        if (state == State::DataReady) {
          // Server has reached its final state, and the streaming is finished.
          responder.Finish(Status::OK, this);
        }
        break;
      case State::NumStates: // no more transitions, and nextState should return
                             // false.
        break;
      }
      return nextState();
    }
    void doTheOperation() {
      using namespace std::chrono_literals;
      std::this_thread::sleep_for(100ms);
      resp.Clear();
      TaskStatus *taskStatus = resp.mutable_currenttask();
      taskStatus->set_currentstage(std::underlying_type_t<State>(state));
      taskStatus->set_statuscode(1);
      printOutput(std::cout)
          << "Sending status at state: " << toString(state) << "\n";
      responder.Write(resp, this);
    }
  };

public:
  void start() {
    grpc::ServerBuilder builder;
    builder.AddListeningPort("localhost:50151",
                             grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    cq = builder.AddCompletionQueue();
    server = builder.BuildAndStart();

    task = std::async(std::launch::async, &AsyncServer::processMessages, this);
  }
  void join() { task.get(); }
};

void AsyncServer::processMessages() {
  new Connection(service, cq.get());
  while (true) {
    void *tag;
    bool ok;
    if (!cq->Next(&tag, &ok))
      continue;
    if (!tag)
      continue;
    Connection *conn = static_cast<Connection *>(tag);
    if (not conn->handleRequest(service)) {
      return;
    }
  }
}

} // namespace

int main() {
  AsyncServer server;
  server.start();
  server.join();
}