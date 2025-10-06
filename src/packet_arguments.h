#pragma once

enum handshake_arguments
{
	VERSION,
	ADDRESS,
	PORT,
	INTENT
};

enum client_info
{
	LOCALE,
	VIEW_DISTANCE,
	CHAT_MODE,
	CHAT_COLORS,
	SKIN_PARTS,
	MAIN_HAND,
	TEXT_FILTERING,
	ALLOW_SERVER_LISTINGS,
	PARTICLES
};


enum update_position_and_rotation
{
	X,
	Y,
	Z,
	YAW,
	PITCH,
	FLAG,
};

enum set_creative_inventory_slot
{
	SLOT_ID,
	ITEM_COUNT,
	ITEM_ID
};

enum play_actions
{
	STATUS,
	LOCATION,
	FACE,
	SEQUENCE
};