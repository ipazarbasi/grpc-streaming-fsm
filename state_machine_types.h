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

}

#endif /* !lrr_example_state_machine_types_h_ */