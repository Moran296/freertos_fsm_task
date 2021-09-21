# freertos_fsm_task
A Freertos task class running a lightweight finite state machine using variants

This implementation of the fsm class is based on Mateusz Pusz mpusz/fsm-variant repository presented in his cppCon talk.
The difference is this implemantation runs under a freertos task.

### Usage:
    in order to use the library one must:
    1. first create the structs/classes used as events and states.
    2. ordered states/events in a variant, the first index in the state variant is the entry state.
    3. create a class which inherits the fsm task variants and itself (CRTP) as template arguments, and task info as ctor args
    
###   Example for a button fsm:
    
    //EVENTS:
    struct event_press {};
    struct event_release {};
    struct event_timer {int seconds}
    using EventsVariant = std::variant<event_press, event_release, event_timer>

    //STATES:
    struct state_idle {};
    struct state_pressed {};
    using EventsVariant = std::variant<state_idle, state_pressed>

    //STATE_MACHINE
    class ButtonFSM : public FsmTask<sample_fsm, states, events>
    {
    public:
        sample_fsm() : FsmTask(2048, 3, "button_fsm") {}

        //DEFAULT_HANDLER, will be invoked for undefined state/events
        template <typename State, typename Event>
        auto on_event(State &, const Event &)
        {
            printf("got an unknown event!");
            return std::nullopt; //return null option because state havn't changed
        }

        //handler for idle state, press event
        auto on_event(state_idle &, const event_press)
        {
            printf("state idle got press event!");
            return state_pressed{}; //state have changed
        }

        //handler for pressed state, timer event
        auto on_event(state_pressed &, const event_timer &event)
        {
            printf("state pressed got timer event after %d seconds!", event.seconds);
            return std::nullopt;
        }

        //handler for pressed state, release event
        auto on_event(state_pressed &, const event_release &)
        {
            printf("state pressed got release event");
            return state_idle{};
        }

        // We can also define state entry and exit handlers if we defined the requested macros (CALL_ON_STATE_EXIT / CALL_ON_STATE_ENTRY)

        //default handler for state entry
        template <class STATE>
        void on_entry(STATE& ) {}

        void on_entry(state_pressed &) {
            //do state entry logic
        }

        //default handler for state exit
        template <class STATE>
        void on_exit(STATE& ) {}

        void on_exit(state_idle &) {
            //do state exit logic
        }
    };
