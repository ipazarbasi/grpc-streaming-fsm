#include "longrunningservice.grpc.pb.h"
#include <chrono>
#include <future>
#include <grpcpp/grpcpp.h>

using namespace grpc;
using namespace org::ismailp::longrunningtask;

namespace {

enum class ServerState {
  RespondStreamSize,
  RespondStream,
  Completed,
};
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
    ServerState state = ServerState::RespondStreamSize;
    std::uint64_t currentStage = 0;

    explicit Connection(LongRunningService::AsyncService &service,
                        ServerCompletionQueue *cq)
        : responder(&ctx), cq(cq) {
      service.RequestDoSomething(&ctx, &req, &responder, cq, cq, this);
    }
    void handleRequest(LongRunningService::AsyncService &service) {
      using namespace std::chrono_literals;

      switch (state) {
      case ServerState::RespondStreamSize: {
        std::cout << "Req id: " << req.id() << "\n";
        resp.set_numtasks(5);
        state = ServerState::RespondStream;
        responder.Write(resp, this);
        std::cerr << "Written stream size\n";
        break;
      }
      case ServerState::RespondStream: {
        GPR_ASSERT(currentStage < 5);
        std::this_thread::sleep_for(100ms);
        resp.Clear();
        TaskStatus *taskStatus = resp.mutable_currenttask();
        taskStatus->set_currentstage(++currentStage);
        taskStatus->set_statuscode(1);

        if (currentStage == 5) {
          state = ServerState::Completed;
          responder.WriteAndFinish(resp, {}, Status::OK, this);
        } else {
          responder.Write(resp, this);
        }
        std::cout << "Written stage: " << currentStage << "\n";
        break;
      }
      case ServerState::Completed: {
        delete this;
      }
      }
    }
  };

  std::vector<std::unique_ptr<Connection>> connections;

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
  connections.emplace_back(std::make_unique<Connection>(service, cq.get()));
  while (true) {
    void *tag;
    bool ok;
    if (cq->Next(&tag, &ok)) {
      if (tag) {
        Connection *conn = static_cast<Connection *>(tag);
        conn->handleRequest(service);
      }
    }
  }
}

} // namespace

int main() {
  AsyncServer server;
  server.start();
  server.join();
}