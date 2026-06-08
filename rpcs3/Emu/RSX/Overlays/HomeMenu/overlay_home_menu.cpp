#include "stdafx.h"
#include "overlay_home_menu.h"
#include "../overlay_manager.h"
#include "Emu/system_config.h"
#include "Emu/vfs_config.h"
#include "Emu/NP/rpcn_client.h"
#include "Emu/Cell/Modules/sceNp.h"
#include "Emu/Cell/timers.hpp"
#include "Emu/RSX/Overlays/overlay_message.h"
#include "Input/pad_thread.h"
#include "Utilities/date_time.h"
#include "Utilities/File.h"

extern atomic_t<bool> g_user_asked_for_screenshot;

namespace rsx
{
	namespace overlays
	{
		// No-op: we only want a one-shot snapshot of pending invites, so we register then
		// immediately remove this callback.
		static void home_menu_invite_snapshot_cb(void* /*param*/, shared_ptr<std::pair<std::string, message_data>> /*new_msg*/, u64 /*msg_id*/)
		{
		}

		std::string get_time_string()
		{
			return date_time::fmt_time("%Y/%m/%d %H:%M:%S", time(nullptr));
		}

		void home_menu_dialog::build_invite_section()
		{
			m_invite_panel.reset();
			m_invite_glance.reset();
			m_invite_open_btn.reset();
			m_invite_inviter.reset();
			m_join_bg.reset();
			m_join_label.reset();
			m_has_invites     = false;
			m_invite_expanded = false;
			m_top_invite_id   = 0;

			auto rpcn = rpcn::rpcn_client::get_instance(0);
			if (!rpcn)
			{
				return;
			}

			// Snapshot the currently pending invites (include bootable; most game invites are).
			const auto invites = rpcn->get_messages_and_register_cb(SCE_NP_BASIC_MESSAGE_MAIN_TYPE_INVITE, true, home_menu_invite_snapshot_cb, this);
			rpcn->remove_message_cb(home_menu_invite_snapshot_cb, this);

			if (invites.empty())
			{
				return;
			}

			// Use the newest pending invite (active_messages is ordered by ascending id).
			const auto& [id, msg] = invites.back();
			if (!msg)
			{
				return;
			}

			m_top_invite_id   = id;
			m_has_invites     = true;

			constexpr s16 margin = 40;

			// ---- Collapsed glance: "New invite" + a small Triangle button to open ----
			constexpr s16 btn_w = 34;
			constexpr s16 btn_h = 30;
			const s16 btn_x = static_cast<s16>(virtual_width - margin - btn_w);
			const s16 btn_y = static_cast<s16>(virtual_height - margin - btn_h);

			auto open_btn = std::make_unique<image_button>(btn_w, btn_h);
			static_cast<image_button*>(open_btn.get())->set_image_resource(resource_config::standard_image_resource::triangle);
			open_btn->set_pos(btn_x, btn_y);

			// "Received Invite" text just left of the Triangle button.
			auto glance = std::make_unique<label>(std::string("Received Invite"));
			glance->set_font("Arial", 20);
			glance->fore_color   = color4f(1.f, 1.f, 1.f, 1.f);
			glance->back_color.a = 0.f;
			static_cast<label*>(glance.get())->auto_resize();
			glance->set_pos(static_cast<s16>(btn_x - static_cast<s16>(glance->w) - 12), static_cast<s16>(btn_y + (btn_h - static_cast<s16>(glance->h)) / 2));

			// ---- Expanded: inviter name + white Join pill ----
			auto join_label = std::make_unique<label>("Join");
			join_label->set_font("Arial", 20);
			join_label->fore_color   = color4f(0.f, 0.f, 0.f, 1.f);
			join_label->back_color.a = 0.f;
			static_cast<label*>(join_label.get())->align_text(overlay_element::text_align::center);
			static_cast<label*>(join_label.get())->auto_resize();

			constexpr s16 hpad = 30;
			constexpr s16 vpad = 13;
			s16 join_w = static_cast<s16>(join_label->w) + 2 * hpad;
			if (join_w < 150) join_w = 150;
			const s16 join_h = static_cast<s16>(join_label->h) + 2 * vpad;

			const s16 join_x = static_cast<s16>(virtual_width - margin - join_w);
			const s16 join_y = static_cast<s16>(virtual_height - margin - join_h);

			auto join_bg = std::make_unique<rounded_rect>();
			static_cast<rounded_rect*>(join_bg.get())->border_radius = join_h / 2; // full pill
			join_bg->set_size(join_w, join_h);
			join_bg->set_pos(join_x, join_y);
			join_bg->back_color = color4f(1.f, 1.f, 1.f, 1.f); // white
			join_label->set_pos(join_x + (join_w - static_cast<s16>(join_label->w)) / 2, join_y + vpad);

			// Inviter name to the left of the Join pill, on the same row (vertically centered).
			auto inviter = std::make_unique<label>(msg->first);
			inviter->set_font("Arial", 22);
			inviter->fore_color   = color4f(1.f, 1.f, 1.f, 1.f);
			inviter->back_color.a = 0.f;
			static_cast<label*>(inviter.get())->auto_resize();
			inviter->set_pos(static_cast<s16>(join_x - static_cast<s16>(inviter->w) - 16), static_cast<s16>(join_y + (join_h - static_cast<s16>(inviter->h)) / 2));

			m_invite_open_btn = std::move(open_btn);
			m_invite_glance   = std::move(glance);
			m_invite_inviter  = std::move(inviter);
			m_join_bg         = std::move(join_bg);
			m_join_label      = std::move(join_label);
		}

