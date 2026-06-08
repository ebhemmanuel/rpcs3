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
			// Bottom-right invite section. Collapsed: an envelope + count glance and a Triangle button
			// to open it. Expanded (Triangle): the inviter's name and a white Join pill (Cross joins).
			void build_invite_section();
			void join_focused_invite();
			void trigger_close();

			home_menu_main_menu m_main_menu;
			overlay_element m_dim_background{};
			label m_description{};
			label m_time_display{};

			std::unique_ptr<overlay_element> m_invite_panel;    // rounded background
			std::unique_ptr<overlay_element> m_invite_glance;   // "<n> envelope" count (collapsed)
			std::unique_ptr<overlay_element> m_invite_open_btn; // Triangle button to open (collapsed)
			std::unique_ptr<overlay_element> m_invite_inviter;  // inviter name (expanded)
			std::unique_ptr<overlay_element> m_join_bg;         // Join pill background (expanded)
			std::unique_ptr<overlay_element> m_join_label;      // "Join" (expanded)
			bool m_invite_expanded = false;
			bool m_has_invites     = false;
			u64 m_top_invite_id    = 0;

			animation_color_interpolate fade_animation{};
		};
	}
}
