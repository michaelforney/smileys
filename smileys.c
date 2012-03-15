/* smileys: smileys.c
 *
 * Copyright (c) 2012 Michael Forney <mforney@mforney.org>
 *
 * This file is a part of velox.
 *
 * velox is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License version 2, as published by the Free
 * Software Foundation.
 *
 * velox is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with velox.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <yaml.h>
#include <xcb/xcb_keysyms.h>

#include <velox/velox.h>
#include <velox/module.h>
#include <velox/vector.h>
#include <velox/keyboard_mapping.h>
#include <velox/hook.h>
#include <velox/debug.h>

struct key_press
{
    xcb_keycode_t keycode;
    uint16_t modifiers;
};

struct smiley
{
    char name[32];
    uint8_t length;

    char sequence[32];
    struct key_press key_presses[32];
};

DEFINE_VECTOR(smiley_vector, struct smiley);

const char name[] = "smileys";
struct smiley_vector smileys;

static void update_key_presses();
static void send_smiley();

static void __attribute__((constructor)) initialize_smileys()
{
    vector_initialize(&smileys, 32);
}

static void __attribute__((destructor)) free_smileys()
{
    vector_free(&smileys);
}

void configure(yaml_document_t * document)
{
    yaml_node_t * map;
    yaml_node_pair_t * pair;
    yaml_node_t * key, * value;

    struct smiley * smiley;

    printf("Smileys: Loading configuration...");

    map = document->nodes.start;
    assert(map->type == YAML_MAPPING_NODE);

    for (pair = map->data.mapping.pairs.start;
        pair < map->data.mapping.pairs.top;
        ++pair)
    {
        key = yaml_document_get_node(document, pair->key);
        value = yaml_document_get_node(document, pair->value);

        assert(key->type == YAML_SCALAR_NODE);
        assert(value->type == YAML_SCALAR_NODE);

        smiley = vector_append_address(&smileys);
        strcpy(smiley->name, (const char *) key->data.scalar.value);
        smiley->length = value->data.scalar.length;
        strcpy(smiley->sequence, (const char *) value->data.scalar.value);
    }

    printf("done\n");

    vector_for_each(&smileys, smiley)
        printf("    %s: %s\n", smiley->name, smiley->sequence);
}

bool setup()
{
    struct smiley * smiley;
    uint8_t index;

    printf("Smileys: Initializing module...");

    vector_for_each_with_index(&smileys, smiley, index)
    {
        add_key_binding(name, smiley->name, &send_smiley, uint8_argument(index));
    }

    add_hook(&update_key_presses, VELOX_HOOK_KEYBOARD_MAPPING_CHANGED);

    printf("done\n");

    return true;
}

void cleanup()
{
    printf("Smileys: Cleaning up module...");

    printf("done\n");
}

static void update_key_presses()
{
    struct smiley * smiley;
    uint8_t index, keysym_index;
    xcb_keycode_t min_keycode, max_keycode;
    uint16_t keycode;
    xcb_keysym_t keysym;

    min_keycode = xcb_get_setup(c)->min_keycode;
    max_keycode = xcb_get_setup(c)->max_keycode;

    vector_for_each(&smileys, smiley)
    {
        for (index = 0; index < smiley->length; ++index)
        {
            keysym = smiley->sequence[index];

            for (keysym_index = 0; keysym_index < 2; ++keysym_index)
            {
                for (keycode = min_keycode; keycode <= max_keycode; ++keycode)
                {
                    if (xcb_key_symbols_get_keysym(keyboard_mapping, keycode, keysym_index)
                        == keysym)
                    {
                        printf("found keysym: %c, keycode: %u, keysym_index: %u\n", keysym, keycode, keysym_index);
                        smiley->key_presses[index] = (struct key_press) {
                            .keycode = keycode,
                            .modifiers = keysym_index == 0 ? 0 : XCB_MOD_MASK_SHIFT
                        };
                    }
                }
            }
        }
    }
}

static void send_smiley(union velox_argument argument)
{
    uint8_t smiley_index = argument.uint8;
    uint8_t index = 0;

    struct smiley * smiley;
    xcb_get_input_focus_cookie_t focus_cookie;
    xcb_get_input_focus_reply_t * focus_reply;
    xcb_key_press_event_t event;

    DEBUG_ENTER

    focus_cookie = xcb_get_input_focus(c);
    focus_reply = xcb_get_input_focus_reply(c, focus_cookie, NULL);

    event.response_type = XCB_KEY_PRESS;
    event.detail = 52;
    event.root = screen->root;
    event.event = focus_reply->focus;
    event.child = XCB_NONE;

    smiley = &smileys.data[smiley_index];

    for (index = 0; index < smiley->length; ++index)
    {
        event.detail = smiley->key_presses[index].keycode;
        event.state = smiley->key_presses[index].modifiers;

        printf("keycode: %u, state: %u\n", event.detail, event.state);

        xcb_send_event(c, false, focus_reply->focus, XCB_EVENT_MASK_KEY_PRESS,
            (char *) &event);
    }

    xcb_flush(c);
}

// vim: fdm=syntax fo=croql et sw=4 sts=4 ts=8