		void home_menu_dialog::trigger_close()
		{
			fade_animation.current = color4f(1.f);
			fade_animation.end     = color4f(0.f);
			fade_animation.active  = true;

			fade_animation.on_finish = [this]
			{
				close(true, true);

				if (g_cfg.misc.pause_during_home_menu)
				{
					Emu.BlockingCallFromMainThread([]()
					{
						Emu.Resume();
					});
				}
			};
		}

		void home_menu_dialog::join_focused_invite()
		{
			::join_home_menu_invite(m_top_invite_id);
			m_invite_expanded = false;

			// Close the home menu (and resume emulation) so the game acts on the join.
			trigger_close();
		}

		home_menu_dialog::home_menu_dialog()
			: m_main_menu(20, 85, virtual_width - 2 * 20, 540, false, nullptr)
		{
			m_allow_input_on_pause = true;

			m_dim_background.set_size(virtual_width, virtual_height);
			m_dim_background.back_color.a = 0.85f;

			m_description.set_font("Arial", 20);
			m_description.set_pos(20, 37);
			m_description.set_text(m_main_menu.title);
			m_description.auto_resize();
			m_description.back_color.a = 0.f;

			m_time_display.set_font("Arial", 14);
			m_time_display.set_text(get_time_string());
			m_time_display.auto_resize();
			m_time_display.set_pos(virtual_width - (20 + m_time_display.w), (m_description.y + m_description.h) - m_time_display.h);
			m_time_display.back_color.a = 0.f;

			fade_animation.duration_sec = 0.15f;

			return_code = selection_code::canceled;
		}

		void home_menu_dialog::update(u64 timestamp_us)
		{
			// Close from the PS/home button (pressed again while the menu is open).
			if (pad::g_home_menu_close_requested.exchange(false))
			{
				const bool already_closing = fade_animation.active && fade_animation.end.a == 0.f;
				if (!already_closing)
				{
					trigger_close();
				}
			}

			if (fade_animation.active)
			{
				fade_animation.update(timestamp_us);
			}

			static std::string last_time;
			std::string new_time = get_time_string();

			if (last_time != new_time)
			{
				m_time_display.set_text(new_time);
				m_time_display.auto_resize();
				last_time = std::move(new_time);
			}

			m_main_menu.update(timestamp_us);
		}

		void home_menu_dialog::on_button_pressed(pad_button button_press, bool is_auto_repeat)
		{
			if (fade_animation.active) return;

			// Quick-join mode (opened over a live toast): only Join or dismiss, no menu navigation.
			if (m_minimal_mode)
			{
				switch (button_press)
				{
				case pad_button::cross: // Join
					join_focused_invite();
					return;
				case pad_button::circle: // Dismiss -> back to the game
					trigger_close();
					return;
				default:
					return;
				}
			}

			// Bottom-right invite section. Expanded shows the inviter + Join pill.
			if (m_invite_expanded)
			{
				switch (button_press)
				{
				case pad_button::cross: // Join
					join_focused_invite();
					return;
				case pad_button::circle: // Collapse back to the glance
				case pad_button::triangle:
					m_invite_expanded = false;
					return;
				default:
					// Anything else collapses and falls through to normal navigation.
					m_invite_expanded = false;
					break;
				}
			}
			else if (m_has_invites && button_press == pad_button::triangle)
			{
				// Open the invite from the collapsed glance.
				m_invite_expanded = true;
				return;
			}

			// Increase auto repeat interval for some buttons
			switch (button_press)
			{
			case pad_button::dpad_left:
			case pad_button::dpad_right:
			case pad_button::ls_left:
			case pad_button::ls_right:
				m_auto_repeat_ms_interval = 10;
				break;
			default:
				m_auto_repeat_ms_interval = m_auto_repeat_ms_interval_default;
				break;
			}

			const page_navigation navigation = m_main_menu.handle_button_press(button_press, is_auto_repeat, m_auto_repeat_ms_interval);

			switch (navigation)
			{
			case page_navigation::back:
			case page_navigation::next:
			{
				if (home_menu_page* page = m_main_menu.get_current_page(true))
				{
					std::string path = page->title;
					for (home_menu_page* parent = page->parent; parent; parent = parent->parent)
					{
						if (parent->title.empty())
						{
							break;
						}

						path = parent->title + "  >  " + path;
					}
					m_description.set_text(path);
					m_description.auto_resize();
				}
				break;
			}
			case page_navigation::exit:
			case page_navigation::exit_for_screenshot:
			{
				fade_animation.current = color4f(1.f);
				fade_animation.end = color4f(0.f);
				fade_animation.active = true;

				fade_animation.on_finish = [this, navigation]
				{
					close(true, true);

					if (g_cfg.misc.pause_during_home_menu)
					{
						Emu.BlockingCallFromMainThread([]()
						{
							Emu.Resume();
						});
					}

					if (navigation == page_navigation::exit_for_screenshot)
					{
						rsx_log.notice("Taking screenshot after exiting home menu");
						g_user_asked_for_screenshot = true;
					}
				};
				break;
			}
			case page_navigation::stay:
			{
				break;
			}
			}
		}

