#ifndef lrr_example_state_machine_types_h_
#define lrr_example_state_machine_types_h_

#include <iostream>
#include <type_traits>

// To be C++17 compatible; some compilers might warn about unintentional
// fallthrough in switch statements.
#if __cplusplus < 201703L
#ifdef __clang__
#define FALLTHROUGH [[clang::fallthrough]];
#elif defined(__GNUC__) && __GNUC__ >= 7
#define FALLTHROUGH __attribute__((fallthrough))
#else
#define FALLTHROUGH
#endif // __clang__
#else  // __cplusplus < 201703L
#define FALLTHROUGH [[fallthrough]];
#endif

namespace state_machine {
enum class State {
  Initial, ///< Initial state. Both the client and the server starts from this
           ///< state.
  Setup,
  DataUpload,
  DataVerify,
  DataReady, ///< The last state for the FSM.
  NumStates  ///< Used for seting the upper bound for the underlying integer
             ///< type. Useful for checking how many states there will be and
             ///< preventing transition after this state.
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
    return "NumStates";
  }
}

// Don't worry too much about CRTP here; it's used only for printOutput.
template <typename Derived> class BasicStateMachine {
protected:
  State state = State::Initial;

  ~BasicStateMachine() noexcept = default; // non-virtual, protected destructor

public:
  /// Switches to the state, if possible. If the FSM has reached its final
  /// statate, no transition happens.
  /// \return true, if transition is successful. false, if the FSM has already
  /// reached its final state.
  bool nextState() {
    if (state == State::NumStates)
      return false;
    auto v = std::underlying_type_t<State>(state);
    ++v;
    setState(static_cast<State>(v));
    return true;
  }

  /// Sets the state to \arg newState.
  void setState(State newState) {
    printOutput(std::cout) << "Transitioning from: '" << toString(state)
                           << "' to '" << toString(newState) << "'\n";
    state = newState;
  }

  /// Prints output. Used for printing out transitions. It calls the derived
  /// type's printOutput because the derived type knows how to print things
  /// in its own context (i.e. client or server context).
  std::ostream &printOutput(std::ostream &stm) const {
    return static_cast<const Derived *>(this)->printOutputImpl(stm);
  }
};

} // namespace state_machine

#endif /* !lrr_example_state_machine_types_h_ */