#ifndef __FSM_TASK_H__
#define __FSM_TASK_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <variant>
#include <optional>

/*  Finite state machine running over a freertos task.
    This implementation of the fsm class is based on Mateusz Pusz mpusz/fsm-variant repository presented in his cppCon talk.
    The difference is this implemantation runs under a freertos task.

    In order to use this class, one must:
    1. first create the structs/classes used as events and states.
    2. ordered states/events in a variant, the first index in the state variant is the entry state.
    3. create a class which inherits the fsm task variants and itself (CRTP) as template arguments, and task info as ctor args
    Example for a button fsm:
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

*/

/*
    define as 1 if on exit state functions required. default on_exit function can be implemented as
    template <class STATE>
     void on_exit(STATE& ) {}
*/
#ifndef CALL_ON_STATE_EXIT
#define CALL_ON_STATE_EXIT 0
#endif

/*
    define as 1 if on entry state functions required. default on_exit function can be implemented as
    template <class STATE>
     void on_entry(STATE& ) {}
*/
#ifndef CALL_ON_STATE_ENTRY
#define CALL_ON_STATE_ENTRY 0
#endif

#ifndef YIELD_FROM_ISR_IF
#define YIELD_FROM_ISR_IF(X) \
    if (X)                   \
    portYIELD_FROM_ISR()
#endif

template <typename Derived, typename StateVariant, typename EventVariant>
class FsmTask
{

public:
    //Create the FSM Task
    FsmTask(uint32_t taskSize, uint8_t priority, const char *name)
    {
        vSemaphoreCreateBinary(m_semaphore);
        xSemaphoreTake(m_semaphore, 0);
        BaseType_t res = xTaskCreatePinnedToCore(taskFunc, name, taskSize, this, priority, &m_task, tskNO_AFFINITY);
        configASSERT(res == pdPASS);
    }

    /*
        Dispatch an event to state machine
        The event must be of a type from the events provided in EventVariant or otherwise won't compile
    */
    template <typename Event>
    void Dispatch(Event &&event, bool fromISR = false)
    {
        m_events = std::move(event);
        if (fromISR)
        {
            BaseType_t hasHigherWoken = pdFALSE;
            xSemaphoreGiveFromISR(m_semaphore, &hasHigherWoken);
            YIELD_FROM_ISR_IF(hasHigherWoken);
        }

        else
            xSemaphoreGive(m_semaphore);
    }

    //Get the states variant
    const StateVariant &GetState() const { return m_states; }

private:
    StateVariant m_states;
    EventVariant m_events;
    TaskHandle_t m_task;
    SemaphoreHandle_t m_semaphore;

    //Handle event dispatch
    void dispatch()
    {
        Derived &child = static_cast<Derived &>(*this);
        auto newState = std::visit(
            [&](auto &stateVar, auto &eventVar) -> std::optional<StateVariant> { return child.on_event(stateVar, eventVar); },
            m_states, m_events);

        handleNewState(newState);
    }

    //Handle state transition
    void handleNewState(std::optional<StateVariant> newState)
    {
        Derived &child = static_cast<Derived &>(*this);
        if (!newState)
            return;

        if constexpr (CALL_ON_STATE_EXIT)
        {
            std::visit([&](auto &stateVar)
                       { child.on_exit(stateVar); },
                       m_states);
        }

        m_states = *std::move(newState);

        if constexpr (CALL_ON_STATE_ENTRY)
        {
            std::visit([&](auto &stateVar)
                       { child.on_entry(stateVar); },
                       m_states);
        }
    }

    static void taskFunc(void *arg)
    {
        FsmTask *This = reinterpret_cast<FsmTask *>(arg);
        for (;;)
        {
            xSemaphoreTake(This->m_semaphore, portMAX_DELAY);
            This->dispatch();
        }
    }
};

#endif // __FSM_TASK_H__