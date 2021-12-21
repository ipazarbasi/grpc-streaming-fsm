#include "longrunningservice.grpc.pb.h"
#include <grpcpp/grpcpp.h>

using namespace grpc;
using namespace org::ismailp::longrunningtask;

namespace {
enum class ClientState {
  RequestStreamSize,
  ReceiveStreamSize,
  Completed,
  Destroy,
};

struct Connection {
  LongRunningReq request;
  LongRunningResp reply;
  ClientContext context;

  Status status;
  std::size_t numStages;
  std::uint64_t id;
  ClientState state;
  std::unique_ptr<ClientAsyncReader<LongRunningResp>> response_reader;
  explicit Connection(std::uint64_t id, LongRunningService::Stub &stub,
                      CompletionQueue *cq)
      : numStages(0), id(id), state(ClientState::RequestStreamSize) {
    request.set_id(id);
    response_reader = stub.PrepareAsyncDoSomething(&context, request, cq);
    response_reader->StartCall(this);
  }
  bool handleMessage();
  void receiveStreamSize();
};

class Client {
  CompletionQueue cq;
  std::unique_ptr<LongRunningService::Stub> stub;
  std::uint64_t id;

public:
  explicit Client(std::shared_ptr<Channel> channel)
      : stub(LongRunningService::NewStub(channel)), id(0) {}
  void asyncHandleCall();
  void prepareCall();
};

void Client::prepareCall() { new Connection(id++, *stub, &cq); }

void Client::asyncHandleCall() {
  void *tag = nullptr;
  bool ok = false;

  while (cq.Next(&tag, &ok)) {
    GPR_ASSERT(tag);
    auto *call = static_cast<Connection *>(tag);
    if (!call->handleMessage())
      return;
  }
}

bool Connection::handleMessage() {
  switch (state) {
  case ClientState::RequestStreamSize: {
    state = ClientState::ReceiveStreamSize;
    response_reader->Read(&reply, this);
    return true;
  }
  case ClientState::ReceiveStreamSize: {
    receiveStreamSize();
    return true;
  }
  case ClientState::Completed: {
    state = ClientState::Destroy;
    response_reader->Finish(&status, this);
    return true;
  }
  case ClientState::Destroy:
    delete this;
    return false;
  }
  std::cerr << "unreachable has been reached\n";
  std::terminate();
}

void Connection::receiveStreamSize() {
  GPR_ASSERT(numStages == 0);
  response_reader->Read(&reply, this);
  switch (reply.taskStatus_case()) {
  case LongRunningResp::TaskStatusCase::kNumTasks:
    std::cout << "num tasks received\n"
              << "Num tasks: " << reply.numtasks() << "\n";
    break;
  case LongRunningResp::TaskStatusCase::kCurrentTask: {
    const auto &currentTask = reply.currenttask();
    std::cout << "current task received\n"
              << "Current stage: " << currentTask.currentstage() << " "
              << "Status code: " << currentTask.statuscode() << "\n";
    if (currentTask.currentstage() == 5)
      state = ClientState::Completed;
    break;
  }
  case LongRunningResp::TaskStatusCase::TASKSTATUS_NOT_SET:
    std::cout << "TASK STATUS NOT SET\n";
    break;
  }
}

} // namespace
int main() {
  Client c(grpc::CreateChannel("localhost:50151",
                               grpc::InsecureChannelCredentials()));
  c.prepareCall();
  c.asyncHandleCall();

  return 0;
}