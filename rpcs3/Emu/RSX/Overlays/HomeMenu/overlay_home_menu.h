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
			// A glanceable styled card shown at the bottom-right for each pending PSN invite.
			// Pressing Triangle (or opening the menu while the invite toast is up) focuses the
			// "Join" pill above the newest card, where Cross joins and Circle cancels.
			struct invite_card
			{
				std::unique_ptr<overlay_element> background;
				std::unique_ptr<overlay_element> content;
				std::unique_ptr<image_info> icon_data;
			};

			void build_invite_cards();
			void join_focused_invite();

			home_menu_main_menu m_main_menu;
			overlay_element m_dim_background{};
			label m_description{};
			label m_time_display{};
			std::vector<invite_card> m_invite_cards;

			// Focusable white "Join" pill, shown above the newest invite card when focused.
			std::unique_ptr<overlay_element> m_join_bg;
			std::unique_ptr<overlay_element> m_join_label;
			bool m_invite_focused = false;
			bool m_has_invites    = false;
			u64 m_top_invite_id   = 0;

			animation_color_interpolate fade_animation{};
		};
	}
}
