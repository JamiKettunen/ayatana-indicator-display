/*
 * Copyright 2014 Canonical Ltd.
 * Copyright 2021 Robert Tari
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 *   Robert Tari <robert@tari.in>
 */

#include <src/rotation-lock.h>

#include <glib/gi18n.h>

extern "C"
{
    #include <ayatana/common/utils.h>
}

class RotationLockIndicator::Impl
{
public:

  Impl()
  {
    GSettingsSchemaSource *pSource = g_settings_schema_source_get_default();

    if (pSource != NULL)
    {
        if (ayatana_common_utils_is_lomiri()) {

            GSettingsSchema *pSchema = g_settings_schema_source_lookup(pSource, "com.lomiri.touch.system", FALSE);

            if (pSchema != NULL)
            {
                g_settings_schema_unref(pSchema);
                m_settings = g_settings_new("com.lomiri.touch.system");
            }
            else
            {
                g_error("No schema could be found");
            }

        }
        else {

            GSettingsSchema *pSchema = g_settings_schema_source_lookup(pSource, "org.ayatana.indicator.display", FALSE);

            if (pSchema != NULL)
            {
                g_settings_schema_unref(pSchema);
                m_settings = g_settings_new("org.ayatana.indicator.display");
            }
            else
            {
                g_error("No schema could be found");
            }

        }
    }

    m_action_group = create_action_group();

    // build the icon
    const char *rotation_lock_icon_name {"orientation-lock"};

    if (!ayatana_common_utils_is_lomiri())
    {
        rotation_lock_icon_name = "display";
    }

    auto icon = g_themed_icon_new_with_default_fallbacks(rotation_lock_icon_name);
    auto icon_deleter = [](GIcon* o){g_object_unref(G_OBJECT(o));};
    m_icon.reset(icon, icon_deleter);

    // build the phone profile
    auto menu_model_deleter = [](GMenuModel* o){g_object_unref(G_OBJECT(o));};
    std::shared_ptr<GMenuModel> phone_menu (create_phone_menu(), menu_model_deleter);
    m_phone = std::make_shared<SimpleProfile>("phone", phone_menu);
    update_phone_header();

    // build the desktop profile
    std::shared_ptr<GMenuModel> desktop_menu (create_desktop_menu(), menu_model_deleter);
    m_desktop = std::make_shared<SimpleProfile>("desktop", desktop_menu);
    update_desktop_header();
  }

  ~Impl()
  {
    g_signal_handlers_disconnect_by_data(m_settings, this);
    g_clear_object(&m_action_group);
    g_clear_object(&m_settings);
  }

  GSimpleActionGroup* action_group() const
  {
    return m_action_group;
  }

  std::vector<std::shared_ptr<Profile>> profiles()
  {
    std::vector<std::shared_ptr<Profile>> ret;
    ret.push_back(m_phone);
    ret.push_back(m_desktop);
    return ret;
  }

private:

  /***
  ****  Actions
  ***/

  static gboolean settings_to_action_state(GValue *value,
                                           GVariant *variant,
                                           gpointer /*unused*/)
  {
    g_value_set_variant(value, variant);
    return TRUE;
  }

  static GVariant* action_state_to_settings(const GValue *value,
                                            const GVariantType * /*expected_type*/,
                                            gpointer /*unused*/)
  {
    return g_value_dup_variant(value);
  }

  GSimpleActionGroup* create_action_group()
  {
    GSimpleActionGroup* group;
    GSimpleAction* action;

    group = g_simple_action_group_new();
    GVariantType *pVariantType = g_variant_type_new("b");
    action = g_simple_action_new_stateful("rotation-lock",
                                          pVariantType,
                                          g_variant_new_boolean(false));
    g_variant_type_free(pVariantType);
    g_settings_bind_with_mapping(m_settings, "rotation-lock",
                                 action, "state",
                                 G_SETTINGS_BIND_DEFAULT,
                                 settings_to_action_state,
                                 action_state_to_settings,
                                 nullptr,
                                 nullptr);

    g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action));
    g_object_unref(G_OBJECT(action));
    g_signal_connect_swapped(m_settings, "changed::rotation-lock",
                             G_CALLBACK(on_rotation_lock_setting_changed), this);

    return group;
  }

  /***
  ****  Phone profile
  ***/

  static void on_rotation_lock_setting_changed (gpointer gself)
  {
    static_cast<Impl*>(gself)->update_phone_header();
  }

  GMenuModel* create_phone_menu()
  {
    GMenu* menu;
    GMenu* section;
    GMenuItem* menu_item;

    menu = g_menu_new();
    section = g_menu_new();
    menu_item = g_menu_item_new(_("Rotation Lock"), "indicator.rotation-lock");
    g_menu_item_set_attribute(menu_item, "x-ayatana-type", "s", "org.ayatana.indicator.switch");
    g_menu_append_item(section, menu_item);
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);
    g_object_unref(menu_item);

    return G_MENU_MODEL(menu);
  }

  GMenuModel* create_desktop_menu()
  {
    GMenu* menu;
    GMenu* section;
    GMenuItem* menu_item;

    menu = g_menu_new();
    section = g_menu_new();
    menu_item = g_menu_item_new(_("Rotation Lock"), "indicator.rotation-lock");
    g_menu_item_set_attribute(menu_item, "x-ayatana-type", "s", "org.ayatana.indicator.switch");
    g_menu_append_item(section, menu_item);
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);
    g_object_unref(menu_item);

    return G_MENU_MODEL(menu);
  }

  void update_phone_header()
  {
    Header h;
    h.title = _("Rotation");
    h.a11y = h.title;
    h.is_visible = g_settings_get_boolean(m_settings, "rotation-lock");
    h.icon = m_icon;
    m_phone->header().set(h);
  }

  void update_desktop_header()
  {
    Header h;
    h.title = _("Rotation");
    h.a11y = h.title;
    h.is_visible = TRUE;
    h.icon = m_icon;
    m_desktop->header().set(h);
  }

  /***
  ****
  ***/

  GSettings* m_settings = nullptr;
  GSimpleActionGroup* m_action_group = nullptr;
  std::shared_ptr<SimpleProfile> m_phone;
  std::shared_ptr<SimpleProfile> m_desktop;
  std::shared_ptr<GIcon> m_icon;
};

/***
****
***/

RotationLockIndicator::RotationLockIndicator():
    impl(new Impl())
{
}

RotationLockIndicator::~RotationLockIndicator()
{
}

std::vector<std::shared_ptr<Profile>>
RotationLockIndicator::profiles() const
{
  return impl->profiles();
}

GSimpleActionGroup*
RotationLockIndicator::action_group() const
{
  return impl->action_group();
}

const char*
RotationLockIndicator::name() const
{
  return "rotation_lock";
}

/***
****
***/

