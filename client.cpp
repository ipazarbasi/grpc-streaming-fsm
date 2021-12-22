#include "longrunningservice.grpc.pb.h"
#include "state_machine_types.h"
#include <grpcpp/grpcpp.h>
#include <type_traits>

using namespace grpc;
using namespace org::ismailp::longrunningtask;
using namespace state_machine;

namespace {

struct Connection : public BasicStateMachine<Connection> {
  LongRunningReq request;
  LongRunningResp reply;
  ClientContext context;

  Status status;
  std::size_t numStages;
  std::uint64_t id;
  std::unique_ptr<ClientAsyncReader<LongRunningResp>> response_reader;
  bool expectingServerState;
  explicit Connection(std::uint64_t id, LongRunningService::Stub &stub,
                      CompletionQueue *cq)
      : numStages(0), id(id), expectingServerState(false) {
    request.set_id(id);
    response_reader = stub.PrepareAsyncDoSomething(&context, request, cq);
    response_reader->StartCall(this);
  }
  bool handleMessage();
  void observeRemoteFSM();

  std::ostream &printOutputImpl(std::ostream &stm) const {
    stm << "client: ";
    return stm;
  }
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
  bool hasNext = true;

  while (hasNext && cq.Next(&tag, &ok)) {
    GPR_ASSERT(tag);
    auto *call = static_cast<Connection *>(tag);
    hasNext = call->handleMessage();
  }
}

bool Connection::handleMessage() {
  if (not expectingServerState) {
    if (state == State::Initial) {
      response_reader->Read(&reply, this);
      expectingServerState = true;
      return true;
    } else {
      delete this;
      return false;
    }
  }

  observeRemoteFSM();
  if (state == State::DataReady) {
    response_reader->Finish(&status, this);
    expectingServerState = false;
  }
  return true;
}

void Connection::observeRemoteFSM() {
  GPR_ASSERT(numStages == 0);
  response_reader->Read(&reply, this);
  switch (reply.taskStatus_case()) {
  case LongRunningResp::TaskStatusCase::kNumTasks:
    printOutput(std::cout) << "Num states: " << reply.numtasks() << "\n";
    break;
  case LongRunningResp::TaskStatusCase::kCurrentTask: {
    constexpr auto numStates = std::underlying_type_t<State>(State::NumStates);
    const auto &currentTask = reply.currenttask();
    auto curRemoteStateInt = currentTask.currentstage();
    if (numStates < curRemoteStateInt) {
      printOutput(std::cerr) << "invalid server state\n";
      std::terminate();
    }
    State curRemoteState = static_cast<State>(curRemoteStateInt);
    printOutput(std::cout) << "current state received\n"
                           << "  Current server state: "
                           << toString(curRemoteState) << " "
                           << "  Status code: " << currentTask.statuscode()
                           << "\n";
    setState(curRemoteState);
    break;
  }
  case LongRunningResp::TaskStatusCase::TASKSTATUS_NOT_SET:
    printOutput(std::cout) << "TASK STATUS NOT SET\n";
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