		compiled_resource home_menu_dialog::get_compiled()
		{
			if (!visible)
			{
				return {};
			}

			compiled_resource result;
			result.add(m_dim_background.get_compiled());

			if (m_minimal_mode)
			{
				// Quick-join only: the focused inviter + Join pill, no menu chrome.
				if (m_invite_inviter) result.add(m_invite_inviter->get_compiled());
				if (m_join_bg) result.add(m_join_bg->get_compiled());
				if (m_join_label) result.add(m_join_label->get_compiled());

				fade_animation.apply(result);
				return result;
			}

			result.add(m_main_menu.get_compiled());
			result.add(m_description.get_compiled());
			result.add(m_time_display.get_compiled());

			// Bottom-right invite section: a collapsed glance (envelope + count + Triangle button)
			// that expands (Triangle) to the inviter's name and a Join pill.
			if (m_has_invites)
			{
				if (m_invite_expanded)
				{
					if (m_invite_inviter) result.add(m_invite_inviter->get_compiled());
					if (m_join_bg) result.add(m_join_bg->get_compiled());
					if (m_join_label) result.add(m_join_label->get_compiled());
				}
				else
				{
					if (m_invite_glance) result.add(m_invite_glance->get_compiled());
					if (m_invite_open_btn) result.add(m_invite_open_btn->get_compiled());
				}
			}

			fade_animation.apply(result);

			return result;
		}

		error_code home_menu_dialog::show(std::function<void(s32 status)> on_close)
		{
			visible = false;

			fade_animation.current = color4f(0.f);
			fade_animation.end = color4f(1.f);
			fade_animation.active = true;

			this->on_close = std::move(on_close);

			// Build the bottom-right invite section.
			build_invite_section();

			if (m_has_invites)
			{
				// If opened while the invite toast is still on screen, show only the focused Join
				// prompt (no full menu chrome) so the player can join straight from the game.
				const u64 last_toast = g_last_invite_toast_time_us;
				if (last_toast != 0 && (get_system_time() - last_toast) < 6'000'000)
				{
					m_minimal_mode    = true;
					m_invite_expanded = true;

					// Consume the toast window so a later Home press opens the full menu instead.
					g_last_invite_toast_time_us = 0;

					// Quick-join layout: center the Join pill near the bottom with the inviter
					// name centered directly above it (the bottom-right + name-beside layout is
					// only used for the full home menu).
					if (m_join_bg)
					{
						const s16 jw = static_cast<s16>(m_join_bg->w);
						const s16 jh = static_cast<s16>(m_join_bg->h);
						const s16 jx = static_cast<s16>((virtual_width - jw) / 2);
						const s16 jy = static_cast<s16>(virtual_height - 170);
						m_join_bg->set_pos(jx, jy);

						if (m_join_label)
							m_join_label->set_pos(static_cast<s16>(jx + (jw - static_cast<s16>(m_join_label->w)) / 2), static_cast<s16>(jy + (jh - static_cast<s16>(m_join_label->h)) / 2));

						if (m_invite_inviter)
							m_invite_inviter->set_pos(static_cast<s16>((virtual_width - static_cast<s16>(m_invite_inviter->w)) / 2), static_cast<s16>(jy - 36));
					}
				}

				// The section/prompt shows the invite, so fade out the now-redundant toast.
				dismiss_message_queue();
			}

			// Dim less in minimal mode so the game stays visible behind the quick-join prompt.
			m_dim_background.back_color.a = m_minimal_mode ? 0.4f : 0.85f;

			visible = true;

			const auto notify = std::make_shared<atomic_t<u32>>(0);
			auto& overlayman = g_fxo->get<display_manager>();

			overlayman.attach_thread_input(
				uid, "Home menu",
				[notify]() { *notify = true; notify->notify_one(); }
			);

			if (g_cfg.misc.pause_during_home_menu)
			{
				Emu.BlockingCallFromMainThread([]()
				{
					Emu.Pause(false, false);
				});
			}

			while (!Emu.IsStopped() && !*notify)
			{
				notify->wait(false, atomic_wait_timeout{1'000'000});
			}

			return CELL_OK;
		}
	} // namespace overlays
} // namespace RSX
