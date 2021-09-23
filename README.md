# freertos_fsm_task
A Freertos task class running a lightweight finite state machine using std::variants and std::optional (c++ 17)

This implementation of the fsm class is based on Mateusz Pusz mpusz/fsm-variant repository presented in his CppCon talk.
The difference is this implemantation runs under a freertos task.

### Note:
Currently the code is only tested with esp32 (esp-idf) and uses esp-idf xTaskCreatePinnedToCore to create the task.

## Usage:
in order to use the library one must:
1. first create the structs/classes used as events and states.
2. order states/events in a variant, the first index in the state variant is the entry state. (see example)
3. create a class which inherits the fsm task variants and itself (CRTP) as template arguments, and task info as ctor args

###   Example for a button fsm:

    //1. Create events and state structs:
    //EVENTS:
    struct event_press {};
    struct event_release {};
    struct event_timer {int seconds}

    //STATES:
    struct state_idle {};
    struct state_pressed {
        uint32_t time_pressed() {return 0;}
    };

    //2. order states/events in a variant. First state in variant is the entry state.
    using Events = std::variant<event_press, event_release, event_timer>
    using States = std::variant<state_idle, state_pressed>

    // 3. Create the state machine
    //BUTTON STATE MACHINE
    class ButtonFSM : public FsmTask<ButtonFSM, Events, States>
    {
    public:
        ButtonFSM() : FsmTask(2048, 3, "button_fsm") {}

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


    int main() {

        ButtonFSM button;

        configASSERT(button.IsInState<state_idle>()); //we start at idle state

        bool dispatchedSuccessfully = button.Dispatch(press_event{}); // we move to pressed state
        configASSERT(dispatchedSuccessfully); // check that event was dispatched successfully (if not consider enlarging event queue)

        vTaskDelay(pdMS_TO_TICKS(100));
        configASSERT(button.IsInState<state_pressed>());

        //We can do something with the state too (we just have to be sure we are in this state, otherwise we assert)
        auto& p_state = button.Get<state_pressed>();
        printf("time pressed = %d", p_state.time_pressed());

        dispatchedSuccessfully = button.Dispatch(timer_event{3_sec}); // we stay in pressed state
        configASSERT(dispatchedSuccessfully);

        vTaskDelay(pdMS_TO_TICKS(100));
        configASSERT(button.IsInState<state_pressed>());

        dispatchedSuccessfully = button.Dispatch(release_event{}, pdMS_TO_TICKS(300)); //we can also provide a timeout for dispatch
        configASSERT(dispatchedSuccessfully);

        vTaskDelay(pdMS_TO_TICKS(100));
        configASSERT(button.IsInState<state_idle>());
    }

    void a_button_ISR(void* arg) {
        ButtonFSM *button = reinterpret_cast<ButtonFSM *>(arg);
        BaseType_t higherPriorityTaskWoken = pdFALSE;
        bool isButtonPressed = a_way_to_know_if_button_is_pushed_or_released();
        if  (isButtonPressed)
            button->DispatchFromIsr(press_event{}, &higherPriorityTaskWoken);
        else
            button->DispatchFromIsr(release_event{}, &higherPriorityTaskWoken);

        if (higherPriorityTaskWoken)
            portYIELD_FROM_ISR();
    }
