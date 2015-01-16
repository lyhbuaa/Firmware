#pragma once

#include <type_traits>

#include <drivers/drv_airleash.h>

#include "debug.hpp"
#include "kbd_defines.hpp"

namespace kbd_handler {

using namespace airleash;

constexpr bool
in_air_mode(ModeId m)
{
	return ModeId::FLIGHT <= m;
}


template< bool matches >
using When = typename std::enable_if< matches >::type;


template <ModeId, EventKind, ButtonId, typename = void>
struct handle : Default // Only this one should be inherited from Default.
{ static void exec(App&) { say("unknown"); } }; // TODO tone bzzz

/*
 * Power long press hangling is special one.
 */

template <ModeId MODE>
struct handle<MODE, EventKind::LONG_PRESS, BTN_MASK_POWER, When<
	MODE != ModeId::CONFIRM_ARM
> > {
	static void
	exec(App&)
	{
		say("power off");
		usleep(5000); // 5ms to make the message visible at console.
		halt_and_power_off();
	}
};

/*
 * State transitions
 */

static void
on_mode_timeout(App & app, ModeId mode_if_inactive)
{
	ModeId m = mode_if_inactive;
	if (app.drone_status.active())
	{
		m = app.drone_status.in_air() ?
			ModeId::FLIGHT : ModeId::PREFLIGHT;
	}
	app.set_mode_transition(m);
}

template <ModeId MODE>
struct handle< MODE, EventKind::COPTER_CHANGED_STATE, BTN_NONE, When<
	MODE == ModeId::INIT or MODE == ModeId::PREFLIGHT
> > {
	static void
	exec(App & app)
	{
		say("INIT or PREFLIGHT on_mode_timeout");
		on_mode_timeout(app, ModeId::INIT);
	}
};

template <ModeId MODE>
struct handle< MODE, EventKind::COPTER_CHANGED_STATE, BTN_NONE, When<
	in_air_mode(MODE)
> > {
	static void
	exec(App & app)
	{
		say("FLIGHT_* on_mode_timeout");
		on_mode_timeout(app, ModeId::FLIGHT_NO_SIGNAL);
	}
};

template <>
struct handle<ModeId::PREFLIGHT, EventKind::LONG_PRESS, BTN_MASK_PLAY>
{
	static void
	exec(App & app)
	{
		say("PREFLIGHT LONG_PRESS PLAY");
		if (app.drone_status.ready_to_arm())
		{
			// notify drone
			app.set_mode_transition(ModeId::CONFIRM_ARM);
			say("ARM waiting confirm.");
		}
		else
		{
			app.tone.arm_failed();
			say("ARM request failed: drone is not ready to arm");
		}
	}
};

template <>
struct handle<ModeId::CONFIRM_ARM, EventKind::LONG_PRESS, BTN_MASK_CENTER>
{
	static void
	exec(App & app)
	{
		say("CONFIRM_ARM LONG_PRESS OK");
		if (app.drone_status.ready_to_arm())
		{
			app.drone_cmd.send_arm_command(app.drone_status);
		}
		else
		{
			app.tone.arm_failed();
			say("ARM confirm failed: drone is not ready to arm");
		}
	}
};

template <EventKind EVENT, ButtonId AnyButton>
struct handle< ModeId::CONFIRM_ARM, EVENT, AnyButton, When<
	EVENT == EventKind::RELEASE or EVENT == EventKind::TIMEOUT
> > {
//: Default // LONG_PRESS/REPEAT_PRESS does not matter
	static void
	exec(App & app)
	{
		say("CONFIRM_ARM RELEASE or TIMEOUT *");
		say("Switching back to PREFLIGHT mode.");
		app.set_mode_transition(ModeId::PREFLIGHT);
	}
};

template <ModeId MODE>
struct handle< MODE, EventKind::SHORT_PRESS, BTN_MASK_POWER, When<
	MODE != ModeId::CONFIRM_ARM
> > {
	static void
	exec(App&)
	{
		say("open menu");
		// app.set_mode_transition(ModeId::MENU);
	}
};

template <>
struct handle<ModeId::FLIGHT, EventKind::SHORT_PRESS, BTN_MASK_CENTER>
{
	static void
	exec(App & app)
	{
		say("FLIGHT SHORT_PRESS CENTER");
		app.set_mode_transition(ModeId::FLIGHT_ALT);
	}
};

template <>
struct handle<ModeId::FLIGHT_ALT, EventKind::SHORT_PRESS, BTN_MASK_CENTER>
{
	static void
	exec(App & app)
	{
		say("FLIGHT_ALT SHORT_PRESS CENTER");
		app.set_mode_transition(ModeId::FLIGHT);
	}
};

template <>
struct handle<ModeId::FLIGHT_ALT, EventKind::TIMEOUT, BTN_NONE>
{
	static void
	exec(App & app)
	{
		say("FLIGHT_ALT TIMEOUT");
		app.set_mode_transition(ModeId::FLIGHT);
	}
};

template <>
struct handle<ModeId::FLIGHT, EventKind::LONG_PRESS, BTN_MASK_CENTER>
{
	static void
	exec(App & app)
	{
		say("FLIGHT_ALT LONG_PRESS CENTER");
		app.set_mode_transition(ModeId::SHORTCUT);
	}
};

template <EventKind EVENT>
struct handle< ModeId::SHORTCUT, EVENT, BTN_NONE, When<
	EVENT == EventKind::RELEASE or EVENT == EventKind::TIMEOUT
> > {
	static void
	exec(App & app)
	{
		say("SHORTCUT TIMEOUT or RELEASE");
		app.set_mode_transition(ModeId::FLIGHT);
	}
};

} // end of namespace kbd_handler

#include "kbd_handler_flight.hpp"
#include "kbd_handler_base.hpp"  /* this one should be last */
