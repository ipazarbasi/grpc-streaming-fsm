#include "longrunningservice.grpc.pb.h"
#include "state_machine_types.h"
#include <chrono>
#include <future>
#include <grpcpp/grpcpp.h>
#include <type_traits>

using namespace grpc;
using namespace org::ismailp::longrunningtask;

#ifdef __clang__
#define FALLTHROUGH [[clang::fallthrough]];
#elif defined(__GNUC__) && __GNUC__ >= 7
#define FALLTHROUGH __attribute__((fallthrough))
#else
#define FALLTHROUGH
#endif

namespace {
using state_machine::State;

class AsyncServer {
  LongRunningService::AsyncService service;
  std::unique_ptr<grpc::ServerCompletionQueue> cq;
  std::unique_ptr<grpc::Server> server;
  std::future<void> task;

  void processMessages();

  struct Connection {
    ServerContext ctx;
    LongRunningReq req;
    LongRunningResp resp;
    ServerAsyncWriter<LongRunningResp> responder;
    grpc::ServerCompletionQueue *cq;
    State state = State::Initial;

    explicit Connection(LongRunningService::AsyncService &service,
                        ServerCompletionQueue *cq)
        : responder(&ctx), cq(cq) {
      service.RequestDoSomething(&ctx, &req, &responder, cq, cq, this);
    }
    bool nextState() {
      if (state == State::NumStates)
        return false;
      auto v = std::underlying_type_t<State>(state);
      ++v;
      setState(static_cast<State>(v));
      return true;
    }
    void setState(State newState) {
      std::cout << "Transitioning from: '" << toString(state) << "' to '"
                << toString(state) << "'";
      state = newState;
    }
    bool handleRequest(LongRunningService::AsyncService &service) {
      constexpr std::size_t numStates =
          std::underlying_type_t<state_machine::State>(
              state_machine::State::NumStates);

      switch (state) {
      case State::Initial: {
        std::cout << "Req id: " << req.id() << "\n";
        resp.set_numtasks(numStates);
        state = State::Setup;
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
        doTheOperation();
        if (state == State::DataReady) {
          responder.Finish(Status::OK, this);
        }
        break;
      case State::NumStates:
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