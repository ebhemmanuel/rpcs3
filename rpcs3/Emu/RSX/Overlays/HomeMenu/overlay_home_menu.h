#pragma once

#include "Emu/RSX/Overlays/overlays.h"
#include "Emu/Cell/ErrorCodes.h"
#include "overlay_home_menu_main_menu.h"

#include <vector>

namespace rsx
{
	namespace overlays
	{
		struct home_menu_dialog : public user_interface
		{
		public:
			home_menu_dialog();

			void update(u64 timestamp_us) override;
			void on_button_pressed(pad_button button_press, bool is_auto_repeat) override;

			compiled_resource get_compiled() override;

			error_code show(std::function<void(s32 status)> on_close);

		private:
			// Builds the pending-invite prompt ("<user> has invited you to play" + a white Join pill,
			// centered above the bottom toast). Triangle focuses it; opening the menu while the invite
			// toast is still up shows it minimally (no menu chrome) for a one-button quick-join.
			void build_invite_prompt();
			void join_focused_invite();
			void trigger_close();

			home_menu_main_menu m_main_menu;
			overlay_element m_dim_background{};
			label m_description{};
			label m_time_display{};

			// Pending-invite prompt elements (info line + white Join pill).
			std::unique_ptr<overlay_element> m_join_bg;
			std::unique_ptr<overlay_element> m_join_label;
			std::unique_ptr<overlay_element> m_invite_info;
			bool m_invite_focused = false;
			bool m_minimal_mode   = false;
			bool m_has_invites    = false;
			u64 m_top_invite_id   = 0;

			animation_color_interpolate fade_animation{};
		};
	}
}
