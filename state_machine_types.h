#ifndef lrr_example_state_machine_types_h_
#define lrr_example_state_machine_types_h_

namespace state_machine {
enum class State {
  Initial,
  Setup,
  DataUpload,
  DataVerify,
  DataReady,
  NumStates
};

static constexpr const char *toString(State state) {
  switch (state) {
  case State::Initial:
    return "Initial";
  case State::Setup:
    return "Setup";
  case State::DataUpload:
    return "DataUpload";
  case State::DataVerify:
    return "DataVerify";
  case State::DataReady:
    return "DataReady";
  case State::NumStates:
    return "NumStates"; // std::terminate?
  }
}

} // namespace state_machine

#endif /* !lrr_example_state_machine_types_h_ */