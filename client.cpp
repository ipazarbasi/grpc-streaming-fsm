#include "longrunningservice.grpc.pb.h"
#include <grpcpp/grpcpp.h>

using namespace grpc;
using namespace org::ismailp::longrunningtask;

namespace {
enum class ClientState {
  RequestStreamSize,
  ReceiveStreamSize,
  Completed,
};

struct Connection {
  LongRunningReq request;
  LongRunningResp reply;

  ClientContext context;

  Status status;
  std::unique_ptr<ClientAsyncReader<LongRunningResp>> response_reader;
 };

class Client {
  CompletionQueue cq_;
  std::unique_ptr<LongRunningService::Stub> stub_;

  std::size_t numStages;
  std::uint64_t id;
  ClientState state;

  void receiveStreamSize(void *);

public:
  explicit Client(std::shared_ptr<Channel> channel)
      : stub_(LongRunningService::NewStub(channel)), numStages(0), id(4),
        state(ClientState::RequestStreamSize) {}
  void asyncHandleCall();
  void prepareCall();
};

void Client::prepareCall() {
  auto req_ptr = new Connection;
  req_ptr->request.set_id(id++);

  state = ClientState::RequestStreamSize;
  req_ptr->response_reader =
      stub_->PrepareAsyncDoSomething(&req_ptr->context, req_ptr->request, &cq_);
  req_ptr->response_reader->StartCall(req_ptr);
}

void Client::asyncHandleCall() {
  void *tag;
  bool ok = false;

  while (cq_.Next(&tag, &ok)) {
    auto *call = static_cast<Connection *>(tag);
    GPR_ASSERT(tag);
    switch (state) {
    case ClientState::RequestStreamSize: {
      state = ClientState::ReceiveStreamSize;
      call->response_reader->Read(&call->reply, tag);
      break;
    }
    case ClientState::ReceiveStreamSize: {
      receiveStreamSize(tag);
      break;
    }
    case ClientState::Completed: {
      delete call;
      while (cq_.Next(&tag, &ok));
      return;
    }
    }
  }
}

void Client::receiveStreamSize(void *tag) {
  GPR_ASSERT(numStages == 0);
  auto *call2 = static_cast<Connection *>(tag);
  call2->response_reader->Read(&call2->reply, tag);
  switch (call2->reply.taskStatus_case()) {
  case LongRunningResp::TaskStatusCase::kNumTasks:
    std::cout << "num tasks received\n"
              << "Num tasks: " << call2->reply.numtasks() << "\n";
    break;
  case LongRunningResp::TaskStatusCase::kCurrentTask: {
    const auto &currentTask = call2->reply.currenttask();
    std::cout << "current task received\n"
              << "Current stage: " << currentTask.currentstage() << " "
              << "Status code: " << currentTask.statuscode() << "\n";
    if (currentTask.currentstage() == 5) {
      state = ClientState::Completed;
      call2->response_reader->Finish(&call2->status, nullptr);
    }
    break;
  }
  case LongRunningResp::TaskStatusCase::TASKSTATUS_NOT_SET:
    std::cout << "TASK STATUS NOT SET\n";
    break;
  }
  std::cout.flush();
}

} // namespace
int main() {
  Client c(grpc::CreateChannel("localhost:50151",
                               grpc::InsecureChannelCredentials()));
  c.prepareCall();
  c.asyncHandleCall();

  return 0;
}